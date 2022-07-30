#pragma once

bool eat_line(str *from, str *to = null)
{
    bool anything_left = from->length > 0;
    if (anything_left) {
        if (to != null) {
            to->data = from->data;
            to->length = 0;
        }
        while (from->length > 0 && !(from->data[0] == '\n' || from->data[0] == '\r')) {
            if (to != null) {
                to->length += 1;
            }
            *from = slice(*from, 1);
        }

        if (from->length >= 2 && from->data[0] == '\r' && from->data[1] == '\n') {
            *from = slice(*from, 2);
        } else if (from->length >= 1) {
            *from = slice(*from, 1);
        }
    }
    return(anything_left);
}

bool eat_spaces(str *from)
{
    bool found_any = false;
    while (from->length > 0 && (from->data[0] == ' ' || from->data[0] == '\t')) {
        *from = slice(*from, 1);
        found_any = true;
    }
    return(found_any);
}

void trim_spaces(str *from)
{
    while (from->length > 0 && (from->data[0] == ' ' || from->data[0] == '\t')) --from->length, ++from->data;
    while (from->length > 0 && (from->data[from->length - 1] == ' ' || from->data[from->length - 1] == '\t')) --from->length;
}

bool eat_specific(str *from, str specific)
{
    bool found = false;
    if (from->length >= specific.length) {
        found = true;
        for (s64 i = 0; i < specific.length && found; i += 1) {
            if (specific.data[i] != from->data[i]) {
                found = false;
            }
        }
    }
    if (found) *from = slice(*from, specific.length);
    return(found);
}

bool eat_specific(str *from, char *specific)
{
    return(eat_specific(from, cstring_to_str(specific)));
}

bool eat_up_to(str *from, str *into, char target)
{
    s32 i = 0;
    while (i < from->length && from->data[i] != target) ++i;
    bool ok = i < from->length;
    if (ok) {
        if (into) *into = { from->data, i };
        *from = slice(*from, i);
    }
    return(ok);
}

bool eat_s32(str *from, s32 *result)
{
    s32 digits_found = 0;
    s32 parsed_length = 0;
    bool overflowed = false;

    bool negative = false;
    if (from->length > 0 && from->data[0] == '-') {
        negative = true;
        ++parsed_length;
    }

    s32 value = 0;
    while (parsed_length < from->length && (from->data[parsed_length] >= '0' && from->data[parsed_length] <= '9')) {
        s32 digit = from->data[parsed_length] - '0';

        s32 old_value = value;
        value = value*10 + digit;
        if (value < old_value) overflowed = true;

        ++digits_found;
        ++parsed_length;
    }
    if (negative) value = -value;


    bool valid = digits_found > 0 && !overflowed;
    if (valid) {
        from->data += parsed_length;
        from->length -= parsed_length;
        if (result) *result = value;
    }
    return(valid);
}

s32 ascii_lowercase(s32 c)
{
    if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return(c);
}

s64 str_search_ignore_case_internal(str haystack, str lowercase, str uppercase)
{
    debug_assert(lowercase.length == uppercase.length);

    for (s64 i = 0; i < haystack.length - lowercase.length + 1; ++i) {
        bool match = true;
        for (s64 j = 0; j < lowercase.length; ++j) {
            if (!(haystack[i + j] == lowercase[j] || haystack[i + j] == uppercase[j])) {
                match = false;
                break;
            }
        }

        if (match) {
            // Note (Morten, 2020-08-16) The way we do the initial match checking we would match a lowercase epsilon (\u3b5) when searching for a micro sign (\ub5) because in utf8 the first byte of the lowercase epsilon matches the first byte of a upppercase mu (\u39c) while the second byte of the lowercase epsilon matches the second byte of the micro sign...
            for (s64 j = 0; j < lowercase.length; ) {
                s64 step = utf8_expected_length(haystack[i + j]);
                if (step == -1 || j + step > lowercase.length) {
                    ++j;
                } else {
                    if (memcmp(&haystack[i + j], &lowercase[j], step) != 0 && memcmp(&haystack[i + j], &uppercase[j], step) != 0) {
                        match = false;
                        break;
                    }
                    j += step;
                }
            }
        }

        if (match) {
            return(i);
        }
    }

    return(-1);
}

s64 str_search_ignore_case(str haystack, str needle)
{
    stack_enter_frame();
    str lowercase = utf8_map(needle, &unicode_lowercase);
    str uppercase = utf8_map(needle, &unicode_uppercase);
    s64 result = str_search_ignore_case_internal(haystack, lowercase, uppercase);
    stack_leave_frame();
    return(result);
}

bool str_equal_ignore_case(str left, str right)
{
    if (left.length != right.length) return(false);

    char *a = left.data;
    char *b = right.data;
    int i = left.length;
    while (i--) {
        char ca = ascii_lowercase(*a++);
        char cb = ascii_lowercase(*b++);
        if (ca != cb) return(false);
    }
    return(true);
}

bool is_newline(s32 c)
{
    return(c == '\n' || c == '\r');
}

bool is_whitespace(s32 c)
{
    return(c == ' ' || c == '\t');
}

bool is_ascii_letter(s32 c)
{
    return((c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z'));
}

bool is_digit(s32 c)
{
    return(c >= '0' && c <= '9');
}

bool is_octal_digit(s32 c)
{
    return(c >= '0' && c <= '7');
}

bool is_hex_digit(s32 c)
{
    return((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'));
}