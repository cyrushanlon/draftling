/* Bounded, read-only pages: viewing never opens or mutates an editor buffer. */
static std::string s_read_path;
static std::string s_tools_path;
static long s_read_offset=0, s_read_next=0;
static long s_read_start=0, s_read_end=0;
static bool s_read_is_moment=false;
static std::string s_read_original;
static std::vector<long> s_read_back;
static bool s_read_large=false;
static int s_library_kind=0, s_library_page=0, s_journal_page=0;
static std::vector<std::string> s_library_paths;
static const char *const DIARY_PROMPTS[]={
    "What felt good today?", "What small moment would you like to keep?",
    "Who helped you feel understood?", "What did you notice on a walk?",
    "What are you learning about yourself?", "What can wait until tomorrow?",
    "What would you tell a friend having your day?", "What made you smile?",
    "What are you grateful for, however small?", "What do you need right now?",
    "What surprised you today?", "What would a kinder tomorrow look like?"
};
static int s_prompt=0;

static bool diary_require_keyboard(void)
{
    bool connected=ble_keyboard_is_connected();
#if defined(CONFIG_DRAFTLING_HAS_USB_HOST)
    connected=connected || usb_kbd_is_connected();
#endif
#if defined(CONFIG_DRAFTLING_HAS_TAB5_KBD)
    connected=connected || tab5_kbd_is_present();
#endif
    if(connected) return true;
    s_diary_waiting_write=true;
    ble_keyboard_enable();
    lv_label_set_text(s_ble_prompt_lbl,"Connect a keyboard to write.\nYou can go Home and keep browsing.");
    lv_scr_load(s_scr_ble_prompt); diary_nav_refresh();
    return false;
}
static std::vector<std::string> diary_files(const std::string &dir,bool dated)
{
    std::vector<std::string> files;
    DIR *d=opendir(dir.c_str()); if(!d) return files;
    while(auto ent=readdir(d)) {
        if(ent->d_name[0]=='.') continue;
        int y,m,day;
        if(dated ? !diary_filename(ent->d_name,&y,&m,&day) : !editor_path_format(ent->d_name,nullptr)) continue;
        std::string path=dir+"/"+ent->d_name; struct stat st;
        if(stat(path.c_str(),&st)==0 && S_ISREG(st.st_mode)) files.push_back(path);
    }
    closedir(d); std::sort(files.rbegin(),files.rend()); return files;
}
static std::string diary_excerpt(const std::string &path)
{
    FILE *f=fopen(path.c_str(),"rb"); if(!f) return "Could not read entry";
    char buf[161]={}; size_t n=fread(buf,1,160,f); bool bad=ferror(f); fclose(f);
    if(bad) return "Could not read entry";
    /* Remove an incomplete UTF-8 tail; labels must never see split glyphs. */
    if(n==160) { while(n && ((unsigned char)buf[n-1]&0xc0)==0x80) --n; if(n && (unsigned char)buf[n-1]>=0xc0) --n; buf[n]=0; }
    std::string s=diary_without_private_markers(buf);
    if(s.rfind("# ",0)==0) { auto p=s.find('\n'); if(p!=std::string::npos) s=s.substr(p+1); }
    for(char &c:s) if(c=='\n' || c=='\r' || c=='\t') c=' ';
    auto first=s.find_first_not_of(' '); return first==std::string::npos?"An empty page":s.substr(first);
}
static void diary_reader_show(void);
static void diary_reader_event(lv_event_t *e)
{
    if(diary_modal() || !diary_storage_ok()) return;
    standby_reset_timer(); int id=(int)(intptr_t)lv_event_get_user_data(e);
    if(id==0) {
        if(!diary_save_before_leave()) return;
        if(s_read_path.find("/.diary/")!=std::string::npos) {
            show_msg_popup("Read-only recovery copy","Use Restore copy in Privacy & recovery. The original snapshot stays unchanged.",false); return;
        }
        if(!open_into_pane(s_focus,s_read_path.c_str())) { show_msg_popup("Cannot edit","The entry could not be opened.",false); return; }
        editor_cancel_pending_moment();
        if(s_read_is_moment) {
            size_t len=0; const char *text=editor_get_text(&len);
            size_t at=std::min((size_t)s_read_end,len);
            /* Keep the trailing newline separating the following section. */
            if(at && text[at-1]=='\n') --at;
            editor_clear_selection(); editor_set_cursor(at);
        }
        editor_ui_show_editor(); return;
    }
    if(id==1 && !s_read_back.empty()) { s_read_offset=s_read_back.back(); s_read_back.pop_back(); }
    if(id==2 && s_read_next>s_read_offset) { s_read_back.push_back(s_read_offset); s_read_offset=s_read_next; }
    if(id==3) s_read_large=!s_read_large;
    if(id==4 || id==5) {
        auto files=diary_files(sd_card_get_mount_point(),true);
        auto it=std::find(files.begin(),files.end(),s_read_path);
        if(it==files.end()) { show_msg_popup("Diary navigation","Previous and next entry work with dated diary files.",false); return; }
        if(id==4) { if(it+1==files.end()) return; ++it; }
        else { if(it==files.begin()) return; --it; }
        diary_read_path(it->c_str()); return;
    }
    diary_reader_show();
}
static void diary_reader_show(void)
{
    diary_preferences_load();
    if(!diary_storage_ok()) return;
    FILE *f=fopen(s_read_path.c_str(),"rb");
    if(!f) { show_msg_popup("Cannot read entry","The file is unavailable. Your writing has not been changed.",false); return; }
    /* Leave the 8 KB LVGL task stack for input and layout callbacks. */
    std::vector<char> chunk(4097,0);
    char *text=chunk.data();
    bool ok=fseek(f,s_read_offset,SEEK_SET)==0;
    size_t wanted=s_read_is_moment?(size_t)std::max(0L,std::min(4096L,s_read_end-s_read_offset)):4096;
    size_t n=ok?fread(text,1,wanted,f):0;
    bool more=s_read_is_moment?s_read_offset+(long)n<s_read_end:n==4096 && fgetc(f)!=EOF;
    ok=ok && !ferror(f) && (!s_read_is_moment || n==wanted); fclose(f);
    if(!ok) { show_msg_popup("Read failed","Check the SD card and try again.",false); return; }
    if(more) { while(n && ((unsigned char)text[n-1]&0xc0)==0x80) --n; if(n && (unsigned char)text[n-1]>=0xc0) --n; text[n]=0; }
    s_read_next=more?s_read_offset+(long)n:0;
    s_diary_mode=6;
    const char *name=strrchr(s_read_path.c_str(),'/');
    diary_screen_prepare(name?name+1:"Reading");
    auto area=lv_obj_create(s_diary_screen); lv_obj_remove_style_all(area);
    lv_obj_set_pos(area,20,106); lv_obj_set_size(area,SCR_W-40,SCR_H-174);
    lv_obj_set_scroll_dir(area,LV_DIR_VER);
    lv_obj_remove_flag(area,(lv_obj_flag_t)(LV_OBJ_FLAG_SCROLL_MOMENTUM | LV_OBJ_FLAG_SCROLL_ELASTIC));
    std::string readable=diary_without_private_markers(n?text:"This entry is empty.");
    auto body=diary_label(area,readable.c_str(),8,4,SCR_W-68,s_read_large?FONT_18:diary_read_font());
    lv_obj_set_style_text_line_space(body,s_read_spacing,0);
    int w=(SCR_W-40)/3;
    diary_button(s_diary_screen,"Older entry",10,54,w,44,diary_reader_event,4);
    diary_button(s_diary_screen,s_read_large?"Text: large":"Text: normal",15+w,54,w,44,diary_reader_event,3);
    diary_button(s_diary_screen,"Newer entry",20+2*w,54,w,44,diary_reader_event,5);
    auto prev=diary_button(s_diary_screen,"Earlier text",10,SCR_H-58,w,48,diary_reader_event,1);
    diary_button(s_diary_screen,s_read_is_moment?(SCR_W<600?"Add words":"Add a few words"):"Write",15+w,SCR_H-58,w,48,diary_reader_event,0);
    auto next=diary_button(s_diary_screen,"More text",20+2*w,SCR_H-58,w,48,diary_reader_event,2);
    if(s_read_back.empty()) { lv_obj_add_state(prev,LV_STATE_DISABLED); lv_label_set_text(lv_obj_get_child(prev,0),"Start of entry"); lv_obj_set_style_border_width(prev,0,0); }
    if(!more) { lv_obj_add_state(next,LV_STATE_DISABLED); lv_label_set_text(lv_obj_get_child(next,0),"End of entry"); lv_obj_set_style_border_width(next,0,0); }
    diary_screen_load();
}
static void diary_read_path(const char *path)
{
    if(!path || !diary_storage_ok() || !diary_save_before_leave()) return;
    s_read_is_moment=false; s_read_start=s_read_end=0;
    s_read_path=path; s_read_offset=0; s_read_back.clear(); diary_reader_show();
}
static void diary_journal_event(lv_event_t *e)
{
    if(diary_modal()) return;
    standby_reset_timer(); int id=(int)(intptr_t)lv_event_get_user_data(e);
    if(id==-3) { diary_hub_show(); return; }
    if(id<0) { s_journal_page=std::max(0,s_journal_page+(id==-1?-1:1)); diary_journal_show(); return; }
    auto files=diary_files(sd_card_get_mount_point(),true);
    if(id<(int)files.size()) diary_read_path(files[id].c_str());
}
static void diary_journal_show(void)
{
    if(!diary_storage_ok()) return;
    s_diary_mode=7; diary_screen_prepare("Journal - newest first");
    auto files=diary_files(sd_card_get_mount_point(),true);
    const int rows=std::max(1,(SCR_H-126)/84);
    s_journal_page=std::min(s_journal_page,std::max(0,((int)files.size()-1)/rows));
    if(files.empty()) diary_label(s_diary_screen,"Your diary starts with one day.\nTap Today to begin.",20,90,SCR_W-40,FONT_14);
    for(int i=0;i<rows && s_journal_page*rows+i<(int)files.size();++i) {
        int idx=s_journal_page*rows+i;
        auto name=files[idx].substr(files[idx].find_last_of('/')+1);
        std::string caption=name.substr(0,10)+"\n"+diary_excerpt(files[idx]);
        auto b=diary_button(s_diary_screen,caption.c_str(),12,58+i*84,SCR_W-24,76,diary_journal_event,idx);
        auto l=lv_obj_get_child(b,0); lv_obj_set_height(l,58); lv_label_set_long_mode(l,LV_LABEL_LONG_DOT);
    }
    int third=(SCR_W-48)/3;
    auto prev=diary_button(s_diary_screen,"Newer",12,SCR_H-58,third,46,diary_journal_event,-1);
    diary_button(s_diary_screen,"Explore",24+third,SCR_H-58,third,46,diary_journal_event,-3);
    auto next=diary_button(s_diary_screen,"Older",36+2*third,SCR_H-58,third,46,diary_journal_event,-2);
    if(s_journal_page==0) lv_obj_add_state(prev,LV_STATE_DISABLED);
    if((s_journal_page+1)*rows>=(int)files.size()) lv_obj_add_state(next,LV_STATE_DISABLED);
    diary_screen_load();
}
static void diary_prompt_event(lv_event_t *e)
{
    if(diary_modal()) return;
    standby_reset_timer(); int id=(int)(intptr_t)lv_event_get_user_data(e);
    if(id==0) { s_prompt=(s_prompt+1)%diary_prompt_count(); diary_prompts_show(); }
    else if(id==1) {
        if(s_prompt_for_day) {
            diary_open_selected();
            if(diary_modal() || lv_scr_act()!=s_scr) return;
        }
        if(!diary_require_keyboard()) return;
        editor_ui_show_editor(); editor_move_doc_end();
        std::string prompt=std::string("\n\n")+diary_prompt_text()+"\n\n";
        if(!editor_insert_text(prompt.c_str(),prompt.size())) show_msg_popup("Note is full","There was no room to add the prompt.",false);
        editor_ui_refresh();
    } else show_home();
}
static void diary_prompts_show(void)
{
    diary_prompts_load();
    s_diary_mode=8; diary_screen_prepare("A little inspiration");
    diary_label(s_diary_screen,"Optional. No streaks, scores or expectations.",20,64,SCR_W-40,FONT_11);
    diary_label(s_diary_screen,diary_prompt_text(),24,124,SCR_W-48,FONT_18);
    int w=(SCR_W-48)/3;
    diary_button(s_diary_screen,"Another",12,SCR_H-72,w,56,diary_prompt_event,0);
    diary_button(s_diary_screen,s_prompt_for_day?"Add to day":"Add to note",24+w,SCR_H-72,w,56,diary_prompt_event,1);
    diary_button(s_diary_screen,"Not now",36+2*w,SCR_H-72,w,56,diary_prompt_event,2);
    diary_screen_load();
}
static void diary_library_event(lv_event_t *e)
{
    if(diary_modal() || !diary_storage_ok()) return;
    standby_reset_timer(); int id=(int)(intptr_t)lv_event_get_user_data(e);
    if(id<0) { s_library_page=std::max(0,s_library_page+(id==-1?-1:1)); diary_library_show(s_library_kind); return; }
    int idx=id/2; if(idx>=(int)s_library_paths.size()) return;
    if(id%2==0) { diary_read_path(s_library_paths[idx].c_str()); return; }
    /* Restore as a NEW document. Never overwrite a current or open entry. */
    const char *ext=strrchr(s_library_paths[idx].c_str(),'.');
    for(int i=0;i<10000;++i) {
        char dst[300]; snprintf(dst,sizeof(dst),"%s/recovered-%010lld-%04d%s",sd_card_get_mount_point(),(long long)time(NULL),i,ext?ext:".md");
        struct stat st; if(stat(dst,&st)==0) continue;
        if(errno!=ENOENT || !diary_copy_exclusive(s_library_paths[idx].c_str(),dst)) {
            show_msg_popup("Restore failed","The recovery copy is unchanged. Check free space and the SD card.",false); return;
        }
        diary_read_path(dst);
        show_msg_popup("Restored as a new note","Find recovered-* in My documents. Your current entry and recovery copy were not overwritten.",false); return;
    }
    show_msg_popup("Restore failed","No unused recovery filename was available.",false);
}
static void diary_library_show(int kind)
{
    if(!diary_storage_ok()) return;
    s_library_kind=kind; s_diary_mode=9;
    diary_screen_prepare(kind==0?"Local versions":kind==1?"Trash":"Export copies");
    std::string root=std::string(sd_card_get_mount_point())+"/.diary/"+(kind==0?"versions":kind==1?"trash":"exports");
    s_library_paths.clear();
    DIR *d=opendir(root.c_str());
    if(d) {
        while(auto ent=readdir(d)) {
            if(ent->d_name[0]=='.') continue;
            auto files=diary_files(root+"/"+ent->d_name,false);
            s_library_paths.insert(s_library_paths.end(),files.begin(),files.end());
        }
        closedir(d);
    }
    std::sort(s_library_paths.rbegin(),s_library_paths.rend());
    int rows=std::max(1,(SCR_H-132)/72);
    s_library_page=std::min(s_library_page,std::max(0,((int)s_library_paths.size()-1)/rows));
    if(s_library_paths.empty()) diary_label(s_diary_screen,"Nothing here yet.\nRecovery copies stay on this SD card.",20,90,SCR_W-40,FONT_11);
    for(int i=0;i<rows && s_library_page*rows+i<(int)s_library_paths.size();++i) {
        int idx=s_library_page*rows+i; const auto &path=s_library_paths[idx];
        auto slash=path.find_last_of('/'); auto name=path.substr(slash+1);
        auto start=path.find_last_of('/',slash-1); std::string stamp=path.substr(start+1,slash-start-1);
        time_t t=(time_t)strtoll(stamp.c_str(),NULL,10); struct tm tm; localtime_r(&t,&tm);
        char date[48]; strftime(date,sizeof(date),"%d %b %Y %H:%M",&tm);
        std::string caption=name+"\n"+date;
        auto b=diary_button(s_diary_screen,caption.c_str(),12,58+i*72,SCR_W-162,64,diary_library_event,idx*2);
        auto l=lv_obj_get_child(b,0); lv_obj_set_height(l,50); lv_label_set_long_mode(l,LV_LABEL_LONG_DOT);
        diary_button(s_diary_screen,"Restore\ncopy",SCR_W-142,58+i*72,130,64,diary_library_event,idx*2+1);
    }
    diary_button(s_diary_screen,"Newer",12,SCR_H-58,(SCR_W-36)/2,46,diary_library_event,-1);
    diary_button(s_diary_screen,"Older",24+(SCR_W-36)/2,SCR_H-58,(SCR_W-36)/2,46,diary_library_event,-2);
    diary_screen_load();
}
static void diary_privacy_event(lv_event_t *e)
{
    if(diary_modal()) return;
    standby_reset_timer(); int id=(int)(intptr_t)lv_event_get_user_data(e);
    if(id<3) { s_library_page=0; diary_library_show(id); return; }
    if(id==4) { diary_sync(); return; }
    if(!diary_storage_ok() || !diary_save_before_leave()) return;
    bool saved=true;
    editor_doc_foreach([](editor_doc_t *doc,void *p) {
        if(editor_doc_is_modified(doc) && (!editor_doc_get_path(doc) || editor_doc_save(doc)!=ESP_OK)) *(bool *)p=false;
    },&saved);
    if(!saved) { show_msg_popup("Export paused","Save every open note first.",false); return; }
    auto files=diary_files(sd_card_get_mount_point(),false); int count=0;
    for(const auto &path:files) {
        if(!diary_archive(sd_card_get_mount_point(),path.c_str(),"exports")) {
            show_msg_popup("Export incomplete","Some copies may exist in .diary/exports. Originals are unchanged; check card space and try again.",false); return;
        }
        ++count;
    }
    char msg[180]; snprintf(msg,sizeof(msg),"Copied %d documents to .diary/exports on the SD card. Copy this folder to your computer for an independent backup. No credentials are included.",count);
    show_msg_popup("Export complete",msg,false);
}
static void diary_privacy_show(void)
{
    s_diary_mode=10; diary_screen_prepare("Privacy & recovery");
    diary_label(s_diary_screen,"Your SD card is not encrypted. Versions, Trash and exports are local copies, not protection against losing the card. Git history may retain deleted writing.",16,60,SCR_W-32,FONT_11);
    const char *names[]={"Versions","Trash","Exports","Export all","Sync destination"};
    const int w=(SCR_W-40)/2,h=56,top=SCR_H-192;
    for(int i=0;i<5;++i) diary_button(s_diary_screen,names[i],12+(i%2)*(w+12),top+(i/2)*62,w,h,diary_privacy_event,i);
    diary_screen_load();
}
