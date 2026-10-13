/* Touch-first diary organisation. Mutations reuse editor recovery/atomic saves;
 * destructive cleanup and two-file moves have separate confirmation screens. */
static bool s_prefs_loaded=false;
static uint8_t s_quote_state[100]={}; /* 0 normal, 1 favourite, 2 hidden */
static int s_quote_mode=0,s_quote_index=0;
static unsigned s_keep_versions=0;
static int s_extra_filter=-1,s_extra_page=0;
static std::vector<std::string> s_extra_files,s_custom_prompts;
static std::string s_label_path,s_label_original,s_conflict_path,s_conflict_original,s_conflict_result;
static diary_labels_t s_labels;
static int s_conflict_choice=0;
static std::vector<diary_copy_t> s_cleanup_plan;
static char s_custom_prompt[257]={};static int s_custom_pos=0;
static lv_obj_t *s_custom_label=NULL;
static int s_move_year=2026,s_move_month=1,s_move_day=1;
static std::string s_move_source;
static std::string s_move_original;
static size_t s_move_first=0,s_move_end=0;
static bool s_ntp_busy=false;static uint32_t s_ntp_attempt=0;
static char s_ntp_notice[160]="Network time is optional. Connect WiFi first.";

static void diary_preferences_load(void)
{
    git_sync_set_network_time(false); /* One owner for optional NTP. */
    if(s_prefs_loaded) return;
    s_prefs_loaded=true;
    nvs_handle_t h;if(nvs_open("diaryprefs",NVS_READONLY,&h)!=ESP_OK) return;
    uint8_t v=0;
    if(nvs_get_u8(h,"zone",&v)==ESP_OK) s_diary_zone=v<9?v:0;
    if(nvs_get_u8(h,"ntp",&v)==ESP_OK) s_network_clock=v==1;
    if(nvs_get_u8(h,"font",&v)==ESP_OK) s_read_font_size=v<3?v:1;
    if(nvs_get_u8(h,"spacing",&v)==ESP_OK) s_read_spacing=(v==4 || v==8 || v==12)?v:8;
    if(nvs_get_u8(h,"quotes",&v)==ESP_OK) s_quote_mode=v<3?v:0;
    if(nvs_get_u8(h,"keep",&v)==ESP_OK) s_keep_versions=(v==20 || v==50)?v:0;
    size_t n=sizeof(s_quote_state);nvs_get_blob(h,"quote_state",s_quote_state,&n);
    for(auto &q:s_quote_state) if(q>2) q=0;
    nvs_close(h);
}
static void diary_preferences_save(void)
{
    nvs_handle_t h;bool ok=nvs_open("diaryprefs",NVS_READWRITE,&h)==ESP_OK;
    if(ok) {
        ok=nvs_set_u8(h,"zone",s_diary_zone)==ESP_OK && nvs_set_u8(h,"ntp",s_network_clock)==ESP_OK &&
           nvs_set_u8(h,"font",s_read_font_size)==ESP_OK && nvs_set_u8(h,"spacing",s_read_spacing)==ESP_OK &&
           nvs_set_u8(h,"quotes",s_quote_mode)==ESP_OK && nvs_set_u8(h,"keep",s_keep_versions)==ESP_OK &&
           nvs_set_blob(h,"quote_state",s_quote_state,sizeof(s_quote_state))==ESP_OK && nvs_commit(h)==ESP_OK;
        nvs_close(h);
    }
    if(!ok) show_msg_popup("Setting not remembered","This change applies for this session only. Try again before restarting.",false);
}
static const lv_font_t *diary_read_font(void)
{
    return s_read_font_size==0?FONT_11:s_read_font_size==2?FONT_18:FONT_14;
}
static const char *diary_pick_quote(void)
{
    if(s_quote_mode==2) return "";
    std::vector<int> choices;
    for(int i=0;i<100;++i) if(s_quote_state[i]!=2 && (s_quote_mode==0 || s_quote_state[i]==1)) choices.push_back(i);
    if(choices.empty()) return s_quote_mode==1?"Choose a favourite quote in Personalise diary.":"";
    s_quote_index=choices[esp_random()%choices.size()];return DIARY_QUOTES[s_quote_index];
}
static bool diary_write_small(const std::string &path,const std::string &text)
{
    return sd_card_write_file(path.c_str(),text.data(),text.size())==ESP_OK;
}
static bool diary_save_all(void)
{
    if(!diary_storage_ok() || !diary_save_before_leave()) return false;
    bool ok=true;editor_doc_foreach([](editor_doc_t *d,void *p){
        if(editor_doc_is_modified(d) && (!editor_doc_get_path(d) || editor_doc_save(d)!=ESP_OK)) *(bool*)p=false;
    },&ok);
    if(!ok) show_msg_popup("Save your writing first","At least one open note could not be saved. Nothing has been moved or replaced.",false);
    return ok;
}
static bool diary_checked_replace(const std::string &path,const std::string &before,const std::string &after)
{
    if(!diary_save_all()) return false;
    std::string disk;if(!diary_read_small(path,disk) || disk!=before) {
        show_msg_popup("Entry changed","Open it again before applying this change. Nothing has been overwritten.",false);return false;
    }
    if(!open_into_pane(s_focus,path.c_str())) return false;
    size_t n=0;const char *text=editor_get_text(&n);
    if(std::string(text,n)!=before) {
        show_msg_popup("Open copy differs","Reopen the entry before changing it. Your open writing is unchanged.",false);return false;
    }
    if(editor_replace_range(0,n,after.c_str())!=ESP_OK) return false;
    if(editor_save_file()!=ESP_OK) {
        show_msg_popup("Not saved","The updated writing is still in memory. Use Save before continuing. A previous version is kept when saving succeeds.",false);return false;
    }
    return true;
}
static void diary_reload_open(void)
{
    auto active=editor_get_active();
    editor_doc_foreach([](editor_doc_t *d,void*) {
        const char *p=editor_doc_get_path(d);if(!p || editor_doc_is_modified(d)) return;
        std::string path=p;editor_set_active(d);editor_open_file(path.c_str());
    },NULL);
    editor_set_active(active);
}
static void diary_extra_list_show(void);
static void diary_topics_show(void);
static void diary_storage_show(void);
static void diary_quotes_show(void);
static void diary_custom_show(void);
static void diary_move_render(void);
static void diary_review_show(void);
static void diary_extra_event(lv_event_t *e)
{
    if(diary_modal()) return;
    standby_reset_timer();int id=(int)(intptr_t)lv_event_get_user_data(e);
    if(id==0) {s_extra_filter=-1;s_extra_page=0;diary_extra_list_show();}
    else if(id==1) diary_topics_show();
    else if(id==2) {s_extra_filter=-2;s_extra_page=0;diary_extra_list_show();}
    else if(id==3) {
        if(!diary_storage_ok()) return;
        auto files=diary_files(sd_card_get_mount_point(),true);
        if(files.empty()) show_msg_popup("Nothing to revisit","Your diary starts with one day. Tap Today to begin.",false);
        else diary_read_path(files[esp_random()%files.size()].c_str());
    } else if(id==4) diary_storage_show();
    else if(id==5) diary_personal_show();
    else if(id==6) diary_zone_show();
    else diary_conflicts_show();
}
static void diary_hub_show(void)
{
    diary_preferences_load();s_diary_mode=20;diary_screen_prepare("Explore your diary");
    const char *names[]={"Bookmarks","Topics","On this day","Revisit a day","Storage","Personalise","Time & zone","Review conflicts"};
    int cols=SCR_W<600?2:3,rows=(8+cols-1)/cols,w=(SCR_W-32)/cols,h=(SCR_H-72)/rows;
    for(int i=0;i<8;++i) diary_button(s_diary_screen,names[i],12+(i%cols)*(w+4),60+(i/cols)*h,w-4,h-10,diary_extra_event,i);
    diary_screen_load();
}
static void diary_topic_event(lv_event_t *e)
{
    if(diary_modal()) return;
    s_extra_filter=(int)(intptr_t)lv_event_get_user_data(e);s_extra_page=0;diary_extra_list_show();
}
static void diary_topics_show(void)
{
    s_diary_mode=33;diary_screen_prepare("Browse topics");
    int w=(SCR_W-36)/2,h=(SCR_H-72)/4;
    for(int i=0;i<8;++i) diary_button(s_diary_screen,DIARY_TOPICS[i],12+(i%2)*(w+12),60+(i/2)*h,w,h-8,diary_topic_event,i);
    diary_screen_load();
}
static void diary_extra_list_event(lv_event_t *e)
{
    if(diary_modal()) return;
    int id=(int)(intptr_t)lv_event_get_user_data(e);
    if(id<0) {s_extra_page=std::max(0,s_extra_page+(id==-1?-1:1));diary_extra_list_show();
    return;}
    if(id<(int)s_extra_files.size()) diary_read_path(s_extra_files[id].c_str());
}
static void diary_extra_list_show(void)
{
    if(!diary_storage_ok() || !diary_save_all()) return;
    struct tm now={};if(s_extra_filter==-2 && !diary_now(&now)) {diary_clock_show();
    return;}
    s_extra_files.clear();int skipped=0;
    for(const auto &path:diary_files(sd_card_get_mount_point(),true)) {
        if(s_extra_filter==-2) {
            auto name=path.substr(path.find_last_of('/')+1);int y,m,d;
            if(diary_filename(name.c_str(),&y,&m,&d) && m==now.tm_mon+1 && d==now.tm_mday && y<now.tm_year+1900) s_extra_files.push_back(path);
        } else {
            std::string text;if(!diary_read_small(path,text)) {++skipped;continue;}
            auto labels=diary_labels(text);
            if(s_extra_filter==-1?labels.star:(labels.tags&(1u<<s_extra_filter))) s_extra_files.push_back(path);
        }
    }
    s_diary_mode=22;diary_screen_prepare(s_extra_filter==-2?"On this day":s_extra_filter==-1?"Bookmarked days":DIARY_TOPICS[s_extra_filter]);
    int rows=std::max(1,(SCR_H-154)/76);s_extra_page=std::min(s_extra_page,std::max(0,((int)s_extra_files.size()-1)/rows));
    if(s_extra_files.empty()) diary_label(s_diary_screen,"Nothing here yet.\nBookmark and tag a day from Reading > Tools > Documents.",20,100,SCR_W-40,FONT_11);
    for(int i=0;i<rows && s_extra_page*rows+i<(int)s_extra_files.size();++i) {
        int idx=s_extra_page*rows+i;auto path=s_extra_files[idx];
        std::string caption=path.substr(path.find_last_of('/')+1,10)+"\n"+diary_excerpt(path);
        auto b=diary_button(s_diary_screen,caption.c_str(),12,60+i*76,SCR_W-24,68,diary_extra_list_event,idx);
        auto l=lv_obj_get_child(b,0);lv_obj_set_height(l,52);lv_label_set_long_mode(l,LV_LABEL_LONG_DOT);
    }
    if(skipped) diary_label(s_diary_screen,"Some unreadable / over-128 KB days were skipped.",12,SCR_H-92,SCR_W-24,FONT_11);
    auto prev=diary_button(s_diary_screen,"Newer",12,SCR_H-58,(SCR_W-36)/2,46,diary_extra_list_event,-1);
    auto next=diary_button(s_diary_screen,"Older",24+(SCR_W-36)/2,SCR_H-58,(SCR_W-36)/2,46,diary_extra_list_event,-2);
    if(!s_extra_page) lv_obj_add_state(prev,LV_STATE_DISABLED);
    if((s_extra_page+1)*rows>=(int)s_extra_files.size()) lv_obj_add_state(next,LV_STATE_DISABLED);
    diary_screen_load();
}
static void diary_labels_render(void);
static void diary_labels_event(lv_event_t *e)
{
    if(diary_modal()) return;
    int id=(int)(intptr_t)lv_event_get_user_data(e);
    if(id==9) {
        if(diary_checked_replace(s_label_path,s_label_original,diary_with_labels(s_label_original,s_labels))) diary_read_path(s_label_path.c_str());
        return;
    }
    if(id==8) s_labels.star=!s_labels.star;else s_labels.tags^=1u<<id;
    diary_labels_render();
}
static void diary_labels_render(void)
{
    s_diary_mode=21;diary_screen_prepare("Bookmark & topics");
    diary_button(s_diary_screen,s_labels.star?"Bookmarked: yes":"Bookmark this day",12,54,SCR_W-24,48,diary_labels_event,8);
    int w=(SCR_W-36)/2,h=(SCR_H-176)/4;
    for(int i=0;i<8;++i) {
        auto b=diary_button(s_diary_screen,DIARY_TOPICS[i],12+(i%2)*(w+12),112+(i/2)*h,w,h-5,diary_labels_event,i);
        if(s_labels.tags&(1u<<i)) lv_obj_set_style_border_width(b,4,0);
    }
    diary_button(s_diary_screen,"Save for this day",12,SCR_H-54,SCR_W-24,44,diary_labels_event,9);
    diary_screen_load();
}
static void diary_labels_show(void)
{
    if(!diary_save_all()) return;
    s_label_path=s_tools_path.empty()?s_read_path:s_tools_path;
    auto name=s_label_path.substr(s_label_path.find_last_of('/')+1);int y,m,d;
    if(!diary_filename(name.c_str(),&y,&m,&d) || s_label_path!=std::string(sd_card_get_mount_point())+"/"+name || !diary_read_small(s_label_path,s_label_original)) {
        show_msg_popup("Open a diary day","Bookmarks and topics apply to dated Markdown entries up to 128 KB. Open a day first.",false);
        return;
    }
    s_labels=diary_labels(s_label_original);diary_labels_render();
}
static void diary_cleanup_show(void);
static void diary_cleanup_event(lv_event_t *e)
{
    if(diary_modal() || !diary_save_all()) return;
    int id=(int)(intptr_t)lv_event_get_user_data(e);
    if(id==0) {s_keep_versions=s_keep_versions==0?20:s_keep_versions==20?50:0;diary_preferences_save();diary_storage_show();
    return;}
    std::vector<diary_copy_t> copies;
    if(!diary_copies(sd_card_get_mount_point(),"versions",copies)) {show_msg_popup("Storage unavailable","Could not inspect recovery copies. Nothing was removed.",false);
    return;}
    auto plan=diary_prune_plan(copies,s_keep_versions);
    if(id==2) {
        bool same=plan.size()==s_cleanup_plan.size();
        for(size_t i=0;same && i<plan.size();++i) same=plan[i].path==s_cleanup_plan[i].path;
        if(!same) {show_msg_popup("Recovery copies changed","Review the cleanup again before confirming.",false);
        return;}
        size_t removed=0;bool ok=diary_prune_versions(sd_card_get_mount_point(),s_keep_versions,removed);
        char text[180];snprintf(text,sizeof(text),"Removed %u older version copies%s. They cannot be restored from this card. Current entries, Trash and exports were not deleted.",(unsigned)removed,ok?"":" before an error");
        diary_storage_show();show_msg_popup(ok?"Cleanup complete":"Cleanup stopped",text,false);
        return;
    }
    s_cleanup_plan=plan;diary_cleanup_show();
}
static void diary_cleanup_show(void)
{
    const auto &plan=s_cleanup_plan;
    s_diary_mode=24;diary_screen_prepare("Review cleanup");
    size_t bytes=0;for(const auto &p:plan) bytes+=p.bytes;
    char text[240];snprintf(text,sizeof(text),"Remove %u older version copies (%.1f MB)?\n\nKeep the latest %u per document. Current writing, Trash and exports stay untouched. Removed copies cannot be recovered from this card.",(unsigned)plan.size(),bytes/1048576.0,s_keep_versions);
    diary_label(s_diary_screen,plan.empty()?"Nothing to remove. Keep all is the safe default.":text,20,74,SCR_W-40,FONT_11);
    if(!plan.empty()) diary_button(s_diary_screen,"Delete listed older versions",12,SCR_H-64,SCR_W-24,50,diary_cleanup_event,2);
    diary_screen_load();
}
static void diary_storage_show(void)
{
    if(!diary_storage_ok()) return;
    diary_preferences_load();
    uint64_t total=0,free=0;bool space=sd_card_space(&total,&free)==ESP_OK;
    std::vector<diary_copy_t> versions,exports,trash;
    bool ok=diary_copies(sd_card_get_mount_point(),"versions",versions) && diary_copies(sd_card_get_mount_point(),"exports",exports) && diary_copies(sd_card_get_mount_point(),"trash",trash);
    auto bytes=[](const std::vector<diary_copy_t> &v){size_t n=0;for(const auto &x:v)n+=x.bytes;return n;};
    s_diary_mode=23;diary_screen_prepare("Storage & recovery");
    char text[400];snprintf(text,sizeof(text),"%s\nFree: %.1f MB of %.1f MB\nVersions: %.1f MB (%u copies)\nExports: %.1f MB\nTrash: %.1f MB\n%s",!space?"Card space unavailable":free<16777216 || free<total/20?"Low space - back up your diary":"Card space available",free/1048576.0,total/1048576.0,bytes(versions)/1048576.0,(unsigned)versions.size(),bytes(exports)/1048576.0,bytes(trash)/1048576.0,ok?"Cleanup is manual, never automatic.":"Inventory incomplete; use a computer to inspect.");
    diary_label(s_diary_screen,text,16,64,SCR_W-32,FONT_11);
    char keep[70];snprintf(keep,sizeof(keep),s_keep_versions?"Keep latest %u versions per document":"Keep all versions",s_keep_versions);
    diary_button(s_diary_screen,keep,12,SCR_H-120,SCR_W-24,48,diary_cleanup_event,0);
    auto b=diary_button(s_diary_screen,"Review cleanup",12,SCR_H-62,SCR_W-24,48,diary_cleanup_event,1);
    if(!ok || !s_keep_versions) lv_obj_add_state(b,LV_STATE_DISABLED);
    diary_screen_load();
}
static void diary_personal_event(lv_event_t *e)
{
    if(diary_modal()) return;
    int id=(int)(intptr_t)lv_event_get_user_data(e);
    if(id==0) s_read_font_size=(s_read_font_size+1)%3;
    else if(id==1) s_read_spacing=s_read_spacing==12?4:s_read_spacing+4;
    else if(id==2) {diary_quotes_show();
    return;}
    else {diary_custom_show();
    return;}
    diary_preferences_save();s_read_large=false;diary_personal_show();
}
static void diary_personal_show(void)
{
    diary_preferences_load();s_diary_mode=25;diary_screen_prepare("Personalise your diary");
    const char *fonts[]={"Small","Comfortable","Large"};char font[64],space[64];
    snprintf(font,sizeof(font),"Reading text: %s",fonts[s_read_font_size]);snprintf(space,sizeof(space),"Line spacing: %s",s_read_spacing==4?"Compact":s_read_spacing==8?"Comfortable":"Airy");
    const char *names[]={font,space,"Home quotes","Add your own prompt"};
    for(int i=0;i<4;++i) diary_button(s_diary_screen,names[i],12,64+i*((SCR_H-88)/4),SCR_W-24,(SCR_H-88)/4-8,diary_personal_event,i);
    diary_screen_load();
}
static void diary_quote_event(lv_event_t *e)
{
    if(diary_modal()) return;
    int id=(int)(intptr_t)lv_event_get_user_data(e);
    if(id==0) s_quote_index=(s_quote_index+99)%100;
    if(id==1) s_quote_index=(s_quote_index+1)%100;
    if(id==2) s_quote_state[s_quote_index]=s_quote_state[s_quote_index]==1?0:1;
    if(id==3) s_quote_state[s_quote_index]=s_quote_state[s_quote_index]==2?0:2;
    if(id==4) s_quote_mode=(s_quote_mode+1)%3;
    if(id>=2) diary_preferences_save();
    diary_quotes_show();
}
static void diary_quotes_show(void)
{
    s_diary_mode=26;diary_screen_prepare("Your Home quotes");
    diary_label(s_diary_screen,DIARY_QUOTES[s_quote_index],20,72,SCR_W-40,FONT_14);
    const char *modes[]={"Home: all visible","Home: favourites","Home: no quote"};
    const char *names[]={"Previous","Next",s_quote_state[s_quote_index]==1?"Unfavourite":"Favourite",s_quote_state[s_quote_index]==2?"Unhide":"Hide",modes[s_quote_mode]};
    int w=(SCR_W-36)/2;
    for(int i=0;i<4;++i) diary_button(s_diary_screen,names[i],12+(i%2)*(w+12),SCR_H-178+(i/2)*56,w,48,diary_quote_event,i);
    diary_button(s_diary_screen,names[4],12,SCR_H-62,SCR_W-24,48,diary_quote_event,4);
    diary_screen_load();
}
static void diary_prompts_load(void)
{
    s_custom_prompts.clear();std::string text;
    if(diary_read_small(std::string(sd_card_get_mount_point())+"/diary-prompts.txt",text)) {
        for(size_t p=0;p<text.size() && s_custom_prompts.size()<100;) {
            auto end=text.find('\n',p);if(end==std::string::npos) end=text.size();
            auto prompt=diary_trim(text.substr(p,end-p));p=end+1;
            if(!prompt.empty() && prompt.size()<=256) s_custom_prompts.push_back(prompt);
        }
    }
    s_prompt%=diary_prompt_count();
}
static int diary_prompt_count(void) { return 12+(int)s_custom_prompts.size(); }
static const char *diary_prompt_text(void)
{
    return s_prompt<12?DIARY_PROMPTS[s_prompt]:s_custom_prompts[s_prompt-12].c_str();
}
static void diary_custom_event(lv_event_t *)
{
    if(diary_modal() || !diary_save_all()) return;
    std::string prompt=diary_trim(s_custom_prompt);if(prompt.empty()) return;
    auto path=std::string(sd_card_get_mount_point())+"/diary-prompts.txt";std::string before;
    if(!diary_read_small(path,before,true) || before.size()>25000) {show_msg_popup("Prompt list unavailable","Keep at most 100 prompts of 256 bytes each. Manage diary-prompts.txt on a computer.",false);
    return;}
    diary_prompts_load();if(s_custom_prompts.size()>=100) {show_msg_popup("Prompt list full","Remove a prompt from diary-prompts.txt before adding another.",false);
    return;}
    std::string after=before+(before.empty() || before.back()=='\n'?"":"\n")+prompt+"\n";
    if(!diary_archive(sd_card_get_mount_point(),path.c_str(),"versions") || !diary_write_small(path,after)) {show_msg_popup("Prompt not saved","Your typed prompt is still here. Check the SD card and try again.",false);
    return;}
    diary_reload_open();s_custom_prompt[0]=0;s_custom_pos=0;diary_prompts_load();s_prompt=diary_prompt_count()-1;s_prompt_for_day=false;
    diary_personal_show();show_msg_popup("Prompt saved","Find it using Another in Prompts. diary-prompts.txt is included in normal Git sync; edit that file to remove prompts.",false);
}
static void diary_custom_show(void)
{
    s_diary_mode=27;diary_screen_prepare("Add a personal prompt");
    diary_label(s_diary_screen,"Use your keyboard (256 bytes max).\nSaved in diary-prompts.txt.",16,64,SCR_W-32,FONT_11);
    auto area=lv_obj_create(s_diary_screen);lv_obj_remove_style_all(area);
    lv_obj_set_pos(area,20,144);lv_obj_set_size(area,SCR_W-40,SCR_H-220);lv_obj_set_scroll_dir(area,LV_DIR_VER);
    s_custom_label=diary_label(area,*s_custom_prompt?s_custom_prompt:"Your prompt...",0,0,SCR_W-48,FONT_14);
    diary_button(s_diary_screen,"Save prompt",12,SCR_H-64,SCR_W-24,50,diary_custom_event,0);
    diary_screen_load();
}
static void diary_custom_prompt_key(const kb_event_t *e)
{
    text_field_key(s_custom_prompt,sizeof(s_custom_prompt),&s_custom_pos,e);
    lv_label_set_text(s_custom_label,*s_custom_prompt?s_custom_prompt:"Your prompt...");
}
static void diary_ntp_task(void *)
{
    esp_sntp_config_t cfg=ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    bool owner=esp_netif_sntp_init(&cfg)==ESP_OK;
    bool ok=owner && esp_netif_sntp_sync_wait(pdMS_TO_TICKS(12000))==ESP_OK;
    if(owner) esp_netif_sntp_deinit();
    draftling_lvgl_port_lock(-1);
    bool rtc=ok && s_clock_writer && s_clock_writer(time(NULL));
    snprintf(s_ntp_notice,sizeof(s_ntp_notice),"%s",ok?(rtc?"Network time saved to the hardware clock.":"Time updated for this session; hardware clock write failed."):"Time not updated. Check WiFi and retry; another time sync may be running.");
    s_ntp_busy=false;
    if(lv_scr_act()==s_diary_screen && s_diary_mode==28) diary_zone_show();
    draftling_lvgl_port_unlock();vTaskDelete(NULL);
}
static void diary_network_time(bool manual)
{
    diary_preferences_load();if(s_ntp_busy || (!manual && !s_network_clock)) return;
    if(!manual && s_ntp_attempt && lv_tick_get()-s_ntp_attempt<3600000) return;
    if(!wifi_manager_is_connected() || git_sync_get_state()==GIT_SYNC_IN_PROGRESS) {
        if(manual) show_msg_popup("Connect WiFi first","Use Sync & backup > WiFi setup. Time sync never enables WiFi by itself, and waits for Git sync to finish.",false);
        return;
    }
    s_ntp_attempt=lv_tick_get();s_ntp_busy=true;snprintf(s_ntp_notice,sizeof(s_ntp_notice),"Checking network time...");
    if(xTaskCreate(diary_ntp_task,"diary_time",4096,NULL,3,NULL)!=pdPASS) {s_ntp_busy=false;snprintf(s_ntp_notice,sizeof(s_ntp_notice),"Could not start time sync. Try again.");}
}
static void diary_zone_event(lv_event_t *e)
{
    if(diary_modal()) return;
    int id=(int)(intptr_t)lv_event_get_user_data(e);
    if(id<9) {s_diary_zone=id;diary_preferences_save();diary_zone_apply();}
    else if(id==9) {s_network_clock=!s_network_clock;diary_preferences_save();}
    else diary_network_time(true);
    diary_zone_show();
}
static void diary_zone_show(void)
{
    diary_preferences_load();s_diary_mode=28;diary_screen_prepare("Timezone & network time");
    int cols=SCR_W<600?2:3,rows=(9+cols-1)/cols,w=(SCR_W-32)/cols,h=(SCR_H-184)/rows;
    for(int i=0;i<9;++i) {
        auto b=diary_button(s_diary_screen,DIARY_ZONE_NAMES[i],12+(i%cols)*(w+4),56+(i/cols)*h,w-4,h-5,diary_zone_event,i);
        if(i==s_diary_zone) lv_obj_set_style_border_width(b,4,0);
    }
    diary_label(s_diary_screen,s_ntp_notice,12,SCR_H-120,SCR_W-24,FONT_11);
    int half=(SCR_W-36)/2;
    diary_button(s_diary_screen,s_network_clock?"Auto time: on":"Auto time: off",12,SCR_H-62,half,48,diary_zone_event,9);
    auto b=diary_button(s_diary_screen,s_ntp_busy?"Checking...":"Sync time now",24+half,SCR_H-62,half,48,diary_zone_event,10);
    if(s_ntp_busy) lv_obj_add_state(b,LV_STATE_DISABLED);
    diary_screen_load();
}
static void diary_move_event(lv_event_t *e)
{
    if(diary_modal()) return;
    int id=(int)(intptr_t)lv_event_get_user_data(e);
    if(id==99) {
        if(!diary_save_all()) return;
        std::string root=sd_card_get_mount_point(),journal=root+"/.diary/move/pending";
        if(!sd_card_file_exists(journal.c_str())) {
            std::string current;
            if(!diary_read_small(root+"/"+s_move_source,current) || current!=s_move_original) {
                show_msg_popup("Entry changed","Open the moment again before moving it. Nothing has been moved.",false);
                return;
            }
            char dest[32];snprintf(dest,sizeof(dest),"%04d-%02d-%02d.md",s_move_year,s_move_month,s_move_day);
            if(!diary_move_prepare(root,s_move_source,dest,s_move_first,s_move_end,diary_write_small)) {
                show_msg_popup("Move not started","Choose a different date. Both days must be readable UTF-8 files under 128 KB with room on the SD card.",false);
                return;
            }
        }
        bool ok=diary_move_resume(root,diary_write_small);diary_reload_open();
        if(ok) {diary_hub_show();show_msg_popup("Moment moved","Appended to the destination day. Other writing and the moment's original time are preserved. Previous files are in Versions.",false);}
        else show_msg_popup("Move needs attention","A destination copy may already exist; the source is kept until its update succeeds. Retry here after checking the card. Changed files will not be overwritten.",false);
        return;
    }
    int delta=id>0?1:-1,field=std::abs(id);
    if(field==1) s_move_year=std::max(2020,std::min(2099,s_move_year+delta));
    if(field==2) s_move_month=(s_move_month-1+delta+12)%12+1;
    if(field==3) s_move_day=std::max(1,std::min(diary_month_days(s_move_year,s_move_month),s_move_day+delta));
    s_move_day=std::min(s_move_day,diary_month_days(s_move_year,s_move_month));diary_move_render();
}
static void diary_move_render(void)
{
    s_diary_mode=29;diary_screen_prepare("Move moment to a day");
    std::string pending;
    bool resume=diary_read_small(std::string(sd_card_get_mount_point())+"/.diary/move/pending",pending);
    if(resume) diary_label(s_diary_screen,("Interrupted move:\n"+pending+"\nRetry only finishes this recorded move. If either file changed, restore/review the copies on a computer.").c_str(),20,80,SCR_W-40,FONT_11);
    else {
        const char *names[]={"Year","Month","Day"};int values[]={s_move_year,s_move_month,s_move_day};
        for(int i=0;i<3;++i) {
            char label[40];snprintf(label,sizeof(label),"%s: %02d",names[i],values[i]);
            diary_label(s_diary_screen,label,20,64+i*56,SCR_W-160,FONT_14);
            diary_button(s_diary_screen,"-",SCR_W-138,56+i*56,56,48,diary_move_event,-i-1);
            diary_button(s_diary_screen,"+",SCR_W-70,56+i*56,56,48,diary_move_event,i+1);
        }
        diary_label(s_diary_screen,"Appends without replacing the other day's writing.\nKeeps the original time. Versions preserve both files.",16,SCR_H-144,SCR_W-32,FONT_11);
    }
    diary_button(s_diary_screen,resume?"Retry recorded move":"Confirm move",12,SCR_H-64,SCR_W-24,50,diary_move_event,99);
    diary_screen_load();
}
static void diary_move_show(void)
{
    if(!diary_save_all()) return;
    auto pending=std::string(sd_card_get_mount_point())+"/.diary/move/pending";
    if(sd_card_file_exists(pending.c_str())) {diary_move_render();
    return;}
    if(!s_read_is_moment || s_read_path.find("/.diary/")!=std::string::npos) {show_msg_popup("Choose a moment","Tap a moment in a day's timeline, then use Tools > Documents > Move moment.",false);
    return;}
    s_move_source=s_read_path.substr(s_read_path.find_last_of('/')+1);
    if(!diary_filename(s_move_source.c_str(),&s_move_year,&s_move_month,&s_move_day)) return;
    if(s_read_original.empty() || !diary_read_small(s_read_path,s_move_original) || s_move_original!=s_read_original) {
        show_msg_popup("Open the moment again","The entry changed or is larger than 128 KB. No writing has been moved.",false);
        return;
    }
    s_move_first=s_read_start;s_move_end=s_read_end;diary_move_render();
}
static void diary_conflict_event(lv_event_t *e)
{
    if(diary_modal()) return;
    int id=(int)(intptr_t)lv_event_get_user_data(e);
    if(id>=0 && id<(int)s_extra_files.size()) {
        s_conflict_path=s_extra_files[id];if(!diary_read_small(s_conflict_path,s_conflict_original)) return;
        s_conflict_choice=0;diary_review_show();
    }
}
static void diary_conflicts_show(void)
{
    if(!diary_save_all()) return;
    s_extra_files.clear();int skipped=0;
    for(const auto &path:diary_files(sd_card_get_mount_point(),false)) {
        std::string text;if(!diary_read_small(path,text)) {++skipped;continue;}
        if(text.find("<<<<<<<")!=std::string::npos) s_extra_files.push_back(path);
    }
    s_diary_mode=30;diary_screen_prepare("Review sync conflicts");
    int rows=std::max(1,(SCR_H-136)/56);
    if(s_extra_files.empty()) diary_label(s_diary_screen,"No conflict markers found in readable notes.\nLarge or unreadable files need a computer review.",20,90,SCR_W-40,FONT_11);
    for(int i=0;i<rows && i<(int)s_extra_files.size();++i) diary_button(s_diary_screen,s_extra_files[i].substr(s_extra_files[i].find_last_of('/')+1).c_str(),12,60+i*56,SCR_W-24,48,diary_conflict_event,i);
    char note[140];snprintf(note,sizeof(note),"%u notes need review; %d skipped.\nResolve these, then reopen to see any remaining notes.",(unsigned)s_extra_files.size(),skipped);
    diary_label(s_diary_screen,note,12,SCR_H-72,SCR_W-24,FONT_11);
    diary_screen_load();
}
static void diary_review_event(lv_event_t *e)
{
    if(diary_modal()) return;
    int id=(int)(intptr_t)lv_event_get_user_data(e);
    if(id<3) {s_conflict_choice=id;diary_review_show();
    return;}
    if(diary_checked_replace(s_conflict_path,s_conflict_original,s_conflict_result)) {
        diary_conflicts_show();show_msg_popup("Resolved on device","Previous content is kept in Versions. Sync again to back up this resolution. Git history may retain both versions.",false);
    }
}
static void diary_review_show(void)
{
    int blocks=0;bool valid=diary_resolve(s_conflict_original,s_conflict_choice,s_conflict_result,blocks);
    s_diary_mode=31;diary_screen_prepare("Conflict resolution");
    const char *names[]={"Keep both","First version","Second version"};int w=(SCR_W-40)/3;
    for(int i=0;i<3;++i) {
        auto b=diary_button(s_diary_screen,names[i],12+i*(w+8),54,w,54,diary_review_event,i);
        if(i==s_conflict_choice) lv_obj_set_style_border_width(b,4,0);
    }
    auto area=lv_obj_create(s_diary_screen);lv_obj_remove_style_all(area);lv_obj_set_pos(area,16,120);lv_obj_set_size(area,SCR_W-32,SCR_H-200);lv_obj_set_scroll_dir(area,LV_DIR_VER);
    std::string preview=valid?diary_short_text(s_conflict_result,4096):"These markers are incomplete or use diff3. Resolve this file on a computer; nothing has been changed.";
    if(s_conflict_result.size()>4096) preview+="\n[Preview limited to 4 KB. Review the full file on a computer.]";
    diary_label(area,preview.c_str(),0,0,SCR_W-40,FONT_11);
    auto b=diary_button(s_diary_screen,"Confirm resolution for this file",12,SCR_H-64,SCR_W-24,50,diary_review_event,3);
    if(!valid || s_conflict_result.size()>4096) lv_obj_add_state(b,LV_STATE_DISABLED);
    diary_screen_load();
}
static void diary_status_event(lv_event_t *e)
{
    if(diary_modal()) return;
    int id=(int)(intptr_t)lv_event_get_user_data(e);
    if(id==0) diary_sync();else diary_conflicts_show();
}
static void diary_status_show(void)
{
    if(!diary_storage_ok()) return;
    int backed=0,pending=0,conflicts=0,unreadable=0,unsaved=0;
    editor_doc_foreach([](editor_doc_t *d,void *p){if(editor_doc_is_modified(d)) ++*(int*)p;},&unsaved);
    for(const auto &path:diary_files(sd_card_get_mount_point(),true)) {
        auto name=path.substr(path.find_last_of('/')+1);
        if(git_sync_file_status(name.c_str())==GIT_SYNC_FILE_PUSHED) ++backed;else ++pending;
        std::string text;if(!diary_read_small(path,text)) ++unreadable;
        else if(text.find("<<<<<<<")!=std::string::npos) ++conflicts;
    }
    s_diary_mode=32;diary_screen_prepare("Saved here / backed up");
    char text[600];snprintf(text,sizeof(text),"Unsaved open notes: %d\nSaved diary days: %d\nMatching last known remote copy: %d\nNeed backup / not verified: %d\nConflict markers: %d (%d not checked)\n\n%s\n\nRemote status is checked locally, not a live server guarantee. Conflicts need review even after sync succeeds.",unsaved,backed+pending,backed,pending,conflicts,unreadable,s_diary_notice);
    auto label=diary_label(s_diary_screen,text,16,64,SCR_W-32,FONT_11);lv_obj_set_height(label,SCR_H-136);lv_label_set_long_mode(label,LV_LABEL_LONG_DOT);
    int w=(SCR_W-36)/2;
    diary_button(s_diary_screen,"Sync & backup",12,SCR_H-64,w,50,diary_status_event,0);
    diary_button(s_diary_screen,"Review conflicts",24+w,SCR_H-64,w,50,diary_status_event,1);
    diary_screen_load();
}
