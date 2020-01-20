// Colorize 0xAARRGGBB hex color literals in theme-*.4coder files

function void
tv_colorize_hex_colors(Application_Links *app, Buffer_ID buffer, Text_Layout_ID text_layout_id)
{
    Scratch_Block scratch(app);
    Range_i64 visible_range = text_layout_get_visible_range(app, text_layout_id);
    String_Const_u8 text = push_buffer_range(app, scratch, buffer, visible_range);
    
    for (i64 index = visible_range.min; text.size > 0;) {
        if (text.size >= 2+8 && text.str[0] == '0' && (text.str[1] == 'x' || text.str[1] == 'X')) {
            text = string_skip(text, 2);
            index += 2;
            
            b32 looks_like_hex_number = true;
            for (i32 i = 0; i < 8; i++) {
                char c = text.str[i];
                if (!((c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F') || (c >= '0' && c <= '9'))) {
                    looks_like_hex_number = false;
                    break;
                }
            }
            if (!looks_like_hex_number) {
                continue;
            }
            
            String_Const_u8 hex_text = string_prefix(text, 8);
            u32 color = (u32)string_to_integer(hex_text, 16);
            
            paint_text_color(app, text_layout_id, Ii64_size(index - 1, 1), color);
            
            paint_text_color(app, text_layout_id, Ii64_size(index - 2, 1), color);
            draw_character_block(app, text_layout_id, Ii64_size(index - 2, 1), 2.0f, color);
            
            text = string_skip(text, 8);
            index += 8;
        } else {
            text = string_skip(text, 1);
            index += 1;
        }
    }
}

#if 0
// add somewhere in render_buffer():

{
    Scratch_Block scratch(app);
    String_Const_u8 buffer_name = push_buffer_base_name(app, scratch, buffer);        
    if (string_match_insensitive(string_prefix(buffer_name, 6), string_u8_litexpr("theme-")) &&
        string_match_insensitive(string_postfix(buffer_name, 7), string_u8_litexpr(".4coder")))
    {
        tv_colorize_hex_colors(app,  buffer, text_layout_id);
    }
}  
#endif
