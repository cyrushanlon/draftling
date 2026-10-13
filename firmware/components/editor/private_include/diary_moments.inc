/* A day stays one portable Markdown file. Timeline offsets are read-only;
 * changes always go through the shared editor and its recovery/atomic save. */
static int s_day_page=0;
static std::string s_day_page_path;
static std::vector<diary_section_t> s_day_sections;
static size_t s_day_total=0;
static int s_check_mood=-1,s_check_energy=-1;
static bool s_check_inserted=false;
static std::string s_check_stamp;
static lv_obj_t *s_check_choices[7]={}, *s_check_summary=NULL, *s_check_save=NULL;

static std::string diary_selected_stamp(void)
{
    struct tm now;
    if(!diary_now(&now) || now.tm_year+1900!=s_diary_year ||
       now.tm_mon+1!=s_diary_month || now.tm_mday!=s_diary_day) return "";
    char stamp[8]; snprintf(stamp,sizeof(stamp),"%02d:%02d",now.tm_hour,now.tm_min);
    return stamp;
}
static bool diary_prepare_selected(void)
{
    if(!diary_storage_ok() || !diary_save_before_leave()) return false;
    editor_cancel_pending_moment();
    char path[300]; diary_path(path,sizeof(path));
    struct stat st; bool exists=stat(path,&st)==0;
    if(!exists && errno!=ENOENT) {
        show_msg_popup("Cannot read entry","Check the SD card. Your writing has not been changed.",false); return false;
    }
    if(!open_into_pane(s_focus,exists?path:NULL)) {
        show_msg_popup("Cannot open entry","The entry may exceed the document size limit.",false); return false;
    }
    editor_clear_selection(); editor_move_doc_end(); return true;
}
static void diary_moment_begin(void)
{
    std::string stamp=diary_selected_stamp();
    if(!diary_prepare_selected()) return;
    char path[300]; diary_path(path,sizeof(path));
    std::string prefix;
    if(!editor_get_file_path()) {
        char date[40]; snprintf(date,sizeof(date),"# %04d-%02d-%02d",s_diary_year,s_diary_month,s_diary_day);
        prefix=date;
    }
    prefix+="\n\n## "+(stamp.empty()?std::string():stamp+" - ")+"Moment\n\n";
    if(!editor_prepare_moment(path,prefix.c_str())) {
        show_msg_popup("Cannot start moment","The entry could not be prepared. Nothing has been added.",false); return;
    }
    editor_ui_show_editor();
    editor_ui_set_status("New moment: start typing. Edit its heading for an optional title.");
}
static void diary_timeline_event(lv_event_t *e)
{
    if(diary_modal()) return;
    standby_reset_timer(); int id=(int)(intptr_t)lv_event_get_user_data(e);
    if(id<0) { s_day_page=std::max(0,s_day_page+(id==-1?-1:1)); diary_timeline_show(); return; }
    if(id>=(int)s_day_sections.size() || !diary_storage_ok() || !diary_save_before_leave()) return;
    char path[300]; diary_path(path,sizeof(path));
    s_read_path=path; s_read_offset=s_day_sections[id].start;
    diary_read_small(path,s_read_original);
    s_read_start=s_read_offset; s_read_end=s_day_sections[id].end;
    s_read_is_moment=true; s_read_back.clear(); diary_reader_show();
}
static void diary_timeline_show(void)
{
    if(!diary_storage_ok() || !diary_save_before_leave()) return;
    editor_cancel_pending_moment();
    char path[300]; diary_path(path,sizeof(path));
    if(s_day_page_path!=path) { s_day_page=0; s_day_page_path=path; }
    const int rows=std::max(1,(SCR_H-182)/76);
    s_day_sections.clear(); s_day_total=0;
    FILE *f=fopen(path,"rb"); bool exists=f!=NULL;
    if(f) {
        bool ok=diary_timeline(f,s_day_page*rows,rows,s_day_sections,s_day_total);
        if(ok && s_day_page && s_day_sections.empty()) {
            s_day_page=std::max(0,((int)s_day_total-1)/rows); rewind(f);
            ok=diary_timeline(f,s_day_page*rows,rows,s_day_sections,s_day_total);
        }
        fclose(f);
        if(!ok) { show_msg_popup("Cannot read day","Check the SD card and try again.",false); return; }
    } else if(errno!=ENOENT) { show_msg_popup("Cannot read day","Check the SD card and try again.",false); return; }
    s_diary_mode=1;
    char date[80]; snprintf(date,sizeof(date),"%d %s %d",s_diary_day,DIARY_MONTHS[s_diary_month-1],s_diary_year);
    diary_screen_prepare(date);
    int w=(SCR_W-36)/2;
    diary_button(s_diary_screen,diary_selected_stamp().empty()?"Add to this day":"Add a moment",12,54,w,52,diary_calendar_event,-11);
    diary_button(s_diary_screen,"Check in",24+w,54,w,52,diary_calendar_event,-12);
    if(!s_day_total) diary_label(s_diary_screen,"What would you like to remember?\nWrite a moment or check in with a tap.\nNothing is saved until you add something.",20,132,SCR_W-40,FONT_11);
    for(size_t i=0;i<s_day_sections.size();++i) {
        std::string caption=s_day_sections[i].title+"\n"+s_day_sections[i].excerpt;
        auto b=diary_button(s_diary_screen,caption.c_str(),12,116+i*76,SCR_W-24,68,diary_timeline_event,(int)i);
        auto l=lv_obj_get_child(b,0); lv_obj_set_height(l,52); lv_label_set_long_mode(l,LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_LEFT,0);
    }
    int third=(SCR_W-48)/3;
    auto prev=diary_button(s_diary_screen,"Earlier",12,SCR_H-66,third,48,diary_timeline_event,-1);
    diary_button(s_diary_screen,exists?"Read day":"Prompts",24+third,SCR_H-66,third,48,diary_calendar_event,exists?-8:-10);
    auto next=diary_button(s_diary_screen,"Later",36+2*third,SCR_H-66,third,48,diary_timeline_event,-2);
    if(!s_day_page) lv_obj_add_state(prev,LV_STATE_DISABLED);
    if((s_day_page+1)*rows>=(int)s_day_total) lv_obj_add_state(next,LV_STATE_DISABLED);
    diary_screen_load();
}
static void diary_checkin_show(void);
static void diary_checkin_update(void)
{
    for(int i=0;i<7;++i) {
        bool selected=i<4?s_check_mood==i:s_check_energy==i-4;
        lv_obj_set_style_border_width(s_check_choices[i],selected?4:2,0);
        if(s_check_inserted) lv_obj_add_state(s_check_choices[i],LV_STATE_DISABLED);
    }
    char text[150]; snprintf(text,sizeof(text),"Feeling: %s\nEnergy: %s",
        s_check_mood<0?"not selected":DIARY_MOODS[s_check_mood],
        s_check_energy<0?"not selected":DIARY_ENERGIES[s_check_energy]);
    lv_label_set_text(s_check_summary,text);
    if(s_check_mood<0 && s_check_energy<0) lv_obj_add_state(s_check_save,LV_STATE_DISABLED);
    else lv_obj_remove_state(s_check_save,LV_STATE_DISABLED);
}
static void diary_checkin_event(lv_event_t *e)
{
    if(diary_modal()) return;
    standby_reset_timer(); int id=(int)(intptr_t)lv_event_get_user_data(e);
    if(id==8) {
        if(s_check_inserted) { show_msg_popup("Check-in not saved","Tap Save to retry. Your check-in is still in memory.",false); return; }
        diary_day_show(); return;
    }
    if(id<7) {
        if(s_check_inserted) return;
        if(id<4) s_check_mood=s_check_mood==id?-1:id;
        else s_check_energy=s_check_energy==id-4?-1:id-4;
        diary_checkin_update(); return;
    }
    if(s_check_mood<0 && s_check_energy<0) return;
    if(!diary_storage_ok()) return;
    if(!s_check_inserted) {
        if(!diary_prepare_selected()) return;
        char path[300]; diary_path(path,sizeof(path));
        std::string text=diary_moment_markdown(s_check_stamp,"","",s_check_mood,s_check_energy);
        if(!editor_get_file_path()) {
            char prefix[40]; snprintf(prefix,sizeof(prefix),"# %04d-%02d-%02d",s_diary_year,s_diary_month,s_diary_day);
            if(!editor_prepare_moment(path,prefix)) return;
        }
        if(!editor_insert_text(text.c_str(),text.size())) {
            editor_cancel_pending_moment();
            show_msg_popup("Entry is full","Your check-in has not been added. Previous writing is unchanged.",false); return;
        }
        s_check_inserted=true; diary_checkin_update();
    }
    if(editor_save_file()!=ESP_OK) {
        show_msg_popup("Check-in not saved","Check the SD card and tap Save to retry. Your check-in is still in memory; retrying will not add it twice.",false); return;
    }
    s_check_inserted=false;
    /* Show the new item on the final page, in the same timeline as writing. */
    s_day_page=1000000; diary_day_show();
    editor_ui_set_status("Check-in saved on device");
}
static void diary_checkin_show(void)
{
    s_diary_mode=12; diary_screen_prepare("A quick check-in");
    char day[100]; snprintf(day,sizeof(day),"%d %s %d%s%s",s_diary_day,DIARY_MONTHS[s_diary_month-1],s_diary_year,
        s_check_stamp.empty()?" - no time":" - ",s_check_stamp.c_str());
    diary_label(s_diary_screen,day,16,52,SCR_W-32,FONT_11);
    diary_label(s_diary_screen,"How are you feeling?",16,90,SCR_W-32,FONT_11);
    int cols=SCR_W<600?2:4,w=(SCR_W-24-(cols-1)*8)/cols;
    for(int i=0;i<4;++i) s_check_choices[i]=diary_button(s_diary_screen,DIARY_MOODS[i],12+(i%cols)*(w+8),120+(i/cols)*56,w,48,diary_checkin_event,i);
    int y=cols==2?238:182;
    diary_label(s_diary_screen,"How is your energy?",16,y,SCR_W-32,FONT_11);
    w=(SCR_W-40)/3;
    const char *energy_labels[]={"Low energy","Steady","High energy"};
    for(int i=0;i<3;++i) s_check_choices[4+i]=diary_button(s_diary_screen,energy_labels[i],12+i*(w+8),y+30,w,48,diary_checkin_event,i+4);
    s_check_summary=diary_label(s_diary_screen,"",16,y+90,SCR_W-32,FONT_11);
    diary_label(s_diary_screen,"Optional. Tap again to clear.",16,SCR_H-94,SCR_W-32,FONT_11);
    int half=(SCR_W-36)/2;
    diary_button(s_diary_screen,"Skip",12,SCR_H-62,half,50,diary_checkin_event,8);
    s_check_save=diary_button(s_diary_screen,"Save",24+half,SCR_H-62,half,50,diary_checkin_event,7);
    diary_checkin_update(); diary_screen_load();
}
static void diary_checkin_begin(void)
{
    if(!diary_storage_ok() || !diary_save_before_leave()) return;
    editor_cancel_pending_moment();
    s_check_mood=s_check_energy=-1; s_check_inserted=false;
    s_check_stamp=diary_selected_stamp(); diary_checkin_show();
}
