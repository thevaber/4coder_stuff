#if !TV_4CODER_HAS_UTILS_CPP
#error Must include tv_utils.cpp before tv_if0.cpp
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if 0
// for #if 0..#endif highlighting, add the following line somewhere in your render_buffer() (after all other code-highlighting calls):
tv_draw_if0_fade(app, buffer, view_id, text_layout_id, &token_array);
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

internal b32
tv__check_if0(Application_Links* app, Buffer_ID buffer, Token_Array* tokens, Token* tok, u64* out_x,
              Token** out_tok_number)
{
    if (tok && tok->sub_kind == TokenCppKind_PPIf) {
        Token* tok_after_if = tv__token_inc(tokens, tok);
        if (tok_after_if && tok_after_if->kind == TokenBaseKind_LiteralInteger) {
            u64 x = 0;
            b32 good_integer_literal = false;

#if TV_4CODER_HAS_INTEGER_CPP
            good_integer_literal = tv_read_integer_literal(app, buffer, tok_after_if->pos,
                                                           &x, NULL, NULL, NULL);
#else
            if (tok_after_if->size == 1) {
                char c = buffer_get_char(app, buffer, tok_after_if->pos);
                if (c >= '0' && c <= '9') {
                    x = (c == '0') ? 0 : 1;
                    good_integer_literal = true;
                }
            }
#endif
            if (good_integer_literal) {
                Token* tok_after_if0 = tv__token_inc(tokens, tok_after_if);

                if (tok_after_if0) {
                    // must be "#if <number>" by itself, not "#if <number> && something" or similar
                    i64 line_a = get_line_number_from_pos(app, buffer, tok->pos);
                    i64 line_b = get_line_number_from_pos(app, buffer, tok_after_if0->pos);
                    if (line_a != line_b) {
                        if (out_x) *out_x = x;
                        if (out_tok_number) *out_tok_number = tok_after_if;
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

internal Token*
tv__draw_if0_fade_scope(Application_Links* app, Arena* arena, Buffer_ID buffer, View_ID view,
                        Text_Layout_ID text_layout_id, Token_Array* tokens, Token* tok_if,
                        i64 visible_range_max)
{
    i32 if0_kind = -1; // -1 = not in #if 0; 0 = in #if 0; 1 = in #if !0

    Token* tok = tok_if;
    {
        u64 x;
        if (tv__check_if0(app, buffer, tokens, tok, &x, NULL)) {
            if0_kind = (x == 0) ? 0 : 1;
            tok = tv__token_inc(tokens, tok); // skip #if
            tok = tv__token_inc(tokens, tok); // skip number after #if
        } else {
            tok = tv__token_inc(tokens, tok); // skip #if/#ifdef/#ifndef
        }
    }

    i64 start = (tok) ? tok->pos : -1;
    i64 end = -1;

    while (tok) {
        end = tok->pos + tok->size;
        if (if0_kind != 0 && tok->pos > visible_range_max) {
            break;
        }

        b32 start_if0_now = false;
        b32 end_if0_now = false;
        b32 end_if = false;

        if (tok->sub_kind == TokenCppKind_PPElse) {
            if (if0_kind == 0) {
                end_if0_now = true;
                if0_kind = 1;
                end -= tok->size;
            } else if (if0_kind == 1) {
                start = tok->pos + tok->size;
                start_if0_now = true;
                if0_kind = 0;
            }
        } else if (tok->sub_kind == TokenCppKind_PPElIf) {
            if (if0_kind == 0) {
                end_if0_now = true;
                end -= tok->size;
            }
            if0_kind = -1;
        } else if (tok->sub_kind == TokenCppKind_PPEndIf) {
            if (if0_kind == 0) {
                end_if0_now = true;
                end -= tok->size;
            }
            end_if = true;
        }

        if (tok->sub_kind == TokenCppKind_PPIf ||
            tok->sub_kind == TokenCppKind_PPIfDef ||
            tok->sub_kind == TokenCppKind_PPIfNDef)
        {
            tok = tv__draw_if0_fade_scope(app, arena, buffer, view,
                                          text_layout_id, tokens, tok,
                                          visible_range_max);
        } else {
            tok = tv__token_inc(tokens, tok);
        }

        if (!tok && if0_kind == 0) {
            end_if0_now = true;
        }

        if (end_if0_now) {
            paint_text_color_blend(app, text_layout_id, Ii64(start, end), 0x00999999, 0.3f);
        }

        if (end_if) {
            break;
        }
    }

    return tok;
}

function void
tv_draw_if0_fade(Application_Links* app, Buffer_ID buffer, View_ID view,
                 Text_Layout_ID text_layout_id, Token_Array* tokens)
{
    if (!tokens || !tokens->tokens) {
        return;
    }

    Range_i64 visible_range = text_layout_get_visible_range(app, text_layout_id);
    Scratch_Block scratch(app);

    Token* tok = &tokens->tokens[0];
    while (tok) {
        if (tok->pos > visible_range.max) {
            break;
        }

        if (tok->sub_kind == TokenCppKind_PPIf ||
            tok->sub_kind == TokenCppKind_PPIfDef ||
            tok->sub_kind == TokenCppKind_PPIfNDef)
        {
            tok = tv__draw_if0_fade_scope(app, scratch, buffer, view,
                                          text_layout_id, tokens, tok,
                                          visible_range.max);
        } else {
            tok = tv__token_inc(tokens, tok);
        }
    }
}

CUSTOM_COMMAND_SIG(if0_toggle)
CUSTOM_DOC("Toggle #if 0")
{
    View_ID view = get_active_view(app, Access_ReadVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
    i64 pos = view_get_cursor_pos(app, view);

    Token_Array tokens = get_token_array_from_buffer(app, buffer);
    if (tokens.tokens == 0) {
        return;
    }

    Token* tok = token_from_pos(&tokens, pos);
    
    if (tok && tok->sub_kind == TokenCppKind_PPEndIf) {
        tok = tv__token_dec(&tokens, tok); // work properly if cursor is over #endif
    }

    // scan upwards for #if <number>
    Token* tok_number = NULL;
    i64 start = 0;
    u64 x = 1;
    i32 level = 0;
    while (tok) {
        if (tok->sub_kind == TokenCppKind_PPEndIf) {
            level++;
        } else if (tok->sub_kind == TokenCppKind_PPIf ||
                   tok->sub_kind == TokenCppKind_PPIfDef ||
                   tok->sub_kind == TokenCppKind_PPIfNDef)
        {
            if (level > 0) {
                level--;
            } else {
                if (tv__check_if0(app, buffer, &tokens, tok, &x, &tok_number)) {
                    start = tok->pos;
                    break;
                }
            }
        }

        tok = tv__token_dec(&tokens, tok);
    }
    if (!tok_number) {
        return;
    }

    // scan downwards for #endif
    tok = tv__token_inc(&tokens, tok_number);
    level = 0;
    while (tok) {
        if (tok->sub_kind == TokenCppKind_PPIf ||
            tok->sub_kind == TokenCppKind_PPIfDef ||
            tok->sub_kind == TokenCppKind_PPIfNDef)
        {
            level++;
        } else if (tok->sub_kind == TokenCppKind_PPElIf) {
            if (level == 0 && tok->pos <= pos) {
                return; // #if 0 which contains #elif, and the cursor is in/after the #elif -> better don't do anything
            }
        } else if (tok->sub_kind == TokenCppKind_PPEndIf) {
            if (level > 0) {
                level--;
            } else {
                i64 end = tok->pos + tok->size;                
                if (pos >= start && pos <= end) { // check if cursor is actually over this #if 0..#endif
                    String_Const_u8 replacement;
                    if (x == 0) {
                        replacement = string_u8_litexpr("1");
                    } else {
                        replacement = string_u8_litexpr("0");
                    }
                    buffer_replace_range(app, buffer, Ii64_size(tok_number->pos, tok_number->size), replacement);
                }                
                return;
            }
        }
        
        tok = tv__token_inc(&tokens, tok);
    }
}
