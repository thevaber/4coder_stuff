#define TV_4CODER_HAS_UTILS_CPP 1

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

internal Token*
tv__token_inc_all(Token_Array* token_array, Token* token)
{
    if (!token) {
        return NULL;
    }
    
    i64 new_index = (token - token_array->tokens) + 1;
    if (new_index < token_array->count) {
        return &token_array->tokens[new_index];
    } else {
        return NULL;
    }
}

internal Token*
tv__token_inc(Token_Array* token_array, Token* token)
{
    Token* token2 = tv__token_inc_all(token_array, token);
    while (token2 && (token2->kind == TokenBaseKind_Whitespace || token2->kind == TokenBaseKind_Comment)) {
        token2 = tv__token_inc_all(token_array, token2);
    }
    return token2;
}

internal Token*
tv__token_dec_all(Token_Array* token_array, Token* token)
{
    if (!token) {
        return NULL;
    }
    
    i64 new_index = (token - token_array->tokens) - 1;
    if (new_index >= 0) {
        return &token_array->tokens[new_index];
    } else {
        return NULL;
    }
}

internal Token*
tv__token_dec(Token_Array* token_array, Token* token)
{
    Token* token2 = tv__token_dec_all(token_array, token);
    while (token2 && (token2->kind == TokenBaseKind_Whitespace || token2->kind == TokenBaseKind_Comment)) {
        token2 = tv__token_dec_all(token_array, token2);
    }
    return token2;
}

internal Token*
tv__skip_nest(Token_Array* token_array, Token* open_token)
{
    Token_Base_Kind open_kind = open_token->kind;
    Token_Base_Kind close_kind;
    if (open_kind == TokenBaseKind_ScopeOpen) close_kind = TokenBaseKind_ScopeClose;
    else if (open_kind == TokenBaseKind_ParentheticalOpen) close_kind = TokenBaseKind_ParentheticalClose;
    else return NULL; // not a nest
    
    Token* tok = tv__token_inc(token_array, open_token);
    i32 level = 0;
    while (tok) {
        if (tok->kind == close_kind) {
            if (level == 0) {
                return tok;
            } else {
                level--;
            }
        } else if (tok->kind == open_kind) {
            level++;
        }
        tok = tv__token_inc(token_array, tok);
    }
    
    return NULL;
}

internal b32
tv__tokens_are_nested(Token_Array* token_array, Token* tok_first, Token* tok_last)
{
    if (!tok_first || !tok_last || tok_first >= tok_last) {
        return false;
    }
    if (!(tok_first->kind == TokenBaseKind_ScopeOpen && tok_last->kind == TokenBaseKind_ScopeClose) &&
        !(tok_first->kind == TokenBaseKind_ParentheticalOpen && tok_last->kind == TokenBaseKind_ParentheticalClose))
    {
        return false;
    }
    
    Token* tok = tv__skip_nest(token_array, tok_first);
    return (tok == tok_last);
}

internal Range_i64
tv__range_from_tokens(Token* start, Token* end)
{
    if (start && end) {
        return Ii64(start->pos, end->pos + end->size);
    } else {
        return Ii64(0,0);
    }
}

internal String_Const_u8
tv__string_from_token(Application_Links* app, Arena* arena, Buffer_ID buffer,
                      Token* tok)
{
    if (tok) {
        Range_i64 token_range = Ii64_size(tok->pos, tok->size);
        String_Const_u8 token_string = push_buffer_range(app, arena, buffer, token_range);
        return token_string;
    } else {
        return String_Const_u8();
    }
}

internal String_Const_u8
tv__string_from_token_range(Application_Links* app, Arena* arena, Buffer_ID buffer,
                            Token_Array* tokens, Token* tok_first, Token* tok_last)
{
    if (tok_first && tok_last) {
        Range_i64 tokens_range = tv__range_from_tokens(tok_first, tok_last);
        String_Const_u8 tokens_string = push_buffer_range(app, arena, buffer, tokens_range);
        return tokens_string;
    } else {
        return String_Const_u8();
    }
}

internal b32
tv__token_check_string(Application_Links *app, Arena* arena, Buffer_ID buffer, Token* tok, String_Const_u8 wanted_text)
{
    if (!tok) {
        return false;
    }
    Range_i64 keyword_range = Ii64_size(tok->pos, tok->size);
    String_Const_u8 keyword_name = push_buffer_range(app, arena, buffer, keyword_range);
    return string_match(keyword_name, wanted_text);
}

internal b32
tv__token_is_keyword_or_identifier(Application_Links *app, Arena* arena, Buffer_ID buffer, Token* tok, String_Const_u8 wanted_keyword_name)
{
    if (!tok || (tok->kind != TokenBaseKind_Keyword && tok->kind != TokenBaseKind_Identifier)) {
        return false;
    }
    return tv__token_check_string(app, arena, buffer, tok, wanted_keyword_name);
}

internal b32
tv__token_is_switch_keyword(Token* tok)
{
    return (tok && tok->kind == TokenBaseKind_Keyword && tok->sub_kind == TokenCppKind_Switch);
}

internal b32
tv__token_is_case_keyword(Token* tok)
{
    return (tok && tok->kind == TokenBaseKind_Keyword && tok->sub_kind == TokenCppKind_Case);
}

internal b32
tv__token_is_default_keyword(Token* tok)
{
    return (tok && tok->kind == TokenBaseKind_Keyword && tok->sub_kind == TokenCppKind_Default);
}

internal b32
tv__token_is_break_keyword(Token* tok)
{
    return (tok && tok->kind == TokenBaseKind_Keyword && tok->sub_kind == TokenCppKind_Break);
}

internal b32
tv__token_is_if_keyword(Token* tok)
{
    return (tok && tok->kind == TokenBaseKind_Keyword && tok->sub_kind == TokenCppKind_If);
}

internal b32
tv__token_is_else_keyword(Token* tok)
{
    return (tok && tok->kind == TokenBaseKind_Keyword && tok->sub_kind == TokenCppKind_Else);
}

internal b32
tv__token_is_do_keyword(Token* tok)
{
    return (tok && tok->kind == TokenBaseKind_Keyword && tok->sub_kind == TokenCppKind_Do);
}

internal b32
tv__token_is_while_keyword(Token* tok)
{
    return (tok && tok->kind == TokenBaseKind_Keyword && tok->sub_kind == TokenCppKind_While);
}

internal b32
tv__token_is_for_keyword(Token* tok)
{
    return (tok && tok->kind == TokenBaseKind_Keyword && tok->sub_kind == TokenCppKind_For);
}

internal b32
tv__token_is_constexpr_keyword(Application_Links *app, Arena* arena, Buffer_ID buffer, Token* tok)
{
    return tv__token_is_keyword_or_identifier(app, arena, buffer, tok, string_u8_litexpr("constexpr"));
}
