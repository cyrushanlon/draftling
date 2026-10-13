/* Included by editor_ui.cpp so diary navigation shares the editor's guards. */
static int s_diary_year=2026, s_diary_month=1, s_diary_day=1;
static int s_clock_hour=12, s_clock_minute=0, s_clock_offset=0;
static bool s_zone_loaded=false;
static bool s_clock_editing=false;
static int s_clock_saved_offset=0;
static int s_clock_saved_zone=0;
static int s_diary_mode=0; /* 0 calendar, 1 day, 2 clock, 3 sync */
static lv_obj_t *s_diary_sync_label=NULL;
static const char *const DIARY_MONTHS[]={"January","February","March","April","May","June","July","August","September","October","November","December"};

static void diary_zone_apply(void)
{
    diary_preferences_load();
    if (!s_zone_loaded) {
        nvs_handle_t h;
        int32_t offset=0;
        if(nvs_open("diary",NVS_READONLY,&h)==ESP_OK) {
            nvs_get_i32(h,"utc_minutes",&offset); nvs_close(h);
        }
        s_clock_offset=std::max(-720,std::min(840,(int)offset)); s_zone_loaded=true;
    }
    char tz[32];
    const int n=std::abs(s_clock_offset);
    snprintf(tz,sizeof(tz),"LOC%s%d:%02d",s_clock_offset>=0?"-":"",n/60,n%60);
    setenv("TZ",s_diary_zone?DIARY_ZONE_RULES[s_diary_zone]:tz,1); tzset();
}
static bool diary_now(struct tm *out)
{
    diary_zone_apply();
    time_t now=time(NULL); localtime_r(&now,out);
    return out->tm_year>=120 && out->tm_year<=199;
}
static void diary_home_status(void)
{
    if(!s_home_date) return;
    struct tm now; char date[64], status[110];
    if(diary_now(&now)) strftime(date,sizeof(date),"%a %d %b %H:%M",&now);
    else snprintf(date,sizeof(date),"Set date & time");
    const char *battery="";
#if defined(CONFIG_DRAFTLING_HAS_BATTERY)
    if(s_lbl_dev_batt) battery=lv_label_get_text(s_lbl_dev_batt);
#endif
    snprintf(status,sizeof(status),"%s  %s%s",date,battery,wifi_manager_is_connected()?" WiFi":"");
    if(strcmp(lv_label_get_text(s_home_date),status)) lv_label_set_text(s_home_date,status);
}
static bool diary_storage_ok(void)
{
#if defined(CONFIG_DRAFTLING_HAS_USB_MSC)
    if(usb_msc_get_mode()!=USB_MSC_MODE_OFF) {
        show_msg_popup("SD card in use","Turn off USB file access in Advanced before opening your diary.",false); return false;
    }
#endif
    if(!sd_card_is_ready()) { show_msg_popup("SD card needed","Insert your SD card and restart. Your diary is stored on the card.",false); return false; }
    if(git_sync_get_state()==GIT_SYNC_IN_PROGRESS) { show_msg_popup("Sync in progress","Please wait for sync to finish before changing documents.",false); return false; }
    return true;
}
static bool diary_save_before_leave(void)
{
    if(!editor_is_modified()) return true;
    if(!diary_storage_ok()) return false;
    if(!editor_get_file_path()) {
        editor_ui_show_editor(); show_save_prompt(); return false;
    }
    if(editor_save_file()!=ESP_OK) {
        show_msg_popup("Not saved","Your writing is still open. Check your SD card and try Save again.",false); return false;
    }
    return true;
}
static void diary_path(char *path,size_t size)
{
    snprintf(path,size,"%s/%04d-%02d-%02d.md",sd_card_get_mount_point(),s_diary_year,s_diary_month,s_diary_day);
}
static void diary_open_selected(void)
{
    if(!diary_storage_ok() || !diary_save_before_leave()) return;
    char path[300]; diary_path(path,sizeof(path));
    struct stat st;
    if(stat(path,&st)!=0) {
        if(errno!=ENOENT) { show_msg_popup("Cannot read entry","The SD card could not be read. Your existing files have not been changed.",false); return; }
        char heading[64];
        snprintf(heading,sizeof(heading),"# %04d-%02d-%02d\n\n",s_diary_year,s_diary_month,s_diary_day);
        if(sd_card_write_file(path,heading,strlen(heading))!=ESP_OK) {
            show_msg_popup("Could not create entry","Check the SD card and try again.",false); return;
        }
    }
    if(!open_into_pane(s_focus,path)) { show_msg_popup("Cannot open entry","The file could not be opened. It may exceed the document size limit.",false); return; }
    editor_move_doc_end();
    editor_ui_show_editor();
    editor_ui_set_status("Saved on device. Changes save after a short pause.");
}
static lv_obj_t *diary_label(lv_obj_t *parent,const char *text,int x,int y,int w,const lv_font_t *font)
{
    lv_obj_t *label=lv_label_create(parent);
    lv_label_set_text(label,text); lv_obj_set_pos(label,x,y); lv_obj_set_width(label,w);
    lv_obj_set_style_text_font(label,font,0); lv_obj_set_style_text_color(label,theme_fg(),0);
    lv_label_set_long_mode(label,LV_LABEL_LONG_WRAP);
    return label;
}
static lv_obj_t *diary_button(lv_obj_t *parent,const char *text,int x,int y,int w,int h,lv_event_cb_t cb,int id)
{
    lv_obj_t *b=lv_button_create(parent); lv_obj_remove_style_all(b);
    lv_obj_set_pos(b,x,y); lv_obj_set_size(b,w,h);
    lv_obj_set_style_bg_color(b,theme_bg(),0); lv_obj_set_style_bg_opa(b,LV_OPA_COVER,0);
    lv_obj_set_style_border_color(b,theme_fg(),0); lv_obj_set_style_border_width(b,2,0);
    lv_obj_set_style_radius(b,8,0); lv_obj_set_style_pad_all(b,4,0);
    lv_obj_set_style_border_width(b,3,LV_STATE_PRESSED);
    lv_obj_remove_flag(b,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *l=diary_label(b,text,0,0,w-12,FONT_11);
    lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_CENTER,0); lv_obj_center(l);
    lv_obj_add_event_cb(b,cb,LV_EVENT_CLICKED,(void *)(intptr_t)id);
    return b;
}
/* Reserve the same content box on every screen. Child hit testing inherits
 * this padding; the editor's absolute touch/typing clips account for it too. */
static void diary_header_reserve(lv_obj_t *screen)
{
#if defined(CONFIG_DRAFTLING_TOUCHSCREEN)
    lv_obj_set_style_pad_all(screen,0,0);
    lv_obj_set_style_border_width(screen,0,0);
    lv_obj_set_style_pad_top(screen,DIARY_HEADER_H,0);
    lv_obj_set_style_pad_bottom(screen,NAV_H,0);
    lv_obj_remove_flag(screen,LV_OBJ_FLAG_SCROLLABLE);
#endif
}
static void diary_header_build(void)
{
#if defined(CONFIG_DRAFTLING_TOUCHSCREEN)
    s_diary_header=lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_diary_header);
    lv_obj_set_pos(s_diary_header,0,0);lv_obj_set_size(s_diary_header,SCR_W,DIARY_HEADER_H);
    lv_obj_set_style_bg_color(s_diary_header,theme_bg(),0);
    lv_obj_set_style_bg_opa(s_diary_header,LV_OPA_COVER,0);
    lv_obj_remove_flag(s_diary_header,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_diary_header,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_diary_header,diary_date_touch,LV_EVENT_CLICKED,NULL);
    s_home_date=diary_label(s_diary_header,"",8,3,SCR_W-16,FONT_11);
    lv_label_set_long_mode(s_home_date,LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(s_home_date,LV_TEXT_ALIGN_RIGHT,0);
    lv_obj_remove_flag(s_home_date,LV_OBJ_FLAG_CLICKABLE);
    diary_home_status();
#endif
}
static void diary_screen_prepare(const char *title)
{
    editor_cancel_pending_moment();
    if(s_clock_editing && strcmp(title,"Date & time")) {
        s_clock_offset=s_clock_saved_offset; s_diary_zone=s_clock_saved_zone; s_clock_editing=false; diary_zone_apply();
    }
    s_home_open=true; s_menu_open=false; s_settings_open=false;
    s_help_open=false;
    close_inpane_browser();
    s_diary_sync_label=NULL;
    if(!s_diary_screen) {
        s_diary_screen=lv_obj_create(NULL);
        lv_obj_remove_style_all(s_diary_screen);
        lv_obj_set_style_bg_color(s_diary_screen,theme_bg(),0);
        lv_obj_set_style_bg_opa(s_diary_screen,LV_OPA_COVER,0);
        lv_obj_remove_flag(s_diary_screen,LV_OBJ_FLAG_SCROLLABLE);
    }
    /* Shared overlays are reparented onto the current screen. Keep them
     * alive when rebuilding a month or changing diary pages. */
    lv_obj_t *overlays[]={s_msg_panel,s_passkey_panel,s_save_panel,s_exit_panel,
                         s_del_panel,s_search_panel,s_wifi_pw_panel,s_dbx_panel};
    for(auto panel:overlays)
        if(panel && lv_obj_get_parent(panel)==s_diary_screen) lv_obj_set_parent(panel,s_scr);
    lv_obj_clean(s_diary_screen);
    diary_header_reserve(s_diary_screen);
    diary_label(s_diary_screen,title,12,8,SCR_W-24,FONT_18);
}
static void diary_screen_load(void)
{
    /* Repaint through the existing driver; no white pre-clear frame. */
    lv_obj_invalidate(s_diary_screen); lv_scr_load(s_diary_screen);
    if(s_diary_header) lv_obj_invalidate(s_diary_header);
    diary_nav_refresh();
}
static void diary_day_show(void);
static void diary_month_picker(void);
static void diary_calendar_event(lv_event_t *e)
{
    if(diary_modal()) return;
    const int id=(int)(intptr_t)lv_event_get_user_data(e);
    standby_reset_timer();
    if(id==-1 || id==-2) {
        s_diary_month += id==-1 ? -1 : 1;
        if(s_diary_month<1) { s_diary_month=12; --s_diary_year; }
        if(s_diary_month>12) { s_diary_month=1; ++s_diary_year; }
        s_diary_year=std::max(2020,std::min(2099,s_diary_year));
        diary_calendar_show();
    } else if(id==-3) { diary_today_open(); }
    else if(id==-4) diary_clock_show();
    else if(id==-5) diary_open_selected();
    else if(id==-6) diary_calendar_show();
    else if(id==-7) diary_month_picker();
    else if(id==-8) { char path[300]; diary_path(path,sizeof(path)); diary_read_path(path); }
    else if(id==-9) diary_journal_show();
    else if(id==-10) { s_prompt_for_day=true; diary_prompts_show(); }
    else if(id==-11) diary_moment_begin();
    else if(id==-12) diary_checkin_begin();
    else { s_diary_day=id; diary_day_show(); }
}
static void diary_calendar_show(void)
{
    if(!diary_storage_ok()) return;
    s_diary_mode=0;
    diary_screen_prepare("Diary calendar");
    char title[64]; snprintf(title,sizeof(title),"%s %d",DIARY_MONTHS[s_diary_month-1],s_diary_year);
    diary_button(s_diary_screen,"<",12,52,56,44,diary_calendar_event,-1);
    diary_button(s_diary_screen,">",SCR_W-68,52,56,44,diary_calendar_event,-2);
    auto heading=diary_label(s_diary_screen,title,72,60,SCR_W-144,FONT_14);
    lv_obj_set_style_text_align(heading,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_add_flag(heading,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(heading,4);
    lv_obj_add_event_cb(heading,diary_calendar_event,LV_EVENT_CLICKED,(void *)(intptr_t)-7);
    static const char *const weekdays[]={"Mon","Tue","Wed","Thu","Fri","Sat","Sun"};
    static const char *const weekdays_short[]={"Mo","Tu","We","Th","Fr","Sa","Su"};
    const int gap=5, cell=(SCR_W-24-6*gap)/7, top=128, h=(SCR_H-top-32)/6;
    for(int i=0;i<7;++i) {
        auto label=diary_label(s_diary_screen,SCR_W<600?weekdays_short[i]:weekdays[i],12+i*(cell+gap),102,cell,FONT_11);
        lv_obj_set_style_text_align(label,LV_TEXT_ALIGN_CENTER,0);
    }
    const int first=diary_weekday(s_diary_year,s_diary_month,1);
    struct tm today; const bool valid=diary_now(&today);
    for(int day=1;day<=diary_month_days(s_diary_year,s_diary_month);++day) {
        const int slot=first+day-1;
        char path[300],caption[24];
        snprintf(path,sizeof(path),"%s/%04d-%02d-%02d.md",sd_card_get_mount_point(),s_diary_year,s_diary_month,day);
        const bool exists=sd_card_file_exists(path);
        snprintf(caption,sizeof(caption),"%d%s",day,exists?" *":"");
        auto b=diary_button(s_diary_screen,caption,12+(slot%7)*(cell+gap),top+(slot/7)*h,cell,h-4,diary_calendar_event,day);
        if(valid && today.tm_year+1900==s_diary_year && today.tm_mon+1==s_diary_month && today.tm_mday==day)
            lv_obj_set_style_border_width(b,4,0);
    }
    diary_label(s_diary_screen,"* Saved entry",12,SCR_H-28,SCR_W/2-24,FONT_11);
    diary_button(s_diary_screen,"Journal list",SCR_W/2,SCR_H-34,SCR_W/2-12,32,diary_calendar_event,-9);
    diary_screen_load();
}
static void diary_month_event(lv_event_t *e)
{
    if(diary_modal()) return;
    int id=(int)(intptr_t)lv_event_get_user_data(e);
    standby_reset_timer();
    if(id<0) { s_diary_year=std::max(2020,std::min(2099,s_diary_year+(id==-1?-1:1))); diary_month_picker(); }
    else { s_diary_month=id; diary_calendar_show(); }
}
static void diary_month_picker(void)
{
    s_diary_mode=5; diary_screen_prepare("Choose a month");
    char year[12]; snprintf(year,sizeof(year),"%d",s_diary_year);
    diary_button(s_diary_screen,"Previous year",12,54,(SCR_W-120)/2,44,diary_month_event,-1);
    diary_label(s_diary_screen,year,SCR_W/2-28,64,68,FONT_11);
    diary_button(s_diary_screen,"Next year",SCR_W/2+48,54,SCR_W/2-60,44,diary_month_event,-2);
    int w=(SCR_W-40)/3,h=(SCR_H-116)/4;
    for(int m=1;m<=12;++m) diary_button(s_diary_screen,DIARY_MONTHS[m-1],10+((m-1)%3)*(w+5),108+((m-1)/3)*h,w,h-6,diary_month_event,m);
    diary_screen_load();
}
static void diary_day_show(void)
{
    diary_timeline_show();
}
static void diary_today_open(void)
{
    if(!diary_storage_ok()) return;
    struct tm today;
    if(!diary_now(&today)) { diary_clock_show(); return; }
    s_diary_year=today.tm_year+1900; s_diary_month=today.tm_mon+1; s_diary_day=today.tm_mday;
    diary_day_show();
}
static void diary_clock_render(void);
static void diary_clock_event(lv_event_t *e)
{
    if(diary_modal()) return;
    const int id=(int)(intptr_t)lv_event_get_user_data(e);
    standby_reset_timer();
    if(id==98) { s_clock_offset=s_clock_saved_offset;s_diary_zone=s_clock_saved_zone;s_clock_editing=false;diary_zone_apply();diary_zone_show();return; }
    if(id==99) {
        struct tm t={}; t.tm_year=s_diary_year-1900; t.tm_mon=s_diary_month-1; t.tm_mday=s_diary_day;
        t.tm_hour=s_clock_hour; t.tm_min=s_clock_minute; t.tm_isdst=-1;
        diary_zone_apply(); time_t utc=mktime(&t);
        bool persisted=s_clock_writer && s_clock_writer(utc);
        struct timeval tv={utc,0}; settimeofday(&tv,NULL);
        s_clock_editing=false;
        diary_preferences_save();
        nvs_handle_t h;
        if(nvs_open("diary",NVS_READWRITE,&h)==ESP_OK) { nvs_set_i32(h,"utc_minutes",s_clock_offset); nvs_commit(h); nvs_close(h); }
        show_home();
        if(!persisted) show_msg_popup("Clock set for this session","The hardware clock could not be updated. Check the date after restarting.",false);
        return;
    }
    int field=std::abs(id),delta=id>0?1:-1;
    if(field==1) s_diary_year=std::max(2020,std::min(2099,s_diary_year+delta));
    if(field==2) s_diary_month=(s_diary_month-1+delta+12)%12+1;
    if(field==3) s_diary_day=std::max(1,std::min(diary_month_days(s_diary_year,s_diary_month),s_diary_day+delta));
    if(field==4) s_clock_hour=(s_clock_hour+delta+24)%24;
    if(field==5) s_clock_minute=(s_clock_minute+delta+60)%60;
    if(field==6) { s_diary_zone=0; s_clock_offset=std::max(-720,std::min(840,s_clock_offset+delta*15)); }
    s_diary_day=std::min(s_diary_day,diary_month_days(s_diary_year,s_diary_month));
    diary_clock_render();
}
static void diary_clock_render(void)
{
    s_diary_mode=2; diary_screen_prepare("Date & time");
    const char *names[]={"Year","Month","Day","Hour","Minute","Manual UTC"};
    int values[]={s_diary_year,s_diary_month,s_diary_day,s_clock_hour,s_clock_minute,s_clock_offset};
    const int row=(SCR_H-112)/6;
    for(int i=0;i<6;++i) {
        char text[64];
        if(i==5) snprintf(text,sizeof(text),"%s  %c%02d:%02d",names[i],values[i]>=0?'+':'-',std::abs(values[i])/60,std::abs(values[i])%60);
        else snprintf(text,sizeof(text),"%s  %02d",names[i],values[i]);
        diary_label(s_diary_screen,text,16,60+i*row,SCR_W-156,FONT_11);
        diary_button(s_diary_screen,"-",SCR_W-132,52+i*row,54,row-3,diary_clock_event,-(i+1));
        diary_button(s_diary_screen,"+",SCR_W-70,52+i*row,54,row-3,diary_clock_event,i+1);
    }
    diary_button(s_diary_screen,"Save clock",12,SCR_H-54,(SCR_W-36)/2,44,diary_clock_event,99);
    diary_button(s_diary_screen,"Timezone / NTP",24+(SCR_W-36)/2,SCR_H-54,(SCR_W-36)/2,44,diary_clock_event,98);
    diary_screen_load();
}
static void diary_clock_show(void)
{
    struct tm now;
    if(diary_now(&now)) { s_diary_year=now.tm_year+1900; s_diary_month=now.tm_mon+1; s_diary_day=now.tm_mday; s_clock_hour=now.tm_hour; s_clock_minute=now.tm_min; }
    s_clock_saved_offset=s_clock_offset; s_clock_editing=true;
    s_clock_saved_zone=s_diary_zone;
    diary_clock_render();
}
static void diary_sync_event(lv_event_t *e)
{
    if(diary_modal()) return;
    const int id=(int)(intptr_t)lv_event_get_user_data(e);
    if(id==3) { diary_status_show(); return; }
    if(id==1) { show_advanced_menu(); menu_activate_item(MENU_IDX_WIFI_NEW); }
    else if(id==2) { show_advanced_menu(); menu_activate_item(MENU_IDX_DRAFTBOX); }
    else {
        if(!diary_storage_ok() || !diary_save_before_leave()) return;
        bool saved=true;
        editor_doc_foreach([](editor_doc_t *doc,void *p) {
            if(editor_doc_is_modified(doc) && (!editor_doc_get_path(doc) || editor_doc_save(doc)!=ESP_OK)) *(bool *)p=false;
        },&saved);
        if(!saved) { lv_label_set_text(s_diary_sync_label,"Save the writing in every open pane before syncing. At least one document could not be saved."); return; }
        if(!git_sync_is_configured()) { lv_label_set_text(s_diary_sync_label,"Set up Draftbox below, or put git.cfg on your SD card. Your diary stays local until you choose to sync."); return; }
        if(!wifi_manager_is_connected()) { wifi_connect_async(); lv_label_set_text(s_diary_sync_label,"Connecting to saved WiFi. Tap Sync now once connected, or choose WiFi setup."); return; }
        if(git_sync_start(GIT_SYNC_BOTH)!=ESP_OK) lv_label_set_text(s_diary_sync_label,git_sync_get_last_error());
    }
}
static void diary_sync(void)
{
    s_diary_mode=3; diary_screen_prepare("Sync & backup");
    s_diary_sync_label=diary_label(s_diary_screen,s_diary_notice,20,70,SCR_W-40,FONT_11);
    lv_obj_set_height(s_diary_sync_label,60);
    lv_label_set_long_mode(s_diary_sync_label,LV_LABEL_LONG_DOT);
    diary_button(s_diary_screen,"Save / backup status",20,136,SCR_W-40,44,diary_sync_event,3);
    char destination[480]; git_sync_describe_destination(destination,sizeof(destination));
    auto dest=diary_label(s_diary_screen,destination,20,184,SCR_W-40,FONT_11);
    lv_obj_set_height(dest,SCR_H-280); lv_label_set_long_mode(dest,LV_LABEL_LONG_DOT);
    const int w=(SCR_W-48)/3;
    diary_button(s_diary_screen,"Sync now",12,SCR_H-88,w,60,diary_sync_event,0);
    diary_button(s_diary_screen,"WiFi setup",24+w,SCR_H-88,w,60,diary_sync_event,1);
    diary_button(s_diary_screen,"Draftbox setup",36+2*w,SCR_H-88,w,60,diary_sync_event,2);
    diary_screen_load();
}
