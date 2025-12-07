global View_ID global_pre_compilation_view = 0;

CUSTOM_COMMAND_SIG(kill_yank_indent)
CUSTOM_DOC("Kills a range, yanks, and idents.")
{
    delete_range(app);
    paste(app);
    auto_indent_range(app);
}

CUSTOM_COMMAND_SIG(close_all_panels_except_active)
CUSTOM_DOC("Closes all panels except the currently active one.")
{
    View_ID active_view = get_active_view(app, Access_Always);
    
    for (;;) {
        View_ID view_to_close = 0;
        
        for (View_ID view = get_view_next(app, 0, Access_Always);
             view != 0;
             view = get_view_next(app, view, Access_Always))
				{
            if (view != active_view) {
                view_to_close = view;
                break;
            }
        }
        
        if (view_to_close != 0) {
            view_close(app, view_to_close);
        } else {
            break;
        }
    }
}

CUSTOM_COMMAND_SIG(insert_four_spaces)
CUSTOM_DOC("Inserts four spaces at the cursor position.")
{
    View_ID view = get_active_view(app, Access_ReadWriteVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWriteVisible);
    i64 pos = view_get_cursor_pos(app, view);
    
    buffer_replace_range(app, buffer, Ii64(pos), string_u8_litexpr("    "));
    view_set_cursor_and_preferred_x(app, view, seek_pos(pos + 4));
}

function void
change_active_panel_send_command_with_compilation(Application_Links *app, Custom_Command_Function *custom_func){
    View_ID current_view = get_active_view(app, Access_Always);
    View_ID start_view = current_view;
    View_ID next_view = 0;
    
    next_view = get_view_next(app, current_view, Access_Always);
    
    if (next_view == 0){
        next_view = get_view_next(app, 0, Access_Always);
    }
    
    if (next_view != 0 && next_view != start_view){
        view_set_active(app, next_view);
        
        if (custom_func != 0){
            view_enqueue_command_function(app, next_view, custom_func);
        }
    }
}

CUSTOM_COMMAND_SIG(change_active_panel_with_compilation)
CUSTOM_DOC("Change the currently active panel, including the compilation panel in the cycle.")
{
    change_active_panel_send_command_with_compilation(app, 0);
}

CUSTOM_COMMAND_SIG(change_to_build_panel_expanding)
CUSTOM_DOC("Expand the build panel and make it the active buffer")
{
    View_ID current_view = get_active_view(app, Access_Always);

    f4_toggle_compilation_expand(app);

    if (global_compilation_view_expanded) {
        global_pre_compilation_view = current_view;

        Buffer_ID buffer = get_buffer_by_name(app, string_u8_litexpr("*compilation*"), Access_Always);

        for (View_ID view = get_view_next(app, 0, Access_Always); view != 0;
             view = get_view_next(app, view, Access_Always)) {
            Buffer_ID view_buffer = view_get_buffer(app, view, Access_Always);
            if (view_buffer == buffer) {
                view_set_active(app, view);
                break;
            }
        }
    } else {
        // Collapsing - restore the previous view
        if (global_pre_compilation_view != 0 &&
            view_exists(app, global_pre_compilation_view)) {
            view_set_active(app, global_pre_compilation_view);
        }
    }
}

CUSTOM_COMMAND_SIG(execute_previous_cli_without_changing_current_panel)
{
    Scratch_Block scratch(app);
    
    String_Const_u8 cmd = SCu8(command_space);
    String_Const_u8 hot_directory = SCu8(hot_directory_space);
    String_Const_u8 out_buffer_name = SCu8(out_buffer_space);
    
    if (cmd.size == 0 || hot_directory.size == 0) return;
    
    View_ID original_view = get_active_view(app, Access_Always);
    
    Buffer_ID buffer = get_buffer_by_name(app, out_buffer_name, Access_Always);
    if (buffer == 0) {
        buffer = create_buffer(app, out_buffer_name, BufferCreate_NeverAttachToFile);
    }
    
    if (buffer != 0) {
        buffer_set_setting(app, buffer, BufferSetting_ReadOnly, false);
        buffer_replace_range(app, buffer, Ii64(0, buffer_get_size(app, buffer)), SCu8());
        
        Child_Process_ID child_process = create_child_process(app, hot_directory, cmd);
        if (child_process != 0) {
            child_process_set_target_buffer(
							app,
							child_process,
							buffer, 
							ChildProcessSet_FailIfBufferAlreadyAttachedToAProcess);
            
            for (View_ID view = get_view_next(app, 0, Access_Always); 
                 view != 0;
                 view = get_view_next(app, view, Access_Always))
						{
                Buffer_ID view_buffer = view_get_buffer(app, view, Access_Always);
                if (view_buffer == buffer) {
                    global_compilation_view = view;
                    break;
                }
            }
        }
        
        // Restore the original view - don't jump!
        view_set_active(app, original_view);
    }
}

CUSTOM_COMMAND_SIG(execute_any_cli_in_build)
CUSTOM_DOC("Queries for a system command, runs the system command as a CLI and prints the output to the *compilation* buffer.")
{
    Scratch_Block scratch(app);
    Query_Bar_Group group(app);

	  String_Const_u8 out_buf = string_u8_litexpr("*compilation*");
		out_buf.size = clamp_top(out_buf.size, sizeof(out_buffer_space) - 1);
		memcpy(out_buffer_space, out_buf.str, out_buf.size);
    out_buffer_space[out_buf.size] = 0;
    
    Query_Bar bar_cmd = {};
    bar_cmd.prompt = string_u8_litexpr("Command: ");
    bar_cmd.string = SCu8(command_space, (u64)0);
    bar_cmd.string_capacity = sizeof(command_space);
    if (!query_user_string(app, &bar_cmd)) return;
    bar_cmd.string.size = clamp_top(bar_cmd.string.size, sizeof(command_space) - 1);
    command_space[bar_cmd.string.size] = 0;
    
    String_Const_u8 hot = push_hot_directory(app, scratch);
    {
        u64 size = clamp_top(hot.size, sizeof(hot_directory_space));
        block_copy(hot_directory_space, hot.str, size);
        hot_directory_space[hot.size] = 0;
    }
    
    execute_previous_cli_without_changing_current_panel(app);
}

CUSTOM_COMMAND_SIG(execute_previous_cli_in_build)
CUSTOM_DOC("Queries for a system command, runs the system command as a CLI and prints the output to the *compilation* buffer.")
{
    Scratch_Block scratch(app);

    String_Const_u8 cmd = SCu8(command_space);
    String_Const_u8 hot_directory = SCu8(hot_directory_space);

		if (cmd.size <= 0 || hot_directory.size <= 0) return;

	  String_Const_u8 out_buf = string_u8_litexpr("*compilation*");
		out_buf.size = clamp_top(out_buf.size, sizeof(out_buffer_space) - 1);
		memcpy(out_buffer_space, out_buf.str, out_buf.size);
    out_buffer_space[out_buf.size] = 0;
    
    String_Const_u8 hot = push_hot_directory(app, scratch);
    {
        u64 size = clamp_top(hot.size, sizeof(hot_directory_space));
        block_copy(hot_directory_space, hot.str, size);
        hot_directory_space[hot.size] = 0;
    }
    
    execute_previous_cli_without_changing_current_panel(app);
}
