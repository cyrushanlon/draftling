/* Shared touch navigation. All callbacks run under the LVGL task lock. */
static void diary_key(uint16_t key,uint8_t modifier=0)
{
    kb_event_t event={}; event.keycode=key; event.modifier=modifier; event.pressed=true;
    process_key_event(&event);
}
static bool diary_modal(void)
{
    return s_msg_open || s_save_open || s_exit_open || s_del_open || s_search_open || s_wifi_pw_open || s_dbx_open;
}
static void diary_tools_show(void);
static int s_tools_group=0;
static void diary_tool_event(lv_event_t *e)
{
    if(diary_modal()) return;
    int id=(int)(intptr_t)lv_event_get_user_data(e);
    standby_reset_timer();
    if(id==25) {diary_hub_show();return;}
    if(id==26) {diary_labels_show();return;}
    if(id==27) {diary_move_show();return;}
    if(id>=100 && id<=103) { s_tools_group=id-100; diary_tools_show(); return; }
    if(id==21) { diary_journal_show(); return; }
    if(id==22) {
        if(!s_tools_path.empty()) {
            if(s_tools_path.find("/.diary/")!=std::string::npos) { show_msg_popup("Recovery copy","Restore a copy before adding a prompt.",false); return; }
            if(!diary_storage_ok() || !diary_save_before_leave()) return;
            const char *current=editor_get_file_path();
            if((!current || s_tools_path!=current) && !open_into_pane(s_focus,s_tools_path.c_str())) {
                show_msg_popup("Cannot open note","The selected note is unavailable.",false); return;
            }
        }
        s_prompt_for_day=false; diary_prompts_show(); return;
    }
    if(id==23) { diary_privacy_show(); return; }
    if(id==24) { if(diary_save_before_leave()) { const char *p=s_tools_path.empty()?editor_get_file_path():s_tools_path.c_str(); if(p) diary_read_path(p); else show_msg_popup("Save first","Save this note before opening reading view.",false); } return; }
    if((id>=6 && id<=10) || id==12 || id==18 || id==19 || id==20) {
        if(!s_tools_path.empty()) {
            if(s_tools_path.find("/.diary/")!=std::string::npos) { show_msg_popup("Recovery copy","Restore a copy from Privacy & recovery before editing it.",false); return; }
            if(!diary_storage_ok() || !diary_save_before_leave()) return;
            const char *current=editor_get_file_path();
            if((!current || s_tools_path!=current) && !open_into_pane(s_focus,s_tools_path.c_str())) {
                show_msg_popup("Cannot open note","The selected note is unavailable.",false); return;
            }
        }
    }
    if((id>=6 && id<=10) || id==12 || id==18 || id==19) {
        if(!diary_require_keyboard()) return;
    }
    if(id==0) { if(diary_save_before_leave()) { create_new_document(NEWDOC_EDITOR); editor_ui_show_editor(); } }
    else if(id==1) diary_calendar_show();
    else if(id==2) diary_sync();
    else if(id==3) show_settings();
    else if(id==4) diary_clock_show();
    else if(id==5) show_advanced_menu();
    else if(id==11) { if(diary_save_before_leave()) standby_enter_sleep(); }
    else if(id==19 || id==20) {
        if(!diary_storage_ok() || !diary_save_before_leave()) return;
        const char *path=editor_get_file_path();
        if(!path) { show_msg_popup("Choose a document","Open a saved document first, then use Rename or Delete.",false); return; }
        const char *name=strrchr(path,'/'); name=name?name+1:path;
        char selected[256]; snprintf(selected,sizeof(selected),"%s",name);
        editor_ui_show_file_browser(); refresh_file_list(selected);
        if(id==19) show_rename_prompt(); else show_delete_prompt();
    }
    else {
        editor_ui_show_editor();
        if(id==6) diary_key(0x1d,KB_MOD_LCTRL); /* undo */
        if(id==7) diary_key(0x1c,KB_MOD_LCTRL); /* redo */
        if(id==8) diary_key(0x06,KB_MOD_LCTRL); /* copy */
        if(id==9) diary_key(0x19,KB_MOD_LCTRL); /* paste */
        if(id==10) diary_key(0x09,KB_MOD_LCTRL); /* find */
        if(id==12) diary_key(0x04,KB_MOD_LCTRL); /* select all */
        if(id==13) diary_key(0x1f,KB_MOD_LCTRL); /* split */
        if(id==14) diary_key(0x1e,KB_MOD_LCTRL); /* single pane */
        if(id==15) diary_key(KB_KEY_TAB,KB_MOD_LCTRL);
        if(id==16) show_help(HELP_FROM_EDITOR);
        if(id==17) diary_key(0x15,KB_MOD_LCTRL); /* clean refresh */
        if(id==18) show_save_prompt();
    }
}
static void diary_tools_show(void)
{
    s_diary_mode=4;
    const char *titles[]={"Tools","Writing tools","Documents","Device"};
    diary_screen_prepare(titles[s_tools_group]);
    const char *names[]={"New note","Calendar","Sync","Settings","Date & time","Advanced",
                         "Undo","Redo","Copy","Paste","Find","Sleep","Select all",
                         "Split view","Single view","Other pane","Help","Refresh","Save as","Rename","Trash note",
                         "Journal","Prompts","Privacy & recovery","Read note","Explore diary","Bookmark & topics","Move moment"};
    if(s_tools_group==0) {
        const char *groups[]={"Writing","Documents","Device"};
        const int w=(SCR_W-40)/3;
        for(int i=0;i<3;++i) diary_button(s_diary_screen,groups[i],10+i*(w+5),84,w,120,diary_tool_event,101+i);
        diary_label(s_diary_screen,"Writing: edit and reflect\nDocuments: read, organise and recover\nDevice: connection, display and sleep",20,230,SCR_W-40,FONT_11);
    } else {
        const int writing[]={6,7,8,9,10,12,22,13,14,15};
        const int documents[]={24,25,26,27,21,1,0,18,19,20,23,2};
        const int device[]={3,4,5,16,17,11};
        const int *items=s_tools_group==1?writing:s_tools_group==2?documents:device;
        int count=s_tools_group==1?10:s_tools_group==2?12:6;
        const int cols=SCR_W<600?2:3, rows=(count+cols-1)/cols;
        int w=(SCR_W-40)/cols,h=(SCR_H-68)/rows;
        for(int i=0;i<count;++i) diary_button(s_diary_screen,names[items[i]],10+(i%cols)*(w+5),58+(i/cols)*h,w,h-8,diary_tool_event,items[i]);
    }
    diary_screen_load();
}
static void diary_nav_event(lv_event_t *e)
{
    const int id=(int)(intptr_t)lv_event_get_user_data(e);
    standby_reset_timer();
    if(lv_scr_act()==s_scr_ble_prompt) { if(id==0 || id==1) show_home(); return; }
    if(diary_modal()) {
        if(id==2) diary_key(KB_KEY_ENTER);
        else if(id==0 || id==1) diary_key(KB_KEY_ESCAPE);
        return;
    }
    if(id==0) { if(s_settings_open && s_restart_needed) { request_close_settings(); return; } if(diary_save_before_leave()) show_home(); }
    else if(id==1) {
        if(lv_scr_act()==s_diary_screen) {
            if(s_diary_mode==20) {diary_journal_show();return;}
            if(s_diary_mode==24) {diary_storage_show();return;}
            if(s_diary_mode==26 || s_diary_mode==27) {diary_personal_show();return;}
            if(s_diary_mode==31) {diary_conflicts_show();return;}
            if(s_diary_mode==32) {diary_sync();return;}
            if(s_diary_mode>20) {diary_hub_show();return;}
            if(s_diary_mode==4 && s_tools_group) { s_tools_group=0; diary_tools_show(); }
            else if(s_diary_mode==9) diary_privacy_show();
            else if(s_diary_mode==6 && s_read_is_moment) diary_day_show();
            else if(s_diary_mode==12) { if(diary_save_before_leave()) diary_day_show(); }
            else if(s_diary_mode==6) diary_journal_show();
            else if(s_diary_mode==1 || s_diary_mode==5 || s_diary_mode==7) diary_calendar_show(); else show_home();
        } else if(lv_scr_act()==s_scr_browser) show_home();
        else if(s_settings_open && !s_settings_advanced && !s_keyboard_settings_open && !s_restart_confirm_open) request_close_settings();
        else diary_key(KB_KEY_ESCAPE);
    } else if(id==2) {
        if(lv_scr_act()==s_scr_browser || s_inpane_browser_open) {
            if(diary_save_before_leave()) { create_new_document(s_inpane_browser_open?NEWDOC_INPANE:NEWDOC_BROWSER); }
        } else if(lv_scr_act()==s_scr) {
            if(!editor_get_file_path()) { editor_ui_show_editor(); show_save_prompt(); }
            else if(diary_storage_ok()) {
                if(editor_save_file()==ESP_OK) {
                    editor_ui_set_status("Saved on device");
                    show_msg_popup("Saved on device","Use Sync & backup to make a remote copy.",false);
                } else show_msg_popup("Not saved","Check the SD card and try again. Your writing is still open.",false);
            }
        } else diary_today_open();
    } else {
        const char *path=editor_get_file_path();
        if(lv_scr_act()==s_diary_screen && s_diary_mode==6) s_tools_path=s_read_path;
        else s_tools_path=path?path:"";
        s_tools_group=0; diary_tools_show();
    }
}
static void diary_choice_event(lv_event_t *e)
{
    const int id=(int)(intptr_t)lv_event_get_user_data(e);
    if(id>=200) { s_del_sel=id-200; delete_prompt_activate(); }
    else { s_exit_sel=id-100; exit_prompt_activate(); }
}
static void diary_list_event(lv_event_t *e)
{
    if(diary_modal()) return;
    auto row=(lv_obj_t *)lv_event_get_target(e);
    auto list=lv_obj_get_parent(row);
    int idx=(int)lv_obj_get_index(row);
    kb_event_t event={}; event.keycode=KB_KEY_ENTER; event.pressed=true;
    standby_reset_timer();
    if(list==s_menu_list) {
        if(s_wifi_scan_picker_open) s_wifi_scan_picker_sel=idx;
#if defined(CONFIG_DRAFTLING_HAS_USB_MSC)
        else if(s_usbmsc_picker_open) s_usbmsc_picker_sel=idx;
#endif
        else if(s_fmt_step!=FMT_STEP_NONE) { fmt_picker_activate(idx); return; }
        else s_menu_sel=idx;
        handle_menu_key(&event);
    } else if(list==s_settings_list) {
        if(s_restart_confirm_open) s_restart_confirm_sel=idx;
        else if(s_layouts_picker_open) s_layouts_picker_sel=idx;
#if defined(CONFIG_DRAFTLING_DISPLAY_COLOR)
        else if(s_theme_picker_open) s_theme_picker_sel=idx;
#endif
        else s_settings_sel=idx;
        handle_settings_key(&event);
    } else { s_browser_sel=idx; browser_activate_item(idx); }
}
static void diary_wire_list(lv_obj_t *list)
{
#if defined(CONFIG_DRAFTLING_TOUCHSCREEN)
    if(!list) return;
    for(uint32_t i=0;i<lv_obj_get_child_count(list);++i) {
        auto row=lv_obj_get_child(list,i); bool wired=false;
        for(uint32_t j=0;j<lv_obj_get_event_count(row);++j)
            if(lv_event_dsc_get_cb(lv_obj_get_event_dsc(row,j))==diary_list_event) wired=true;
        if(wired) continue;
        lv_obj_remove_event_cb(row,list_touch_event_cb);
        lv_obj_remove_event_cb(row,tile_touch_event_cb);
        lv_obj_add_event_cb(row,diary_list_event,LV_EVENT_CLICKED,NULL);
        lv_obj_set_style_min_height(row,44,0);
    }
#endif
}
static void diary_nav_refresh(void)
{
#if defined(CONFIG_DRAFTLING_TOUCHSCREEN)
    if(!s_diary_nav) return;
    lv_obj_remove_flag(s_diary_nav,LV_OBJ_FLAG_HIDDEN);
    const bool modal=diary_modal();
    const char *primary=modal?"Confirm":lv_scr_act()==s_scr_browser || s_inpane_browser_open?"New note":lv_scr_act()==s_scr?"Save":"Today";
    const char *names[]={modal?"Cancel":"Home","Back",primary,"Tools"};
    for(int i=0;i<4;++i) {
        auto button=lv_obj_get_child(s_diary_nav,i), label=lv_obj_get_child(button,0);
        if(strcmp(lv_label_get_text(label),names[i])) lv_label_set_text(label,names[i]);
    }
    if(s_menu_open) diary_wire_list(s_menu_list);
    if(s_settings_open && (s_settings_advanced || s_keyboard_settings_open || s_restart_confirm_open || s_layouts_picker_open
#if defined(CONFIG_DRAFTLING_DISPLAY_COLOR)
       || s_theme_picker_open
#endif
       )) diary_wire_list(s_settings_list);
    if(lv_scr_act()==s_scr_browser || s_inpane_browser_open) diary_wire_list(s_browser_list);
#endif
}
static void diary_tick(lv_timer_t *)
{
    diary_nav_refresh();
    static uint32_t last_save=0;
    if(lv_tick_get()-last_save<5000) return;
    last_save=lv_tick_get();
    diary_home_status();
    diary_network_time(false);
    if(git_sync_get_state()==GIT_SYNC_IN_PROGRESS || lv_scr_act()==s_scr_ble_prompt) return;
#if defined(CONFIG_DRAFTLING_HAS_USB_MSC)
    if(usb_msc_get_mode()!=USB_MSC_MODE_OFF) return;
#endif
    static uint32_t last_space=0;
    if(lv_tick_get()-last_space>=60000) {
        last_space=lv_tick_get();uint64_t total=0,available=0;
        if(sd_card_space(&total,&available)==ESP_OK && (available<16777216 || available<total/20))
            editor_ui_set_status("Low SD space - back up; review Storage in Explore diary");
    }
    if(editor_is_modified() && editor_get_file_path() && lv_tick_get()-s_diary_last_input>=3000) {
        if(editor_save_file()==ESP_OK) { update_title_bar(); editor_ui_set_status("Saved on device"); }
        else editor_ui_set_status("Not saved - check SD card, then tap Save");
    }
}
static void diary_date_touch(lv_event_t *)
{
    if(diary_modal() || !diary_save_before_leave()) return;
    diary_clock_show();
}
