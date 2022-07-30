#pragma once

const u32 UNICODE_LARGEST_CODEPOINT = 0x10ffff;

struct DecodedCodepoint
{
    bool valid;
    u8 length; // Always at least 1
    s32 codepoint;
};

DecodedCodepoint valid_decoded_codepoint(s32 codepoint)
{
    DecodedCodepoint result = {};
    result.valid = true;
    result.length = 0;
    result.codepoint = codepoint;
    return(result);
}


// Note about out-of-range codepoints:
// Codepoints greater than 0x10ffff can be encoded in utf8 but not in utf16. We output 'Valid = false'
// but 'Length = 4' if these appear in a utf8 string.
// Surrogate pairs (0xd800 - 0xdfff) do NOT get 'Valid = false' if they appear in utf8. This means that
// we decode some ill-formed utf8 strings, which if encoded and redecoded as utf16 will produce different
// codepoint sequences (if the codepoints in utf8 turn into surrogate pairs in utf16). However, if the
// initial string is utf16, we CAN'T decode and reencode it via utf8 to produce different codepoints.
// That means, we just have to be carefull about sending our strings to windows, but not the other way around.

// Reads at most 'Length' bytes from 'data', and tries to decode them as a unicode codepoint
DecodedCodepoint decode_utf8(u8 *data,  s64 length);
DecodedCodepoint decode_utf16(u16 *data, s64 length);

// Writes 'codepoint' into 'buffer' in the proper encoding, and returns the values written. 'buffer' must
// be at least 4 bytes long (i.e. 4*u8 or 2*u16).
// Returns 0 if the codepoint is out of range and can't be encoded.
u8 encode_utf8(s32 codepoint, u8  *buffer);
u8 encode_utf16(s32 codepoint, u16 *buffer);

// Convert strings, replacing invalid characters with the unicode replacement character 0xfffd
// Strings are stack allocated and null-terminated
wstr utf8_to_utf16(str input);
str utf16_to_utf8(wstr input);
str windows_ansi_to_utf8(u8 *bytes, s64 length);

bool utf8_is_continuation(u8 byte);
s32 utf8_expected_length(u8 byte);
s64 utf8_string_length(u8 *bytes, s64 length);


DecodedCodepoint decode_utf8(u8 *data, s64 length)
{
    assert(length > 0);

    DecodedCodepoint result = {};

    if (length >= 1 && (data[0] & 0x80) == 0x00) {
        result.length = 1;
        result.codepoint = data[0];
        result.valid = true;
    } else if (length >= 2 && (data[0] & 0xe0) == 0xc0) {
        result.length = 2;
        result.codepoint = ((((u32) data[0]) & 0x1f) << 6) | ((((u32) data[1]) & 0x3f) << 0);
        result.valid = (data[1] & 0xc0) == 0x80 && !(result.codepoint < 0x80);
    } else if (length >= 3 && (data[0] & 0xf0) == 0xe0) {
        result.length = 3;
        result.codepoint = ((((u32) data[0]) & 0x0f) << 12) | ((((u32) data[1]) & 0x3f) << 6) | ((((u32) data[2]) & 0x3f) << 0);
        result.valid = (data[1] & 0xc0) == 0x80 && (data[2] & 0xc0) == 0x80 && !(result.codepoint < 0x800);
    } else if (length >= 4 && (data[0] & 0xf8) == 0xf0) {
        result.length = 4;
        result.codepoint = ((((u32) data[0]) & 0x07) << 18) | ((((u32) data[1]) & 0x3f) << 12) | ((((u32) data[2]) & 0x3f) << 6) | ((((u32) data[3]) & 0x3f) << 0);
        result.valid = (data[1] & 0xc0) == 0x80 && (data[2] & 0xc0) == 0x80 && (data[3] & 0xc0) == 0x80 && (result.codepoint <= 0x10ffff) && !(result.codepoint < 0x10000);
    }

    if (!result.valid) {
        result.length = 1;
        result.codepoint = data[0];
    }

    return(result);
}

bool utf8_is_continuation(u8 byte)
{
    return((byte & 0xc0) == 0x80);
}

s32 utf8_expected_length(u8 byte)
{
    s32 result = -1;
    if ((byte & 0x80) == 0x00) {
        result = 1;
    } else if ((byte & 0xe0) == 0xc0) {
        result = 2;
    } else if ((byte & 0xf0) == 0xe0) {
        result = 3;
    } else if ((byte & 0xf8) == 0xf0) {
        result = 4;
    }
    return(result);
}

DecodedCodepoint decode_utf16(u16 *data, s64 length)
{
    assert(length > 0);

    DecodedCodepoint result = {};

    if (length >= 2 &&
        data[0] >= 0xd800 && data[0] <= 0xdbff &&
        data[1] >= 0xdc00 && data[1] <= 0xdfff)
    {
        result.valid = true;
        result.length = 2;
        u32 hi = data[0] & 0x3ff;
        u32 lo = data[1] & 0x3ff;
        result.codepoint = ((hi << 10) | lo) + 0x10000;
    } else {
        result.valid = length >= 1;
        result.length = 1;
        result.codepoint = data[0];
    }

    return(result);
}

u8 encode_utf8(s32 codepoint, u8 *buffer)
{
    u8 length = 0;

    if (codepoint < 0x80) {
        length = 1;
        buffer[0] = codepoint;
    } else if (codepoint < 0x800) {
        length = 2;
        buffer[0] = ((codepoint >> 6) & 0x1f) | 0xc0;
        buffer[1] = ((codepoint >> 0) & 0x3f) | 0x80;
    } else if (codepoint < 0x10000) {
        length = 3;
        buffer[0] = ((codepoint >> 12) & 0x0f) | 0xe0;
        buffer[1] = ((codepoint >> 6) & 0x3f) | 0x80;
        buffer[2] = ((codepoint >> 0) & 0x3f) | 0x80;
    } else if (codepoint <= 0x10ffff) {
        length = 4;
        buffer[0] = ((codepoint >> 18) & 0x07) | 0xf0;
        buffer[1] = ((codepoint >> 12) & 0x3f) | 0x80;
        buffer[2] = ((codepoint >> 6) & 0x3f) | 0x80;
        buffer[3] = ((codepoint >> 0) & 0x3f) | 0x80;
    }

    return(length);
}

u8 encode_utf16(s32 codepoint, u16 *buffer)
{
    u8 length = 0;

    if (codepoint <= 0xffff) {
        length = 1;
        buffer[0] = codepoint;
    } else if (codepoint <= 0x10ffff) {
        length = 2;
        codepoint -= 0x10000;
        buffer[0] = ((codepoint >> 10) & 0x3ff) | 0xd800;
        buffer[1] = ((codepoint >> 0)  & 0x3ff) | 0xdc00;
    }

    return(length);
}

wstr utf8_to_utf16(str input)
{
    wstr result = {};

    // 1, 2 or 3 utf8 bytes map to 1 utf16 short, 4 utf8 bytes map to 2 utf16 shorts, +1 for null termination
    s64 max_result_length = input.length;
    result.data = stack_alloc(wchar_t, max_result_length + 1);

    s64 offset = 0;
    while (offset < input.length) {
        DecodedCodepoint decoded = decode_utf8((u8*) (input.data + offset), input.length - offset);
        offset += max(decoded.length, 1); // Advance even on invalid input

        if (decoded.valid) {
            u8 encoded_length = encode_utf16(decoded.codepoint, (u16 *) &result.data[result.length]);
            assert(encoded_length > 0); // We should be able to encode all codepoints 'decode_utf8' marks as valid
            result.length += encoded_length;
        } else {
            result.data[result.length++] = 0xfffd; // Unicode replacement character
        }
    }

    assert(result.length <= max_result_length);
    stack_trim_allocation(result.data, (result.length + 1) * sizeof(wchar_t));
    result.data[result.length] = 0;
    return(result);
};

str utf16_to_utf8(wstr input)
{
    str result = {};

    // 2 utf16 bytes map to 1, 2 or 3 utf8 bytes, 4 utf16 bytes map to 4 utf8 bytes, +1 for null termination
    s64 max_result_length = input.length*3;
    result.data = stack_alloc(char, max_result_length + 1);

    s64 offset = 0;
    while (offset < input.length) {
        DecodedCodepoint decoded = decode_utf16((u16 *) (input.data + offset), input.length - offset);
        offset += max(decoded.length, 1); // Advance even on invalid input

        if (decoded.valid) {
            u8 encoded_length = encode_utf8(decoded.codepoint, (u8*) &result.data[result.length]);
            assert(encoded_length > 0); // We should be able to encode all codepoints 'decode_utf8' marks as valid
            result.length += encoded_length;
        } else {
            // Unicode replacement character
            result.data[result.length++] = 0xef;
            result.data[result.length++] = 0xbf;
            result.data[result.length++] = 0xbd;
        }
    }

    assert(result.length <= max_result_length);
    stack_trim_allocation(result.data, result.length + 1);
    result.data[result.length] = 0;
    return(result);
}


str windows_ansi_to_utf8(u8 *bytes, s64 length)
{
    // For 0x80 to 0x9f
    str extra_table[32] = {
        lit_to_str("\xe2\x82\xac"),
        lit_to_str("\xef\xbf\xbd"),
        lit_to_str("\xe2\x80\x9a"),
        lit_to_str("\xc6\x92"),
        lit_to_str("\xe2\x80\x9e"),
        lit_to_str("\xe2\x80\xa6"),
        lit_to_str("\xe2\x80\xa0"),
        lit_to_str("\xe2\x80\xa1"),
        lit_to_str("\xcb\x86"),
        lit_to_str("\xe2\x80\xb0"),
        lit_to_str("\xc5\xa0"),
        lit_to_str("\xe2\x80\xb9"),
        lit_to_str("\xc5\x92"),
        lit_to_str("\xef\xbf\xbd"),
        lit_to_str("\xc5\xbd"),
        lit_to_str("\xef\xbf\xbd"),
        lit_to_str("\xef\xbf\xbd"),
        lit_to_str("\xe2\x80\x98"),
        lit_to_str("\xe2\x80\x99"),
        lit_to_str("\xe2\x80\x9c"),
        lit_to_str("\xe2\x80\x9d"),
        lit_to_str("\xe2\x80\xa2"),
        lit_to_str("\xe2\x80\x93"),
        lit_to_str("\xe2\x80\x94"),
        lit_to_str("\xcb\x9c"),
        lit_to_str("\xe2\x84\xa2"),
        lit_to_str("\xc5\xa1"),
        lit_to_str("\xe2\x80\xba"),
        lit_to_str("\xc5\x93"),
        lit_to_str("\xef\xbf\xbd"),
        lit_to_str("\xc5\xbe"),
        lit_to_str("\xc5\xb8"),
    };

    str result = {};

    for (s64 i = 0; i < length; ++i) {
        u8 byte = bytes[i];
        if (byte < 0x80) {
            result.length += 1;
        } else if (byte > 0x9f) {
            result.length += 2;
        } else {
            result.length += extra_table[byte - 0x80].length;
        }
    }

    result.data = stack_alloc(char, result.length + 1);
    result.data[result.length] = 0;

    s64 offset = 0;
    for (s64 i = 0; i < length; ++i) {
        u8 byte = bytes[i];
        if (byte < 0x80) {
            result.data[offset++] = byte;
        } else if (byte > 0x9f) {
            assert(encode_utf8((u32) byte, (u8*) &result.data[offset]) == 2);
            offset += 2;
        } else {
            str extra = extra_table[byte - 0x80];
            memcpy(&result.data[offset], extra.data, extra.length);
            offset += extra.length;
        }
    }

    assert(offset == result.length);
    return(result);
}

s64 utf8_string_length(u8 *data, s64 length)
{
    s64 result = 0;
    s64 offset = 0;
    while (offset < length) {
        result += 1;
        DecodedCodepoint decoded = decode_utf8(&data[offset], length - offset);
        offset += decoded.length;
    }
    return(result);
}



struct UnicodeRange
{
    s32 min;
    s32 max;
};

static const
UnicodeRange UNICODE_SUPPORTED_RANGES[] = {
    // basic latin
    { 0x20, 0x7e },
    // skip more control characters
    { 0xa1, 0xac },
    // skip soft hyphen
    { 0xae, 0x1bf },
    // skip 'african letters for clicks', which look like other symbols and are confusing
    { 0x1c4, 0x2af },

    // skip 'spacing modifier letters'
    // skip 'combining diacritical marks'

    // greek (skipping a lot of wierd stuff, which is fine for me because I don't know greek)
    { 0x37b, 0x37d }, // lunate sigma
    // very important, skip greek question mark, because wtf
    { 0x37f, 0x37f }, // capital yot
    { 0x391, 0x3a1 }, // other letters
    // skip \u3a2, because apparently it doesn't exist :/
    { 0x3a3, 0x3ff }, // more other letters

    // cyrillic (skipping some confusing numerical characters)
    { 0x400, 0x481 },
    { 0x48a, 0x52f },

    { 0x20a0, 0x20bf }, // currency signs
    { 0x2150, 0x215e }, // fractions
    { 0x2190, 0x21ff }, // arrows
    { 0x2200, 0x222f }, // some math symbols
};


// Note (Morten, 2020-08-12) Generated with some python script I wrote
enum { _UNICODE_CASE_BLOCK_SIZE = 4 };
s16 _unicode_case_map[71][_UNICODE_CASE_BLOCK_SIZE] = {{0,0,0,0},{0,32,32,32},{32,32,32,32},{32,32,32,0},{1,0,1,0},{0,0,1,0},{0,1,0,1},{-121,1,0,1},{0,1,0,0},{0,210,1,0},{1,0,206,1},{0,205,205,1},{0,0,79,202},{203,1,0,205},{207,0,211,209},{1,0,0,0},{211,213,0,214},{1,0,218,1},{0,218,0,0},{0,217,217,1},{0,1,0,219},{2,1,0,2},{1,0,2,1},{0,2,1,0},{1,0,-97,-56},{-130,0,1,0},{0,0,0,1},{0,-163,0,0},{0,1,0,-195},{69,71,1,0},{0,0,0,116},{32,32,0,32},{0,0,0,8},{-60,0,0,1},{0,-7,1,0},{0,-130,-130,-130},{80,80,80,80},{15,1,0,1},{0,-743,0,0},{32,32,32,-121},{1,0,0,1},{-195,0,0,1},{0,-97,0,0},{0,1,-163,0},{0,0,-130,0},{0,1,0,-56},{0,1,2,0},{1,2,0,1},{2,0,1,0},{1,79,0,1},{0,0,1,2},{0,0,0,210},{206,0,205,205},{0,202,0,203},{205,0,0,207},{209,211,0,0},{0,0,0,211},{0,0,213,0},{0,214,0,0},{218,0,0,218},{218,69,217,217},{71,0,0,0},{0,0,219,0},{0,0,0,-130},{-130,-130,0,0},{32,32,31,32},{62,57,0,0},{0,47,54,8},{86,80,-7,116},{0,96,0,0},{1,0,1,15}};
u8 _unicode_lowercase_map[332] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,2,2,2,2,2,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,2,2,2,2,3,2,3,0,0,0,0,0,0,0,0,4,4,4,4,4,4,4,4,4,4,4,4,5,4,6,6,6,6,5,4,4,4,4,4,4,4,4,4,4,4,7,8,9,10,11,12,13,14,15,16,4,17,18,17,19,20,15,15,0,21,22,6,6,6,6,5,4,4,4,4,23,24,4,4,4,4,4,4,4,4,4,4,25,4,4,4,4,0,26,27,28,29,4,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,30,0,0,0,0,1,2,2,2,31,2,2,0,0,0,0,0,0,0,0,32,0,0,4,4,4,4,4,4,0,33,34,35,36,36,36,36,2,2,2,2,2,2,2,2,0,0,0,0,0,0,0,0,0,0,0,0,4,4,4,4,4,4,4,4,15,0,5,4,4,4,4,4,4,4,4,4,4,4,4,4,37,6,6,8,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4};
s32 unicode_lowercase(s32 codepoint)
{
    s32 i = codepoint / _UNICODE_CASE_BLOCK_SIZE;
    s32 j = codepoint % _UNICODE_CASE_BLOCK_SIZE;
    if (i < sizeof(_unicode_lowercase_map)) {
        codepoint += _unicode_case_map[_unicode_lowercase_map[i]][j];
    }
    return(codepoint);
}
u8 _unicode_uppercase_map[332] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,2,2,2,2,2,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,38,0,0,0,0,0,0,0,0,0,0,2,2,2,2,2,3,2,39,6,6,6,6,6,6,6,6,6,6,6,6,26,6,5,4,4,4,40,6,6,6,6,6,6,6,6,6,6,6,5,4,41,8,15,15,5,42,43,44,6,8,15,8,15,4,8,45,0,46,47,48,4,4,4,49,6,6,6,6,50,8,6,6,6,6,6,6,6,6,6,6,26,6,6,6,6,0,0,15,5,26,6,6,51,52,53,0,54,0,55,56,57,58,0,0,59,0,60,61,62,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,63,64,0,0,0,0,0,0,0,0,0,0,0,0,1,2,2,2,65,2,2,0,66,67,6,6,6,6,6,6,68,69,40,0,0,0,0,0,0,0,0,0,0,0,0,0,2,2,2,2,2,2,2,2,36,36,36,36,6,6,6,6,6,6,6,6,8,0,26,6,6,6,6,6,6,6,6,6,6,6,6,6,5,4,4,70,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6};
s32 unicode_uppercase(s32 codepoint)
{
    s32 i = codepoint / _UNICODE_CASE_BLOCK_SIZE;
    s32 j = codepoint % _UNICODE_CASE_BLOCK_SIZE;
    if (i < sizeof(_unicode_uppercase_map)) {
        codepoint -= _unicode_case_map[_unicode_uppercase_map[i]][j];
    }
    return(codepoint);
}

str utf8_map(str in, s32 (*map)(s32))
{
    str out = stack_make_slice(char, in.length);
    int i = 0;
    while (i < in.length) {
        DecodedCodepoint decoded = decode_utf8((u8 *) in.data + i, in.length - i);
        if (decoded.valid) {
            s32 c = (*map)(decoded.codepoint);
            u8 reencoded_length = encode_utf8(c, (u8 *) out.data + i);
            i += decoded.length;
            debug_assert(reencoded_length == decoded.length);
        } else {
            out[i] = decoded.codepoint;
            ++i;
        }
    }
    return(out);
}