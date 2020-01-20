// Commands/utilities for working with integer literals
//  - Adjusting (doing math on) integer literals in any C/C++ number base/radix, including 0b1010 binary
//    - Increment, decrement, +, -, *, /, %
//  - Cycling through number bases (eg. 10 -> 0xA -> 0b1010 -> 012 -> 10 -> ...)
//  - Some utility functions for work with C/C++ integer literals

///////////////
// Config

// - if true, tv_check_integer_literal_at_position will not accept numbers that look like floating point numbers
// - if false, number such as 123.456 will be treated as two separate integer literals (123 and 456);
//    "456f" in 123.456f won't be accepted as an integer literal though, because of the 'f' suffix
#define TV_INTEGER_LITERAL_DONT_ACCEPT_FLOATS 0

#define TV_INTEGER_LITERAL_BASE_CYCLE_BINARY 1 // true if integer_base_cycle should include binary (0b1010) integer literals
#define TV_INTEGER_LITERAL_BASE_CYCLE_OCTAL 1 // true if integer_base_cycle should include octal (0567) integer literals

///////////////

function b32
tv_is_hex_digit(char c)
{
    return ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'));
}

function b32
tv_is_digit_of_base(char c, u32 radix)
{
    if (radix == 2) return (c == '0' || c == '1');
    else if (radix == 8) return (c >= '0' && c <= '7');
    else if (radix == 10) return (c >= '0' && c <= '9');
    else return tv_is_hex_digit(c);
}

internal b32
tv__helper_check_integer_literal_doesnt_overflow(String_Const_u8 number_text, u32 radix)
{
    // rather ridiculous way of checking whether a number (as string) doesn't overflow 64-bit unsigned limit
    // (note: this also forbids literals that are overly padded, eg. 0x0000000000000000FFFFFFFFFFFFFFFF)
    
    if (radix == 2) return number_text.size <= 64;
    else if (radix == 8) {
        if (number_text.size > 22) return false;
        if (number_text.size < 22) return true;
        return (number_text.str[0] == '0' || number_text.str[0] == '1');
    }
    else if (radix == 10) {
        String_Const_u8 maximum = string_u8_litexpr("18446744073709551615");
        
        if (number_text.size < maximum.size) return true;
        else if (number_text.size > maximum.size) return false;
    
        for (u64 i = 0; i < maximum.size; i++) {
            if (number_text.str[i] < maximum.str[i]) return true;
            if (number_text.str[i] > maximum.str[i]) return false;
        }
        
        return true;
    }
    /*else if (radix == 16)*/ return number_text.size <= 16;
}

internal Range_i64
tv__helper_check_integer_literal_starting_at_position(Application_Links* app, Buffer_ID buffer, i64 start, u32* out_radix)
{
    *out_radix = 10;
    
    i64 start2 = start;
    i64 buffer_end = buffer_get_size(app, buffer);
    char c = buffer_get_char(app, buffer, start);
    if (c != '0') {
        // decimal
    } else if (start + 1 >= buffer_end) {
        // just 0
        return Ii64_size(start, 1);
    } else {
        char c2 = buffer_get_char(app, buffer, start + 1);
        if (c2 == 'x' || c2 == 'X' || c2 == 'b' || c2 == 'B') {
            if (start + 2 >= buffer_end) {
                // 0x or 0b at the end of buffer
                return Ii64_size(start, 1);
            } else {
                if (c2 == 'x' || c2 == 'X') {
                    // hexadecimal
                    *out_radix = 16;
                } else {
                    // binary
                    *out_radix = 2;
                }
                start2 = start + 2;
            }
        } else if (c2 >= '0' && c2 <= '7') {
            // octal
            *out_radix = 8;
            start2 = start + 1;
        }
#if TV_INTEGER_LITERAL_DONT_ACCEPT_FLOATS
        else if (c2 == '.') {
            // looks like floating point number
            return Ii64_size(start, 0); // don't accept
        }
#endif
        else {
            // 0<garbage>
            return Ii64_size(start, 1);
        }
    }
    
    i64 i;
    for (i = start2; i < buffer_end; i++) {
        c = buffer_get_char(app, buffer, i);
#if TV_INTEGER_LITERAL_DONT_ACCEPT_FLOATS
        if (c == '.') {
            // looks like floating point number
            return Ii64_size(start, 0); // don't accept
        }
#endif
        if (!tv_is_digit_of_base(c, *out_radix)) {
            if (tv_is_hex_digit(c)) {
                // looks weird, eg. 100FF, or 078, or 0b0101012
                return Ii64_size(start, 0); // don't accept
            }
            break;
        }
    }
    
    return Ii64(start2, i);
}

// for 0x123, literal_range is the full "0x123", number_range is just the "123" part
function b32
tv_check_integer_literal_at_position(Application_Links* app, Buffer_ID buffer, i64 pos,
                                     Range_i64* out_literal_range, Range_i64* out_number_range, u32* out_radix)
{
    // scan left to find start of a potential integer literal
    i64 start_of_number = pos;
    for (i64 i = pos; ; ) {
        char c = buffer_get_char(app, buffer, i);
        if (c == 'x' || c == 'X' || c == 'b' || c == 'B') {
            if (i != 0) {
                char c2 = buffer_get_char(app, buffer, i - 1);
                if (c2 == '0') {
                    start_of_number = i - 1;
                    break;
                }
            }
            // invalid boundary (<something>x)
            return false;
        }
#if TV_INTEGER_LITERAL_DONT_ACCEPT_FLOATS
        else if (c == '.') {
            // looks like floating point number
            return false;
        }
#endif
        else if (tv_is_hex_digit(c)) {
            if (i == 0) {
                // reached start of buffer
                start_of_number = i;
                break;
            }
            i -= 1;
        } else {
            if (c == '_' || character_is_alpha(c)) {
                // looks like part of identifier
                return false;
            } else {
                // reached some non-alpha boundary
                start_of_number = i + 1;
                break;
            }
        }
    }
    if (start_of_number > pos) {
        return false; // not an integer literal
    }
    
    // now try to read the integer literal from its start
    u32 radix;
    Range_i64 number_range = tv__helper_check_integer_literal_starting_at_position(app, buffer, start_of_number, &radix);
    i64 number_length = number_range.max - number_range.min;
    if (number_length <= 0) {
        return false; // not an integer literal
    }
    
    if (out_radix) *out_radix = radix;
    if (out_literal_range) *out_literal_range = Ii64(start_of_number, number_range.max);
    if (out_number_range) *out_number_range = number_range;
    
    return true;
}

function b32
tv_read_integer_literal(Application_Links* app, Buffer_ID buffer, i64 pos,
                        u64* out_number, Range_i64* out_literal_range, Range_i64* out_number_range, u32* out_radix)
{
    u32 radix;
    Range_i64 number_range;
    b32 is_integer_literal = tv_check_integer_literal_at_position(app, buffer, pos,
                                                                  out_literal_range, &number_range, &radix);
    if (!is_integer_literal) {
        return false;
    }
    
    Scratch_Block scratch(app);
    String_Const_u8 number_text = push_buffer_range(app, scratch, buffer, number_range);
    
    b32 overflows = !tv__helper_check_integer_literal_doesnt_overflow(number_text, radix);
    if (overflows) {
        return false; // not safe to convert to u64
    }
    
    u64 x = string_to_integer(number_text, radix);
    
    if (out_number) *out_number = x;
    if (out_number_range) *out_number_range = number_range;
    if (out_radix) *out_radix = radix;
    
    return true;
}

internal u64 tv__next_power_of_two(u64 x)
{
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x |= x >> 32;
    x++;
    return x;
}

// pad_length < 0 -> auto padding to power of two lengths, eg. 0xABCDE -> 0x000ABCDE
function String_Const_u8
tv_integer_to_literal(Application_Links* app, Arena* arena, u64 x, u32 radix, i32 pad_length)
{
    char buf[256];
    
    if (radix == 2 && pad_length > 64) pad_length = 64;
    else if (radix == 8 && pad_length > 22) pad_length = 22;
    else if (radix == 16 && pad_length > 16) pad_length = 16;
    
    u64 len = 0;
    if (radix == 16) { // hexadecimal
        do {
            int d = x % 16;
            if (d < 10) buf[255 - len++] = '0' + (char)d;
            else buf[255 - len++] = 'A' + (char)d - 10;
            x /= 16;
        } while (x > 0);
        
        if (pad_length < 0) {
            pad_length = (i32)tv__next_power_of_two(len);
        }
        if (len < (u64)pad_length) {
            u64 n = (u64)pad_length - len;
            for (u64 i = 0; i < n; i++) buf[255 - len++] = '0';
        }
        
        buf[255 - len++] = 'x';
        buf[255 - len++] = '0';
    } else if (radix == 8) { // octal
        do {
            buf[255 - len++] = '0' + (x % 8);
            x /= 8;
        } while (x > 0);
        
        if (pad_length > 1 && len < (u64)pad_length) {
            u64 n = (u64)pad_length - len;
            for (u64 i = 0; i < n; i++) buf[255 - len++] = '0';
        }
        
        buf[255 - len++] = '0';
    } else if (radix == 2) { // binary
        do {
            buf[255 - len++] = '0' + (x % 2);
            x /= 2;
        } while (x > 0);
        
        if (pad_length < 0) {
            pad_length = (i32)tv__next_power_of_two(len);
        }
        if (len < (u64)pad_length) {
            u64 n = (u64)pad_length - len;
            for (u64 i = 0; i < n; i++) buf[255 - len++] = '0';
        }
        
        buf[255 - len++] = 'b';
        buf[255 - len++] = '0';
    } else { // decimal
        do {
            buf[255 - len++] = '0' + (x % 10);
            x /= 10;
        } while (x > 0);
    }
    
    String_Const_u8 new_text = SCu8(buf + 256 - len, len);
    return push_string_copy(arena, new_text);
}

function void
tv_change_integer_literal_base(Application_Links* app, u32 target_radix)
{
    View_ID view = get_active_view(app, Access_ReadWriteVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWriteVisible);
    i64 cursor_pos = view_get_cursor_pos(app, view);
    
    // check for integer literal at cursor position
    u32 source_radix;
    Range_i64 literal_range;
    u64 x;
    b32 is_integer_literal = tv_read_integer_literal(app, buffer, cursor_pos,
                                                     &x, &literal_range, NULL, &source_radix);
    if (!is_integer_literal) {
        return;
    }
    
    if (target_radix == 0) { // target base when cycling (2->8->10->16->2->...)
        if (source_radix == 2) target_radix = 8;
        else if (source_radix == 8) target_radix = 10;
        else if (source_radix == 10) target_radix = 16;
        else if (source_radix == 16) target_radix = 2;
        
#if !TV_INTEGER_LITERAL_BASE_CYCLE_BINARY
        if (target_radix == 2) target_radix = 8;
#endif
        
#if !TV_INTEGER_LITERAL_BASE_CYCLE_OCTAL
        if (target_radix == 8) target_radix = 10;
#endif
    }
    
    // convert to the wanted target base
    Scratch_Block scratch(app);
    String_Const_u8 new_text = tv_integer_to_literal(app, scratch, x, target_radix, -1);
    
    // finish
    buffer_replace_range(app, buffer, literal_range, new_text);
}

//////

enum {
    TV_Integer_Literal_Adjust__Noop,
    TV_Integer_Literal_Adjust__Add,
    TV_Integer_Literal_Adjust__Subtract,
    TV_Integer_Literal_Adjust__Multiply,
    TV_Integer_Literal_Adjust__Divide,
    TV_Integer_Literal_Adjust__Mod,
};

function void
tv_integer_literal_adjust(Application_Links* app, i32 operation, i64 d)
{
    if (operation == TV_Integer_Literal_Adjust__Noop) {
        return;
    }
    
    if (operation == TV_Integer_Literal_Adjust__Subtract) {
        operation = TV_Integer_Literal_Adjust__Add;
        d = -d;
    }
    
    View_ID view = get_active_view(app, Access_ReadWriteVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWriteVisible);
    i64 cursor_pos = view_get_cursor_pos(app, view);
        
    // check for integer literal at cursor position
    u32 radix;
    Range_i64 literal_range, number_range;
    u64 x;
    b32 is_integer_literal = tv_read_integer_literal(app, buffer, cursor_pos,
                                                     &x, &literal_range, &number_range, &radix);
    if (!is_integer_literal) {
        return;
    }
    
    u64 x2 = x;
    if (operation == TV_Integer_Literal_Adjust__Add) {
        x2 = x + d;
        // try to clamp overflows
#if 1 // disable if you don't want to handle overflows like this
        if (d < 0) {
            if (x2 > x) x2 = 0;
        } else {
            if (x2 < x) x2 = 0xFFFFFFFFFFFFFFFF;
        }
#endif
    } else if (operation == TV_Integer_Literal_Adjust__Multiply) {
        x2 = x * d;
    } else if (operation == TV_Integer_Literal_Adjust__Divide) {
        if (d != 0) {
            x2 = x / d;
        }
    } else if (operation == TV_Integer_Literal_Adjust__Mod) {
        if (d != 0) {
            x2 = x % d;
        }
    }
        
    // finish
    Scratch_Block scratch(app);
    i32 pad_length = (i32)(number_range.max - number_range.min); // try to keep original padding
    String_Const_u8 new_text = tv_integer_to_literal(app, scratch, x2, radix, pad_length);
    buffer_replace_range(app, buffer, literal_range, new_text);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////// Commands:

CUSTOM_COMMAND_SIG(integer_base_cycle)
CUSTOM_DOC("Cycle base/radix of integer literal under cursor (decimal -> hexadecimal -> binary -> octal -> decimal -> ...)")
{
    tv_change_integer_literal_base(app, 0);
}

CUSTOM_COMMAND_SIG(integer_base_2)
CUSTOM_DOC("Set base of integer literal under cursor to binary (0b01010101)")
{
    tv_change_integer_literal_base(app, 2);
}

CUSTOM_COMMAND_SIG(integer_base_8)
CUSTOM_DOC("Set base of integer literal under cursor to octal (0567)")
{
    tv_change_integer_literal_base(app, 8);
}

CUSTOM_COMMAND_SIG(integer_base_10)
CUSTOM_DOC("Set base of integer literal under cursor to decimal")
{
    tv_change_integer_literal_base(app, 10);
}

CUSTOM_COMMAND_SIG(integer_base_16)
CUSTOM_DOC("Set base of integer literal under cursor to hexadecimal (0x00FF)")
{
    tv_change_integer_literal_base(app, 16);
}

CUSTOM_COMMAND_SIG(integer_base_interactive)
CUSTOM_DOC("Interactively set base of integer literal under cursor")
{
    Query_Bar_Group group(app);
    u8 string_space[256];
    Query_Bar bar = {};
    bar.prompt = string_u8_litexpr("Radix: ");
    bar.string = SCu8(string_space, (u64)0);
    bar.string_capacity = sizeof(string_space);
    if (query_user_number(app, &bar)){
        i64 radix = string_to_integer(bar.string, 10);
        
        switch (radix) {
            default:  break;
            case 2: integer_base_2(app); break;
            case 8: integer_base_8(app); break;
            case 10: integer_base_10(app); break;
            case 16: integer_base_16(app); break;
        }
    }
}

CUSTOM_COMMAND_SIG(integer_inc)
CUSTOM_DOC("Increment integer literal under cursor")
{
    tv_integer_literal_adjust(app, TV_Integer_Literal_Adjust__Add, 1);
}

CUSTOM_COMMAND_SIG(integer_dec)
CUSTOM_DOC("Decrement integer literal under cursor")
{
    tv_integer_literal_adjust(app, TV_Integer_Literal_Adjust__Subtract, 1);
}

// TODO: query_user_number() version which can do signed integers
// TODO: simple expression parser for integer_adjust_interactive

internal b32
tv__query_user_integer(Application_Links* app, String_Const_u8 prompt, i64* out_x)
{
    Query_Bar_Group group(app);
    u8 string_space[256];
    Query_Bar bar = {};
    bar.prompt = prompt;
    bar.string = SCu8(string_space, (u64)0);
    bar.string_capacity = sizeof(string_space);
    if (query_user_number(app, &bar)){
        *out_x = string_to_integer(bar.string, 10);
        return true;
    } else {
        return false;
    }
}

CUSTOM_COMMAND_SIG(integer_add_interactive)
CUSTOM_DOC("Interactively add number to integer literal under cursor")
{
    i64 x;
    if (tv__query_user_integer(app, string_u8_litexpr("Add number: "), &x)) {
        tv_integer_literal_adjust(app, TV_Integer_Literal_Adjust__Add, x);
    }
}

CUSTOM_COMMAND_SIG(integer_subtract_interactive)
CUSTOM_DOC("Interactively subtract number from integer literal under cursor")
{
    i64 x;
    if (tv__query_user_integer(app, string_u8_litexpr("Subtract number: "), &x)) {
        tv_integer_literal_adjust(app, TV_Integer_Literal_Adjust__Subtract, x);
    }
}

CUSTOM_COMMAND_SIG(integer_multiply_interactive)
CUSTOM_DOC("Interactively multiply integer literal under cursor by number")
{
    i64 x;
    if (tv__query_user_integer(app, string_u8_litexpr("Multiply number: "), &x)) {
        tv_integer_literal_adjust(app, TV_Integer_Literal_Adjust__Multiply, x);
    }
}

CUSTOM_COMMAND_SIG(integer_divide_interactive)
CUSTOM_DOC("Interactively divide integer literal under cursor by number")
{
    i64 x;
    if (tv__query_user_integer(app, string_u8_litexpr("Divide number: "), &x)) {
        tv_integer_literal_adjust(app, TV_Integer_Literal_Adjust__Divide, x);
    }
}

CUSTOM_COMMAND_SIG(integer_mod_interactive)
CUSTOM_DOC("Interactively mod integer literal under cursor by number")
{
    i64 x;
    if (tv__query_user_integer(app, string_u8_litexpr("Mod number: "), &x)) {
        tv_integer_literal_adjust(app, TV_Integer_Literal_Adjust__Mod, x);
    }
}

CUSTOM_COMMAND_SIG(integer_adjust_interactive)
CUSTOM_DOC("Interactively adjust integer literal under cursor (+, -, *, /, %)")
{
    Query_Bar_Group group(app);
    u8 string_space[1];
    Query_Bar bar = {};
    bar.prompt = string_u8_litexpr("Enter one of [+ - * / %]: ");
    bar.string = SCu8(string_space, (u64)0);
    bar.string_capacity = sizeof(string_space);
    if (query_user_string(app, &bar) && bar.string.size == 1) {
        char c = bar.string.str[0];
        
        switch (c) {
            default: break;
            case '+': integer_add_interactive(app); break;
            case '-': integer_subtract_interactive(app); break;
            case '*': integer_multiply_interactive(app); break;
            case '/': integer_divide_interactive(app); break;
            case '%': integer_mod_interactive(app); break;
        }
    }
}
