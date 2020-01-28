/// Commands:
//     switch_to_if     - Convert switch statement to if-elseif-else
//     if_to_switch     - Convert if-elseif-else statement to swith
//     if_switch_toggle - Togle if-elseif-else <-> switch

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// TODO: cleaner error reporting

#if !TV_4CODER_HAS_UTILS_CPP
#error Must include tv_utils.cpp before tv_switch.cpp
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

internal b32
tv__read_cpp_parens(Token_Array* tokens, Token* tok,
                    Token** out_paren_open_tok, Token** out_paren_close_tok,
                    Token** out_inside_first_tok, Token** out_inside_last_tok)
{
    if (out_paren_open_tok) *out_paren_open_tok = NULL;
    if (out_paren_close_tok) *out_paren_close_tok = NULL;
    if (out_inside_first_tok) *out_inside_first_tok = NULL;
    if (out_inside_last_tok) *out_inside_last_tok = NULL;

    if (!tok || tok->kind != TokenBaseKind_ParentheticalOpen) {
        return false;
    }

    Token* tok_paren_open = tok;
    if (out_paren_open_tok) *out_paren_open_tok = tok_paren_open;

    Token* tok_inside_paren_first = tok = tv__token_inc(tokens, tok);
    if (!tok_inside_paren_first) {
        return false;
    }

    if (tok->kind == TokenBaseKind_ParentheticalClose) { // empty parentheses
        if (out_paren_close_tok) *out_paren_close_tok = tok;
        return true;
    }

    Token* tok_paren_close_tok = tok = tv__skip_nest(tokens, tok_paren_open);
    if (!tok) { // failed to find ')'
        return false;
    }
    Token* tok_inside_paren_last = tv__token_dec(tokens, tok);

    if (out_paren_close_tok) *out_paren_close_tok = tok_paren_close_tok;
    if (out_inside_first_tok) *out_inside_first_tok = tok_inside_paren_first;
    if (out_inside_last_tok) *out_inside_last_tok = tok_inside_paren_last;

    return true;
}

internal b32
tv__read_cpp_scope(Token_Array* tokens, Token* tok,
                   Token** out_scope_open_tok, Token** out_scope_close_tok,
                   Token** out_inside_first_tok, Token** out_inside_last_tok)
{
    if (out_scope_open_tok) *out_scope_open_tok = NULL;
    if (out_scope_close_tok) *out_scope_close_tok = NULL;
    if (out_inside_first_tok) *out_inside_first_tok = NULL;
    if (out_inside_last_tok) *out_inside_last_tok = NULL;

    if (!tok || tok->kind != TokenBaseKind_ScopeOpen) {
        return false;
    }

    Token* tok_scope_open = tok;
    if (out_scope_open_tok) *out_scope_open_tok = tok_scope_open;

    Token* tok_inside_scope_first = tok = tv__token_inc(tokens, tok);
    if (!tok_inside_scope_first) {
        return false;
    }

    if (tok->kind == TokenBaseKind_ScopeClose) { // empty scope
        if (out_scope_close_tok) *out_scope_close_tok = tok;
        return true;
    }

    Token* tok_scope_close = tok = tv__skip_nest(tokens, tok_scope_open);
    if (!tok) { // failed to find '}'
        return false;
    }
    Token* tok_inside_scope_last = tv__token_dec(tokens, tok);

    if (out_scope_close_tok) *out_scope_close_tok = tok_scope_close;
    if (out_inside_first_tok) *out_inside_first_tok = tok_inside_scope_first;
    if (out_inside_last_tok) *out_inside_last_tok = tok_inside_scope_last;

    return true;
}

internal b32
tv__read_cpp_statement(Application_Links *app, Arena* arena, Buffer_ID buffer,
                       Token_Array* tokens, Token* tok,
                       Token** out_first_tok, Token** out_last_tok,
                       String_Const_u8* out_error_text); // fwd

internal b32
tv__read_cpp_simple_statement(Application_Links *app, Arena* arena, Buffer_ID buffer,
                              Token_Array* tokens, Token* tok,
                              Token** out_first_tok, Token** out_last_tok,
                              String_Const_u8* out_error_text)
{
    if (out_first_tok) *out_first_tok = NULL;
    if (out_last_tok) *out_last_tok = NULL;
    if (out_error_text) *out_error_text = {};

    if (!tok)  {
        if (out_error_text) *out_error_text = string_u8_litexpr("unexpected end of buffer while trying to read simple statement");
        return false;
    }

    Token* tok_first = tok;

    while (tok && tok->sub_kind != TokenCppKind_Semicolon) {
        if (tok->kind == TokenBaseKind_ParentheticalOpen || tok->kind == TokenBaseKind_ScopeOpen) {
            tok = tv__skip_nest(tokens, tok);
        } else {
            tok = tv__token_inc(tokens, tok);
        }
    }

    if (!tok) {
        if (out_error_text) *out_error_text = string_u8_litexpr("unexpected end of buffer while trying to read simple statement");
        return false;
    }

    if (out_first_tok) *out_first_tok = tok_first;
    if (out_last_tok) *out_last_tok = tok;

    return true;
}

internal b32
tv__read_cpp_simple_control_statement(Application_Links *app, Arena* arena, Buffer_ID buffer,
                                      Token_Array* tokens, Token* tok,
                                      Token** out_first_tok, Token** out_last_tok,
                                      String_Const_u8* out_error_text)
{
    if (out_first_tok) *out_first_tok = NULL;
    if (out_last_tok) *out_last_tok = NULL;
    if (out_error_text) *out_error_text = {};

    if (!tok) {
        if (out_error_text) *out_error_text = string_u8_litexpr("unexpected end of buffer while trying to read control statement");
        return false;
    }

    Token* tok_keyword = tok;
    tok = tv__token_inc(tokens, tok); // skip the keyword
    Token *paren_open_tok, *paren_close_tok;
    b32 good_parens = tv__read_cpp_parens(tokens, tok,
                                          &paren_open_tok, &paren_close_tok,
                                          NULL, NULL);
    if (!good_parens) {
        if (out_error_text) *out_error_text = string_u8_litexpr("failed to read parentheses of control statement");
        return false;
    }

    tok = tv__token_inc(tokens, paren_close_tok); // skip ')'

    Token *tok_first, *tok_last;
    b32 good_body = tv__read_cpp_statement(app, arena, buffer,
                                           tokens, tok,
                                           &tok_first, &tok_last,
                                           NULL);
    if (!good_body) {
        if (out_error_text) *out_error_text = string_u8_litexpr("failed to read body of control statement");
        return false;
    }

    if (out_first_tok) *out_first_tok = tok_keyword;
    if (out_last_tok) *out_last_tok = tok_last;

    return true;
}

internal b32
tv__read_cpp_do_while_statement(Application_Links *app, Arena* arena, Buffer_ID buffer,
                                Token_Array* tokens, Token* tok,
                                Token** out_first_tok, Token** out_last_tok,
                                String_Const_u8* out_error_text)
{
    if (out_first_tok) *out_first_tok = NULL;
    if (out_last_tok) *out_last_tok = NULL;
    if (out_error_text) *out_error_text = {};

    if (!tok) {
        if (out_error_text) *out_error_text = string_u8_litexpr("unexpected end of buffer while trying to read do..while statement");
        return false;
    }

    Token* tok_keyword = tok;
    tok = tv__token_inc(tokens, tok); // skip the 'do' keyword

    Token *tok_body_first, *tok_body_last;
    b32 good_body = tv__read_cpp_statement(app, arena, buffer,
                                           tokens, tok,
                                           &tok_body_first, &tok_body_last,
                                           NULL);
    if (!good_body) {
        if (out_error_text) *out_error_text = string_u8_litexpr("failed to read body of do..while statement");
        return false;
    }

    tok = tv__token_inc(tokens, tok_body_last);
    if (!tv__token_is_while_keyword(tok)) {
        if (out_error_text) *out_error_text = string_u8_litexpr("failed to read body of do..while statement - missing 'while'");
        return false;
    }

    // read the while(...); part as a simple statement (ie. read until ';')
    Token *tok_while_first, *tok_while_last;
    b32 good_while_part = tv__read_cpp_simple_statement(app, arena, buffer,
                                                        tokens, tok,
                                                        &tok_while_first, &tok_while_last,
                                                        NULL);
    if (!good_while_part) {
        if (out_error_text) *out_error_text = string_u8_litexpr("failed to read 'while (...);' of do..while statement");
        return false;
    }

    if (out_first_tok) *out_first_tok = tok_keyword;
    if (out_last_tok) *out_last_tok = tok_while_last;

    return true;
}

internal b32
tv__read_cpp_if_else_statement(Application_Links *app, Arena* arena, Buffer_ID buffer,
                               Token_Array* tokens, Token* tok,
                               Token** out_first_tok, Token** out_last_tok,
                               String_Const_u8* out_error_text)
{
    if (out_first_tok) *out_first_tok = NULL;
    if (out_last_tok) *out_last_tok = NULL;
    if (out_error_text) *out_error_text = {};

    if (!tok) {
        if (out_error_text) *out_error_text = string_u8_litexpr("unexpected end of buffer while trying to read 'if..else' statement");
        return false;
    }

    Token* tok_if = tok;
    tok = tv__token_inc(tokens, tok_if); // skip 'if'
    if (tv__token_is_constexpr_keyword(app, arena, buffer, tok)) {
        tok = tv__token_inc(tokens, tok); // skip 'constexpr'
    }

    Token *if_paren_open_tok, *if_paren_close_tok;
    b32 good_if_parens = tv__read_cpp_parens(tokens, tok,
                                             &if_paren_open_tok, &if_paren_close_tok,
                                             NULL, NULL);
    if (!good_if_parens) {
        if (out_error_text) *out_error_text = string_u8_litexpr("failed to read parentheses for 'if' statement condition");
        return false;
    }

    tok = tv__token_inc(tokens, if_paren_close_tok); // skip ')'

    Token *tok_if_body_first, *tok_if_body_last;
    b32 good_if_body = tv__read_cpp_statement(app, arena, buffer,
                                              tokens, tok,
                                              &tok_if_body_first, &tok_if_body_last,
                                              NULL);
    if (!good_if_body) {
        if (out_error_text) *out_error_text = string_u8_litexpr("failed to read body of 'if' statement");
        return false;
    }

    tok = tv__token_inc(tokens, tok_if_body_last);
    if (!tv__token_is_else_keyword(tok)) {
        // no 'else' part
        if (out_first_tok) *out_first_tok = tok_if;
        if (out_last_tok) *out_last_tok = tok_if_body_last;
        return true;
    }

    Token* tok_else = tok;
    tok = tv__token_inc(tokens, tok_else); // skip 'else'

    Token *tok_else_body_first, *tok_else_body_last;
    b32 good_else_body = tv__read_cpp_statement(app, arena, buffer,
                                                tokens, tok,
                                                &tok_else_body_first, &tok_else_body_last,
                                                NULL);
    if (!good_else_body) {
        if (out_error_text) *out_error_text = string_u8_litexpr("failed to read body of 'else' statement");
        return false;
    }

    if (out_first_tok) *out_first_tok = tok_if;
    if (out_last_tok) *out_last_tok = tok_else_body_last;
    return true;
}

internal b32
tv__read_cpp_statement(Application_Links *app, Arena* arena, Buffer_ID buffer,
                       Token_Array* tokens, Token* tok,
                       Token** out_first_tok, Token** out_last_tok,
                       String_Const_u8* out_error_text)
{
    if (out_first_tok) *out_first_tok = NULL;
    if (out_last_tok) *out_last_tok = NULL;
    if (out_error_text) *out_error_text = {};

    if (!tok)  {
        if (out_error_text) *out_error_text = string_u8_litexpr("unexpected end of buffer while trying to read statement");
        return false;
    }

    // TODO: handle try..catch statements
    if (tok->kind == TokenBaseKind_ScopeOpen) { // compound statement
        Token *tok_scope_open, *tok_scope_close;
        b32 good_scope = tv__read_cpp_scope(tokens, tok,
                                            &tok_scope_open, &tok_scope_close,
                                            NULL, NULL);
        if (!good_scope) {
            if (out_error_text) *out_error_text = string_u8_litexpr("failed to read compound statement");
            return false;
        }
        if (out_first_tok) *out_first_tok = tok_scope_open;
        if (out_last_tok) *out_last_tok = tok_scope_close;
        return true;
    } else if (tv__token_is_switch_keyword(tok) ||
               tv__token_is_while_keyword(tok) ||
               tv__token_is_for_keyword(tok))
    {
        return tv__read_cpp_simple_control_statement(app, arena, buffer,
                                                     tokens, tok,
                                                     out_first_tok, out_last_tok,
                                                     out_error_text);
    } else if (tv__token_is_do_keyword(tok)) {
        return tv__read_cpp_do_while_statement(app, arena, buffer,
                                               tokens, tok,
                                               out_first_tok, out_last_tok,
                                               out_error_text);
    } else if (tv__token_is_if_keyword(tok)) {
        return tv__read_cpp_if_else_statement(app, arena, buffer,
                                              tokens, tok,
                                              out_first_tok, out_last_tok,
                                              out_error_text);
    } else {
        return tv__read_cpp_simple_statement(app, arena, buffer,
                                             tokens, tok,
                                             out_first_tok, out_last_tok,
                                             out_error_text);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct TV_If_Statement_Case {
    Token *tok_condition_first, *tok_condition_last;
    Token *tok_body_first, *tok_body_last;

    TV_If_Statement_Case* next;
};

struct TV_If_Statement {
    Token* tok_first_if;
    Token* tok_last;

    TV_If_Statement_Case* ifs;
    TV_If_Statement_Case* else_case;
};

internal b32
tv__find_if_at_position(Application_Links* app, Arena* arena, Buffer_ID buffer, i64 pos, Token_Array* tokens,
                        b32 report_error,
                        Token** out_tok_first_if)
{
    Token* tok = token_from_pos(tokens, pos);

    // find 'if' keyword
    {
        i32 level = 0;
        while (tok) {
            if (level <= 0 && tv__token_is_if_keyword(tok)) {
                break;
            } else if (tok->kind == TokenBaseKind_ScopeClose) {
                level++;
            } else if (tok->kind == TokenBaseKind_ScopeOpen) {
                level--;
            }
            tok = tv__token_dec(tokens, tok);
        }
    }

    if (!tok) {
        if (report_error) {
            print_message(app, string_u8_litexpr("error parsing 'if' statement: cursor not over if statement\n"));
        }
        return false;
    }

    Token* tok_first_if = tok;

    // go up the if-elseif-else chain and find the first 'if'
    {
        for (;;) {
            Token* tok_before_if = tv__token_dec(tokens, tok_first_if);
            if (!tv__token_is_else_keyword(tok_before_if)) {
                break; // no more 'else'
            }
            tok = tv__token_dec(tokens, tok_before_if);
            if (!tok) {
                break; // ('else' at beginning of buffer)
            }
            i32 level = 0;
            while (tok) { // find if on same scope level
                if (level == 0 && tv__token_is_if_keyword(tok)) {
                    tok_first_if = tok;
                    break;
                } else if (tok->kind == TokenBaseKind_ScopeClose) {
                    level++;
                } else if (tok->kind == TokenBaseKind_ScopeOpen) {
                    if (level > 0) {
                        level--;
                    } else {
                        tok = NULL;
                        break;
                    }
                }
                tok = tv__token_dec(tokens, tok);
            }
        }
    }

    Token *if_else_stmt_first, *if_else_stmt_last;
    String_Const_u8 err_text = {};
    b32 good_if_else_stmt = tv__read_cpp_if_else_statement(app, arena, buffer,
                                                           tokens, tok_first_if,
                                                           &if_else_stmt_first, &if_else_stmt_last,
                                                           &err_text);
    if (!good_if_else_stmt) {
        if (report_error) {
            print_message(app, string_u8_litexpr("error parsing 'if' statement:\n"));
            print_message(app, string_u8_litexpr("   "));
            print_message(app, err_text);
            print_message(app, string_u8_litexpr("\n"));
        }
        return false;
    }

    if (pos < tok_first_if->pos || pos > if_else_stmt_last->pos+1) {
        if (report_error) {
            print_message(app, string_u8_litexpr("error parsing 'if' statement: cursor not over 'if' statement\n"));
        }
        return false;
    }

    if (out_tok_first_if) *out_tok_first_if = tok_first_if;

    return true;
}

function b32
tv_parse_if(Application_Links* app, Arena* arena, Buffer_ID buffer, i64 pos, Token_Array* tokens,
            TV_If_Statement* out_if_statement)
{
    TV_If_Statement ifst = {};
    TV_If_Statement_Case* cases_tail = NULL;

    Token* tok_first_if;
    b32 found_if = tv__find_if_at_position(app, arena, buffer, pos, tokens, true, &tok_first_if);
    if (!found_if) {
        return false;
    }

    ifst.tok_first_if = tok_first_if;

    b32 is_first_if = true;
    Token* tok = tok_first_if;
    Token* tok_last = tok;
    for (;;) { // read all if cases
        b32 is_if = false;
        b32 is_else = false;
        if (tv__token_is_else_keyword(tok)) {
            tok = tv__token_inc(tokens, tok);
            if (!tok) {
                print_message(app, string_u8_litexpr("error parsing 'if' statement: unexpected end of buffer after 'else'\n"));
                return false;
            }
            is_else = true;
        } else if (!is_first_if) {
            break;
        }

        if (tv__token_is_if_keyword(tok)) {
            is_if = true;
        }

        if (!is_else && !is_if) {
            break;
        }

        Token *tok_if_paren_open = NULL, *tok_if_paren_close = NULL;
        Token *tok_inside_if_paren_first = NULL, *tok_inside_if_paren_last = NULL;
        if (is_if) {
            tok = tv__token_inc(tokens, tok); // skip 'if'
            if (tv__token_is_constexpr_keyword(app, arena, buffer, tok)) {
                tok = tv__token_inc(tokens, tok); // skip 'constexpr'
            }
            if (tok->kind != TokenBaseKind_ParentheticalOpen) {
                print_message(app, string_u8_litexpr("error parsing 'if' statement: missing '(' after 'if'\n"));
                return false;
            }

            b32 good_parens = tv__read_cpp_parens(tokens, tok,
                                                  &tok_if_paren_open, &tok_if_paren_close,
                                                  &tok_inside_if_paren_first, &tok_inside_if_paren_last);
            if (!good_parens) {
                print_message(app, string_u8_litexpr("error parsing 'if' statement: failed to read parentheses after 'if'\n"));
                return false;
            }

            if (!tok_inside_if_paren_first || !tok_inside_if_paren_last) {
                print_message(app, string_u8_litexpr("error parsing 'if' statement: empty if condition parentheses\n"));
                return false;
            }

            tok = tv__token_inc(tokens, tok_if_paren_close); // skip ')' which closes if paren
        }

        Token *tok_if_body_stmt_first, *tok_if_body_stmt_last;
        String_Const_u8 err_text = {};
        b32 good_stmt = tv__read_cpp_statement(app, arena, buffer,
                                               tokens, tok,
                                               &tok_if_body_stmt_first, &tok_if_body_stmt_last,
                                               &err_text);
        if (!good_stmt) {
            print_message(app, string_u8_litexpr("error parsing 'if' statement body:\n"));
            print_message(app, string_u8_litexpr("   "));
            print_message(app, err_text);
            print_message(app, string_u8_litexpr("\n"));
            return false;
        }

        tok_last = tok_if_body_stmt_last;
        tok = tv__token_inc(tokens, tok_if_body_stmt_last); // skip last token of 'if' body statement

        TV_If_Statement_Case* ifc = push_array_zero(arena, TV_If_Statement_Case, 1);
        ifc->tok_condition_first = tok_inside_if_paren_first;
        ifc->tok_condition_last = tok_inside_if_paren_last;
        ifc->tok_body_first = tok_if_body_stmt_first;
        ifc->tok_body_last = tok_if_body_stmt_last;

        if (is_if) {
            if (!cases_tail) {
                ifst.ifs = ifc;
                cases_tail = ifc;
            } else {
                cases_tail->next = ifc;
                cases_tail = ifc;
            }
        } else {
            ifst.else_case = ifc;
            break;
        }

        is_first_if = false;
    }

    ifst.tok_last = tok_last;

    *out_if_statement = ifst;

    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct TV_Switch_Statement_Case {
    Token *tok_case;
    Token *tok_value_first, *tok_value_last;
    Token *tok_body_first, *tok_body_last;
    Token *tok_body_clean_first, *tok_body_clean_last;
    b32 is_empty;
    b32 is_default;

    TV_Switch_Statement_Case* next;
};

struct TV_Switch_Statement {
    Token* tok_switch;
    Token *tok_scope_open, *tok_scope_close;
    Token *tok_condition_first, *tok_condition_last;

    TV_Switch_Statement_Case* cases;
    TV_Switch_Statement_Case* default_case;
};

// cleanup 'case' body to get rid of 'break;' at the end, and/or '{' + '}'
// eg. "{ something; } break;" -> "something;"
internal void
tv__switch_case_body_cleanup_for_if(Application_Links *app, Arena* arena, Buffer_ID buffer, Token_Array* tokens,
                                    Token* in_tok_body_first, Token* in_tok_body_last,
                                    Token** out_new_tok_body_first, Token** out_new_tok_body_last,
                                    b32* out_is_empty)
{
    Token* tok_body_first = in_tok_body_first;
    Token* tok_body_last = in_tok_body_last;

    b32 removed_break = false;

    b32 did_a_thing;
    do {
        did_a_thing = false;

        if (tok_body_first && tok_body_first < tok_body_last) { // get rid of '{ }'
            if (tok_body_first->sub_kind == TokenCppKind_BraceOp && tok_body_last->sub_kind == TokenCppKind_BraceCl &&
                tv__tokens_are_nested(tokens, tok_body_first, tok_body_last)) {
                tok_body_first = tv__token_inc(tokens, tok_body_first);
                tok_body_last = tv__token_dec(tokens, tok_body_last);
                did_a_thing = true;
            }
        }

        if (tok_body_first && tok_body_first < tok_body_last) { // get rid of ending 'break;'
            if (tok_body_last->sub_kind == TokenCppKind_Semicolon) {
                Token* token_before_last = tv__token_dec(tokens, tok_body_last);
                if (tv__token_is_break_keyword(token_before_last)) {
                    tok_body_last = tv__token_dec(tokens, token_before_last);
                    removed_break = true;
                    did_a_thing = true;
                }
            }
        }
    } while (did_a_thing);

    if (tok_body_first && tok_body_first < tok_body_last) {
        *out_new_tok_body_first = tok_body_first;
        *out_new_tok_body_last = tok_body_last;
        *out_is_empty = false;
    } else {
        *out_new_tok_body_first = *out_new_tok_body_last = NULL;
        *out_is_empty = !removed_break;
    }
}

// return true if it is likely a good idea to put parentheses around an expression
internal b32
tv__should_use_parens_around_expression(Token_Array* tokens, Token* tok_first, Token* tok_last)
{
    if (tok_first == tok_last) {
        return false;
    }

    if (tok_first->kind == TokenBaseKind_ParentheticalOpen && tok_last->kind == TokenBaseKind_ParentheticalClose &&
        tv__tokens_are_nested(tokens, tok_first, tok_last)) {
        return false;
    }

    Token* tok = tok_first;
    for (;;) {
        if (tok->kind != TokenBaseKind_Identifier &&
            tok->kind != TokenBaseKind_Comment &&
            tok->kind != TokenBaseKind_Whitespace &&
            tok->kind != TokenBaseKind_LiteralInteger &&
            tok->kind != TokenBaseKind_LiteralFloat &&
            tok->kind != TokenBaseKind_LiteralString &&
            tok->kind != TokenBaseKind_LiteralInteger &&
            tok->sub_kind != TokenCppKind_Arrow &&
            tok->sub_kind != TokenCppKind_ArrowStar &&
            tok->sub_kind != TokenCppKind_Dot &&
            tok->sub_kind != TokenCppKind_DotStar &&
            tok->sub_kind != TokenCppKind_Star &&
            tok->sub_kind != TokenCppKind_ColonColon &&
            tok->sub_kind != TokenCppKind_BrackOp &&
            tok->sub_kind != TokenCppKind_BrackCl &&
            tok->sub_kind != TokenCppKind_Plus &&
            tok->sub_kind != TokenCppKind_Minus &&
            tok->sub_kind != TokenCppKind_Div &&
            tok->sub_kind != TokenCppKind_Mod)
        {
            return true;
        } else if (tok >= tok_last) {
            break;
        } else {
            tok = tv__token_inc(tokens, tok);
        }
    }

    return false;
}

internal b32
tv__switch_case_is_fallthrough_to_default(TV_Switch_Statement* swst, TV_Switch_Statement_Case* scase)
{
    if (swst->default_case) {
        while (scase) {
            if (scase->is_default) {
                return true;
            }
            if (!scase->is_empty) {
                return false;
            }
            scase = scase->next;
        }
    }
    return false;
}

internal b32
tv__find_switch_at_position(Application_Links* app, i64 pos, Token_Array* tokens,
                            b32 report_error,
                            Token** out_tok_switch_keyword,
                            Token** out_tok_condition_first, Token** out_tok_condition_last,
                            Token** out_tok_scope_open, Token** out_tok_scope_close)
{
    Token* tok = token_from_pos(tokens, pos);

    // find 'switch' keyword
    {
        i32 level = 0;
        while (tok) {
            if (level <= 0 && tv__token_is_switch_keyword(tok)) {
                break;
            } else if (tok->kind == TokenBaseKind_ScopeClose) {
                level++;
            } else if (tok->kind == TokenBaseKind_ScopeOpen) {
                level--;
            }
            tok = tv__token_dec(tokens, tok);
        }
    }

    if (!tok) {
        if (report_error) {
            print_message(app, string_u8_litexpr("error parsing switch statement: cursor not over switch statement\n"));
        }
        return false;
    }

    Token* tok_switch_keyword = tok;
    if (out_tok_switch_keyword) *out_tok_switch_keyword = tok_switch_keyword;

    tok = tv__token_inc(tokens, tok); // skip the switch keyword

    Token *tok_switch_paren_open, *tok_switch_paren_close;
    Token *tok_inside_switch_paren_first, *tok_inside_switch_paren_last;
    b32 good_parens = tv__read_cpp_parens(tokens, tok,
                                          &tok_switch_paren_open, &tok_switch_paren_close,
                                          &tok_inside_switch_paren_first, &tok_inside_switch_paren_last);
    if (!good_parens) {
        print_message(app, string_u8_litexpr("error parsing switch statement: failed to read parentheses after 'switch'\n"));
        return false;
    }

    if (out_tok_condition_first) *out_tok_condition_first = tok_inside_switch_paren_first;
    if (out_tok_condition_last) *out_tok_condition_last = tok_inside_switch_paren_last;

    tok = tv__token_inc(tokens, tok_switch_paren_close); // skip ')' which closes switch paren

    Token *tok_switch_scope_open, *tok_switch_scope_close;
    b32 good_scope = tv__read_cpp_scope(tokens, tok,
                                        &tok_switch_scope_open, &tok_switch_scope_close,
                                        NULL, NULL);
    if (!good_scope) {
        if (report_error) {
            print_message(app, string_u8_litexpr("error parsing switch statement: failed to read scope of switch statement\n"));
        }
        return false;
    }

    if (pos < tok_switch_keyword->pos || pos > tok_switch_scope_close->pos+1) {
        if (report_error) {
            print_message(app, string_u8_litexpr("error parsing switch statement: cursor not over switch statement\n"));
        }
        return false;
    }

    if (out_tok_scope_open) *out_tok_scope_open = tok_switch_scope_open;
    if (out_tok_scope_close) *out_tok_scope_close = tok_switch_scope_close;

    return true;
}

function b32
tv_parse_switch(Application_Links* app, Arena* arena, Buffer_ID buffer, i64 pos, Token_Array* tokens,
                TV_Switch_Statement* out_switch_statement)
{
    TV_Switch_Statement swst = {};
    TV_Switch_Statement_Case* cases_tail = NULL;


    b32 found_switch = tv__find_switch_at_position(app, pos, tokens, true,
                                                       &swst.tok_switch,
                                                       &swst.tok_condition_first, &swst.tok_condition_last,
                                                       &swst.tok_scope_open, &swst.tok_scope_close);
    if (!found_switch) {
        return false;
    }
    if (swst.tok_condition_first == NULL || swst.tok_condition_last == NULL) {
        print_message(app, string_u8_litexpr("error parsing switch statement: empty switch condition parentheses\n"));
        return false;
    }

    // now start looking for case/default statements
    Token* tok = tv__token_inc(tokens, swst.tok_scope_open);
    while (tok) {
        if (tok->kind == TokenBaseKind_ScopeClose) {
            // note: this kind of has to be switch_scope_close, because we do balanced skips of all nests
            break;
        }

        b32 is_case = tv__token_is_case_keyword(tok);
        b32 is_default = tv__token_is_default_keyword(tok);
        b32 valid_in_switch = false;
        if (is_case || is_default) {
            valid_in_switch = true;

            if (is_default && swst.default_case) {
                print_message(app, string_u8_litexpr("error parsing switch statement: multiple 'default' cases\n"));
                return false;
            }

            Token* tok_case = tok;

            tok = tv__token_inc(tokens, tok); // skip keyword

            Token* case_value_first = NULL;
            Token* case_value_last = NULL;

            if (is_case) {
                case_value_first = tok;
                if (!tok || tok->kind == TokenBaseKind_StatementClose) {
                    print_message(app, string_u8_litexpr("error parsing switch statement: 'case' is missing its value\n"));
                    return false;
                }
                case_value_last = case_value_first;
                while (tok) {
                    if (tok->kind == TokenBaseKind_ParentheticalOpen) {
                        tok = case_value_last = tv__skip_nest(tokens, tok);
                        tok = tv__token_inc(tokens, tok); // skip ')'
                    } else {
                        if (tok->kind == TokenBaseKind_StatementClose) { // colon
                            break;
                        } else {
                            case_value_last = tok;
                            tok = tv__token_inc(tokens, tok);
                        }
                    }
                }
                if (!tok) {
                    print_message(app, string_u8_litexpr("error parsing switch statement: missing ':' after case\n"));
                    return false;
                }
            } else {
                if (!tok || tok->kind != TokenBaseKind_StatementClose) {
                    print_message(app, string_u8_litexpr("error parsing switch statement: missing ':' after 'default'\n"));
                    return false;
                }
            }

            // read case body
            Token* case_body_first = tok = tv__token_inc(tokens, tok); // skip ':'
            if (!tok) {
                print_message(app, string_u8_litexpr("error parsing switch statement: unexpected end of buffer after 'case ...:'\n"));
                return false;
            }

            Token* case_body_last = case_body_first;
            while (tok) {
                if (tok->kind == TokenBaseKind_ParentheticalOpen || tok->kind == TokenBaseKind_ScopeOpen) {
                    tok = case_body_last = tv__skip_nest(tokens, tok);
                    tok = tv__token_inc(tokens, tok); // skip ')' or '}'
                } else {
                    if (tok->kind == TokenBaseKind_ScopeClose) { // '}', ending the switch statement
                        break;
                    } else if (tok->kind == TokenBaseKind_Keyword) {
                        Range_i64 keyword2_range = Ii64_size(tok->pos, tok->size);
                        String_Const_u8 keyword2_name = push_buffer_range(app, arena, buffer, keyword2_range);
                        if (string_match(keyword2_name, string_u8_litexpr("case")) ||
                            string_match(keyword2_name, string_u8_litexpr("default")))
                        {
                            break; // case/default keyword, ie. start of another case
                        }
                    }

                    case_body_last = tok;
                    tok = tv__token_inc(tokens, tok);
                }
            }

            if (case_body_first == tok) {
                case_body_first = case_body_last = NULL; // empty case
            }

            TV_Switch_Statement_Case* swc = push_array_zero(arena, TV_Switch_Statement_Case, 1);
            swc->tok_case = tok_case;
            swc->tok_value_first = case_value_first;
            swc->tok_value_last = case_value_last;
            swc->tok_body_first = case_body_first;
            swc->tok_body_last = case_body_last;
            swc->is_default = is_default;
            tv__switch_case_body_cleanup_for_if(app, arena, buffer, tokens,
                                                swc->tok_body_first, swc->tok_body_last,
                                                &swc->tok_body_clean_first, &swc->tok_body_clean_last,
                                                &swc->is_empty);
            if (is_default) {
                swst.default_case = swc;
            }

            if (!cases_tail) {
                swst.cases = swc;
                cases_tail = swc;
            } else {
                cases_tail->next = swc;
                cases_tail = swc;
            }
        } else if (tok->kind == TokenBaseKind_ScopeClose) {
            // end of switch statement
            if (tok != swst.tok_scope_close) {
                // mismatch between what we now think is the end of the switch statement, and what we thought was the end before
                print_message(app, string_u8_litexpr("error parsing switch statement: failed to understand switch statement\n"));
                return false;
            }
            break;
        }

        if (!valid_in_switch) {
            print_message(app, string_u8_litexpr("error parsing switch statement: unexpected token in switch scope\n"));
            return false;
        }
    }

    if (!tok) {
        print_message(app, string_u8_litexpr("error parsing switch statement: unexpected end of buffer in switch scope\n"));
        return false;
    }

    *out_switch_statement = swst;
    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CUSTOM_COMMAND_SIG(switch_to_if)
CUSTOM_DOC("Convert switch statement to if-elseif-else")
{
    View_ID view = get_active_view(app, Access_ReadVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
    i64 pos = view_get_cursor_pos(app, view);

    Token_Array tokens = get_token_array_from_buffer(app, buffer);
    if (tokens.tokens == 0) {
        return;
    }

    Scratch_Block scratch(app);

    TV_Switch_Statement swst;

    b32 good = tv_parse_switch(app, scratch, buffer, pos, &tokens, &swst);
    if (!good || !swst.cases) {
        return;
    }

    Range_i64 switch_range = tv__range_from_tokens(swst.tok_switch, swst.tok_scope_close);

    String_Const_u8 condition_string = tv__string_from_token_range(app, scratch, buffer, &tokens,
                                                                   swst.tok_condition_first, swst.tok_condition_last);
    b32 condition_needs_parens = tv__should_use_parens_around_expression(&tokens, swst.tok_condition_first, swst.tok_condition_last);

    u64 if_string_buffer_capacity = KB(64);
    u8* if_string_buffer = push_array(scratch, u8, if_string_buffer_capacity);
    String_u8 if_string = Su8(if_string_buffer, 0, if_string_buffer_capacity);

    b32 any_case_written = false;
    for (i32 phase = 0; phase < 2; phase++) { // 0 = cases; 1 = default case
        TV_Switch_Statement_Case* scase = swst.cases;
        while (scase) {
            b32 is_fallthrough_to_default = tv__switch_case_is_fallthrough_to_default(&swst, scase);
            b32 is_default = scase->is_default;

            if (phase == 0 && is_default && scase->is_empty) {
                // 'default:' label falls through
                do {
                    scase->is_default = false;
                    scase = scase->next;
                    if (scase) {
                        scase->is_default = true;
                    }
                } while (scase && scase->is_empty);

                continue;
            }

            if ((phase == 0 && is_fallthrough_to_default) ||
                (phase == 1 && !is_default))
            {
                scase = scase->next;
                continue;
            }

            if (any_case_written && !is_default) {
                string_append(&if_string, string_u8_litexpr(" else "));
            }
            b32 allow_single_line = true;
            if (!is_default) {
                string_append(&if_string, string_u8_litexpr("if ("));

                TV_Switch_Statement_Case* scase2 = scase;
                b32 first_or = true;
                b32 has_or = false;
                do {
                    if (!first_or) {
                        string_append(&if_string, string_u8_litexpr(" ||\n"));
                        has_or = true;
                        allow_single_line = false;
                    }

                    scase = scase2;
                    if (condition_needs_parens) {
                        string_append(&if_string, string_u8_litexpr("("));
                    }
                    string_append(&if_string, condition_string);
                    if (condition_needs_parens) {
                        string_append(&if_string, string_u8_litexpr(")"));
                    }
                    string_append(&if_string, string_u8_litexpr(" == "));
                    b32 value_needs_parens = tv__should_use_parens_around_expression(&tokens, scase->tok_value_first, scase->tok_value_last);
                    String_Const_u8 value_string = tv__string_from_token_range(app, scratch, buffer, &tokens,
                                                                               scase->tok_value_first, scase->tok_value_last);
                    if (value_needs_parens) {
                        string_append(&if_string, string_u8_litexpr("("));
                    }
                    string_append(&if_string, value_string);
                    if (value_needs_parens) {
                        string_append(&if_string, string_u8_litexpr(")"));
                    }
                    scase2 = scase->next;
                    first_or = false;
                } while (scase->is_empty && scase2);

                if (!has_or) {
                    string_append(&if_string, string_u8_litexpr(") "));
                } else {
                    string_append(&if_string, string_u8_litexpr(")\n"));
                }
            } else {
                if (any_case_written) {
                    string_append(&if_string, string_u8_litexpr(" else "));
                }
            }

            b32 single_line = false;
            if (allow_single_line && !scase->is_empty && scase->tok_body_last) {
                i64 case_line = get_line_number_from_pos(app, buffer, scase->tok_case->pos);
                i64 body_end_line = get_line_number_from_pos(app, buffer, scase->tok_body_last->pos);
                if (case_line == body_end_line) {
                    single_line = true;
                }
            }

            String_Const_u8 body_string = tv__string_from_token_range(app, scratch, buffer, &tokens,
                                                                      scase->tok_body_clean_first, scase->tok_body_clean_last);

            if (!single_line) {
                string_append(&if_string, string_u8_litexpr("{\n"));
            } else {
                string_append(&if_string, string_u8_litexpr("{"));
                if (body_string.size > 0) {
                    string_append(&if_string, string_u8_litexpr(" "));
                }
            }
            string_append(&if_string, body_string);
            if (!single_line) {
                string_append(&if_string, string_u8_litexpr("\n}"));
            } else {
                string_append(&if_string, string_u8_litexpr(" }\n"));
            }

            any_case_written = true;

            scase = scase->next;
        }
    }

    Range_i64 out_range = Ii64_size(switch_range.min, if_string.size);
    ARGB_Color argb = fcolor_resolve(fcolor_id(defcolor_paste));
    buffer_post_fade(app, buffer, 0.667f, out_range, argb);

    History_Group hg = history_group_begin(app, buffer);
    buffer_replace_range(app, buffer, switch_range, SCu8(if_string.str, if_string.size));
    auto_indent_buffer(app, buffer, out_range);
    history_group_end(hg);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct TV_If_To_Switch_Condition {
    Token *tok_lhs_first, *tok_lhs_last;
    Token *tok_rhs_first, *tok_rhs_last;
    TV_If_To_Switch_Condition* next_or;
};

internal TV_If_To_Switch_Condition*
tv__parse_if_condition_for_if_to_switch(Application_Links* app, Arena* arena, Token_Array* tokens,
                                        Token* tok_condition_first, Token* tok_condition_last)
{
    TV_If_To_Switch_Condition* ifconds = NULL;
    TV_If_To_Switch_Condition* ifconds_tail = NULL;

    Token* tok = tok_condition_first;
    if (!tok) {
        return NULL;
    }

    b32 reading_lhs = true;
    Token *lhs_first = NULL, *lhs_last = NULL;
    Token *rhs_first = NULL, *rhs_last = NULL;
    for (;;) {
        if (!tok || tok > tok_condition_last || tok->sub_kind == TokenCppKind_OrOr) {
            if (reading_lhs) {
                print_message(app, string_u8_litexpr("error converting if->switch: expecting <lhs> == <rhs> comparisons in condition expression\n"));
                return NULL;
            } else if (!rhs_first) {
                print_message(app, string_u8_litexpr("error converting if->switch: missing right-hand-side value for == comparison\n"));
                return NULL;
            }

            TV_If_To_Switch_Condition* ifcond = push_array_zero(arena, TV_If_To_Switch_Condition, 1);
            ifcond->tok_lhs_first = lhs_first;
            ifcond->tok_lhs_last = lhs_last;
            ifcond->tok_rhs_first = rhs_first;
            ifcond->tok_rhs_last = rhs_last;

            if (!ifconds) {
                ifconds = ifcond;
                ifconds_tail = ifcond;
            } else {
                ifconds_tail->next_or = ifcond;
                ifconds_tail = ifcond;
            }

            reading_lhs = true;
            lhs_first = lhs_last = NULL;
            rhs_first = rhs_last = NULL;

            if (!tok || tok > tok_condition_last) {
                break;
            } else {
                tok = tv__token_inc(tokens, tok); // skip '||'
                continue;
            }
        }

        if (tok->sub_kind == TokenCppKind_AndAnd) {
            print_message(app, string_u8_litexpr("error converting if->switch: condition must not contain '&&'\n"));
            return NULL;
        }

        if (reading_lhs && tok->sub_kind == TokenCppKind_EqEq) { // ==
            if (!lhs_first) {
                print_message(app, string_u8_litexpr("error converting if->switch: left-hand-side of 'if' == comparison is empty\n"));
                return NULL;
            }
            reading_lhs = false;
            tok = tv__token_inc(tokens, tok); // skip '=='
            continue;
        }

        if (reading_lhs && !lhs_first) {
            lhs_first = tok;
        } else if (!reading_lhs && !rhs_first) {
            rhs_first = tok;
        }

        if (tok->kind == TokenBaseKind_ParentheticalOpen || tok->kind == TokenBaseKind_ScopeOpen) {
            tok = tv__skip_nest(tokens, tok);
            if (reading_lhs) {
                lhs_last = tok;
            } else {
                rhs_last = tok;
            }
            if (!tok || tok > tok_condition_last) {
                print_message(app, string_u8_litexpr("error converting if->switch: unexpected unclosed nest in 'if' condition\n"));
                return NULL;
            }
            tok = tv__token_inc(tokens, tok); // skip ')' or '}' or ']'
        } else {
            if (reading_lhs) {
                lhs_last = tok;
            } else {
                rhs_last = tok;
            }
            tok = tv__token_inc(tokens, tok);
        }
    }

    if (!ifconds) {
        print_message(app, string_u8_litexpr("error converting if->switch: 'if' condition expression is empty\n"));
        return NULL;
    }

    return ifconds;
}

internal void
tv__tokens_remove_outer_parentheses(Token_Array* tokens, Token** inout_tok_first, Token** inout_tok_last)
{
    if (!inout_tok_first || !*inout_tok_first || !inout_tok_last || !*inout_tok_last) {
        return;
    }

    Token* tok_first = *inout_tok_first;
    Token* tok_last = *inout_tok_last;

    while ((tok_first->kind == TokenBaseKind_ParentheticalOpen && tok_last->kind == TokenBaseKind_ParentheticalClose) &&
           tv__tokens_are_nested(tokens, tok_first, tok_last))
    {
        Token* first = tv__token_inc(tokens, tok_first);
        Token* last = tv__token_dec(tokens, tok_last);
        if (first <= last) {
            tok_first = first;
            tok_last = last;
        } else {
            break;
        }
    }

    *inout_tok_first = tok_first;
    *inout_tok_last = tok_last;
}

internal b32
tv__tokens_are_only_braces(Token_Array* tokens, Token* tok_first, Token* tok_last)
{
    Token* tok_after_first = tv__token_inc(tokens, tok_first);
    return (tok_first && tok_last && tok_last == tok_after_first &&
            tok_first->kind == TokenBaseKind_ScopeOpen && tok_last->kind == TokenBaseKind_ScopeClose);
}

internal b32
tv__check_token_expressions_equal(Application_Links* app, Arena* arena, Buffer_ID buffer, Token_Array* tokens,
                                  Token* tok_a_first, Token* tok_a_last,
                                  Token* tok_b_first, Token* tok_b_last)
{
    tv__tokens_remove_outer_parentheses(tokens, &tok_a_first, &tok_a_last);
    tv__tokens_remove_outer_parentheses(tokens, &tok_b_first, &tok_b_last);

    Token* tok_a = tok_a_first;
    Token* tok_b = tok_b_first;
    for (;;) {
        if (!tok_a || !tok_b || tok_a > tok_a_last || tok_b > tok_b_last) {
            return (tok_a >= tok_a_last && tok_b >= tok_b_last);
        }

        if (tok_a->kind != tok_b->kind ||
            tok_a->sub_kind != tok_b->sub_kind ||
            tok_a->sub_flags != tok_b->sub_flags ||
            tok_a->size != tok_b->size)
        {
            return false;
        }

        if (tok_a->size > 0) {
            String_Const_u8 sa = tv__string_from_token(app, arena, buffer, tok_a);
            String_Const_u8 sb = tv__string_from_token(app, arena, buffer, tok_b);
            if (!string_match(sa, sb)) {
                return false;
            }
        }

        tok_a = tv__token_inc(tokens, tok_a);
        tok_b = tv__token_inc(tokens, tok_b);
    }
}

CUSTOM_COMMAND_SIG(if_to_switch)
CUSTOM_DOC("Convert if statement to switch")
{
    View_ID view = get_active_view(app, Access_ReadVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
    i64 pos = view_get_cursor_pos(app, view);

    Token_Array tokens = get_token_array_from_buffer(app, buffer);
    if (tokens.tokens == 0) {
        return;
    }

    Scratch_Block scratch(app);

    TV_If_Statement ifst = {};

    b32 good = tv_parse_if(app, scratch, buffer, pos, &tokens, &ifst);
    if (!good || !ifst.ifs) {
        return;
    }

    Range_i64 if_range = tv__range_from_tokens(ifst.tok_first_if, ifst.tok_last);

    u64 switch_string_buffer_capacity = KB(64);
    u8* switch_string_buffer = push_array(scratch, u8, switch_string_buffer_capacity);
    String_u8 switch_string = Su8(switch_string_buffer, 0, switch_string_buffer_capacity);

    TV_If_To_Switch_Condition* first_cond = tv__parse_if_condition_for_if_to_switch(app, scratch, &tokens,
                                                                                    ifst.ifs->tok_condition_first, ifst.ifs->tok_condition_last);
    if (!first_cond) {
        return;
    }

    // figure out which expression belongs in the 'switch()' condition
    // (in order to support "if (x == 0 || x == 1)" as well as "if (0 == x || 1 == x)")
    Token *switch_cond_first, *switch_cond_last;
    {
        b32 lhs_ok = true, rhs_ok = true;
        for (TV_If_Statement_Case* ifc = ifst.ifs; ifc; ifc = ifc->next) {
            // note: tv__parse_if_condition_for_if_to_switch is called once here, and once in the emitting loop, which is a bit
            //        inefficient, but it probably doesn't matter at all
            TV_If_To_Switch_Condition* cond = tv__parse_if_condition_for_if_to_switch(app, scratch, &tokens,
                                                                                      ifc->tok_condition_first, ifc->tok_condition_last);
            if (!cond) {
                return;
            }
            for ( ; cond; cond = cond->next_or) {
                if (lhs_ok) {
                    if (!tv__check_token_expressions_equal(app, scratch, buffer, &tokens,
                                                           first_cond->tok_lhs_first, first_cond->tok_lhs_last,
                                                           cond->tok_lhs_first, cond->tok_lhs_last) &&
                        !tv__check_token_expressions_equal(app, scratch, buffer, &tokens,
                                                           first_cond->tok_lhs_first, first_cond->tok_lhs_last,
                                                           cond->tok_rhs_first, cond->tok_rhs_last))
                    {
                        lhs_ok = false;
                    }
                }

                if (rhs_ok) {
                    if (!tv__check_token_expressions_equal(app, scratch, buffer, &tokens,
                                                           first_cond->tok_rhs_first, first_cond->tok_rhs_last,
                                                           cond->tok_lhs_first, cond->tok_lhs_last) &&
                        !tv__check_token_expressions_equal(app, scratch, buffer, &tokens,
                                                           first_cond->tok_rhs_first, first_cond->tok_rhs_last,
                                                           cond->tok_rhs_first, cond->tok_rhs_last))
                    {
                        rhs_ok = false;
                    }
                }

                if (!lhs_ok && !rhs_ok) {
                    print_message(app, string_u8_litexpr("error converting if->switch: conditions are not convertible to a switch statement\n"));
                    return;
                }
            }
        }

        if (lhs_ok) {
            switch_cond_first = first_cond->tok_lhs_first;
            switch_cond_last = first_cond->tok_lhs_last;
        } else {
            switch_cond_first = first_cond->tok_rhs_first;
            switch_cond_last = first_cond->tok_rhs_last;
        }

        tv__tokens_remove_outer_parentheses(&tokens, &switch_cond_first, &switch_cond_last);
    }

    // write 'switch (...) {'
    {
        string_append(&switch_string, string_u8_litexpr("switch ("));
        String_Const_u8 cond_string;
        cond_string = tv__string_from_token_range(app, scratch, buffer, &tokens,
                                                  switch_cond_first, switch_cond_last);
        string_append(&switch_string, cond_string);
        string_append(&switch_string, string_u8_litexpr(") {\n"));
    }

    TV_If_Statement_Case* ifc = ifst.ifs;
    for (;;) {
        if (!ifc) ifc = ifst.else_case;
        if (!ifc) break;

        String_Const_u8 body_string = {};
        if (!tv__tokens_are_only_braces(&tokens, ifc->tok_body_first, ifc->tok_body_last)) {
            body_string = tv__string_from_token_range(app, scratch, buffer, &tokens,
                                                      ifc->tok_body_first, ifc->tok_body_last);
        }

        if (ifc != ifst.else_case) {
            TV_If_To_Switch_Condition* cond = tv__parse_if_condition_for_if_to_switch(app, scratch, &tokens,
                                                                                      ifc->tok_condition_first, ifc->tok_condition_last);
            for ( ; cond; cond = cond->next_or) {
                string_append(&switch_string, string_u8_litexpr("case "));
                String_Const_u8 cond_string;

                if (tv__check_token_expressions_equal(app, scratch, buffer, &tokens,
                                                      switch_cond_first, switch_cond_last,
                                                      cond->tok_lhs_first, cond->tok_lhs_last))
                {
                    cond_string = tv__string_from_token_range(app, scratch, buffer, &tokens,
                                                              cond->tok_rhs_first, cond->tok_rhs_last);
                } else {
                    cond_string = tv__string_from_token_range(app, scratch, buffer, &tokens,
                                                              cond->tok_lhs_first, cond->tok_lhs_last);
                }

                string_append(&switch_string, cond_string);
                if (cond->next_or) {
                    string_append(&switch_string, string_u8_litexpr(": /* fallthrough */\n"));
                } else {
                    string_append(&switch_string, string_u8_litexpr(":"));
                    if (body_string.size > 0) {
                        string_append(&switch_string, string_u8_litexpr(" "));
                        string_append(&switch_string, body_string);
                    }
                    string_append(&switch_string, string_u8_litexpr(" break;\n"));
                }
            }
        } else {
            string_append(&switch_string, string_u8_litexpr("default:"));
            if (body_string.size > 0) {
                string_append(&switch_string, string_u8_litexpr(" "));
                string_append(&switch_string, body_string);
            }
            string_append(&switch_string, string_u8_litexpr(" break;\n"));
            break;
        }

        ifc = ifc->next;
    }

    string_append(&switch_string, string_u8_litexpr("}"));

    Range_i64 out_range = Ii64_size(if_range.min, switch_string.size);
    ARGB_Color argb = fcolor_resolve(fcolor_id(defcolor_paste));
    buffer_post_fade(app, buffer, 0.667f, out_range, argb);

    History_Group hg = history_group_begin(app, buffer);
    buffer_replace_range(app, buffer, if_range, SCu8(switch_string.str, switch_string.size));
    auto_indent_buffer(app, buffer, out_range);
    history_group_end(hg);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CUSTOM_COMMAND_SIG(if_switch_toggle)
CUSTOM_DOC("Toggle if<->switch")
{
    View_ID view = get_active_view(app, Access_ReadVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
    i64 pos = view_get_cursor_pos(app, view);

    Token_Array tokens = get_token_array_from_buffer(app, buffer);
    if (tokens.tokens == 0) {
        return;
    }

    Scratch_Block scratch(app);

    Token* tok_switch = NULL;
    b32 found_switch = tv__find_switch_at_position(app, pos, &tokens, false, &tok_switch, NULL, NULL, NULL, NULL);

    Token* tok_if = NULL;
    b32 found_if = tv__find_if_at_position(app, scratch, buffer, pos, &tokens, false, &tok_if);

    if (found_switch && !found_if) {
        switch_to_if(app);
    } else if (found_if && !found_switch) {
        if_to_switch(app);
    } else if (found_if && found_switch && tok_switch && tok_if) {
        if (pos - tok_switch->pos < pos - tok_if->pos) {
            switch_to_if(app);
        } else {
            if_to_switch(app);
        }
    }
}
