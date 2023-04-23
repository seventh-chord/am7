#pragma once

#include "parse.hpp"

enum struct HighlightKind
{
    None,
    Comment,
    Keyword,
    Identifier,
    OtherLiteral,
    NumberLiteral,
    StringLiteral,
    CharacterLiteral,
    Preprocessor,
    _Count,
};

enum struct HighlightState
{
    Invalid = 0,
    Default,
    BlockComment,
    _Count,
};

struct Highlight
{
    HighlightKind kind;
    s32 start;
    s32 end;
};

struct HighlightList
{
    Highlight highlight;
    HighlightList *next;
};

struct NamedHighlightFunction
{
    char *name;
    HighlightList *(*function)(str line, HighlightState *state);
    str extensions[4];
};

HighlightList *highlight_c(str line, HighlightState *state);
HighlightList *highlight_asm(str line, HighlightState *state);
HighlightList *highlight_python(str line, HighlightState *state);
HighlightList *highlight_odin(str line, HighlightState *state);
HighlightList *highlight_bat(str line, HighlightState *state);

NamedHighlightFunction HIGHLIGHT_FUNCTIONS[] = {
    { "None", null, {} },
    { "C and C++", &highlight_c, { lit_to_str("c"), lit_to_str("h"), lit_to_str("cpp"), lit_to_str("hpp") } },
    { "Python", &highlight_python, { lit_to_str("py") } },
    { "Odin", &highlight_odin, { lit_to_str("odin") } },
    { "Batch scripts", &highlight_bat, { lit_to_str("bat"), lit_to_str("cmd") } },
    { "Assembly", &highlight_asm, { lit_to_str("asm"), lit_to_str("s") } },
};

s32 highlight_get_function_index(str ext) {
    if (ext.length > 0) {
        for (s32 i = 1; i < array_length(HIGHLIGHT_FUNCTIONS); ++i) {
            NamedHighlightFunction *function = &HIGHLIGHT_FUNCTIONS[i];
            for (s32 j = 0; j < array_length(function->extensions); ++j) {
                if (str_equal_ignore_case(ext, function->extensions[j])) {
                    return(i);
                }
            }
        }
    }
    return(0);
}


struct EatResult
{
    s64 length;
    bool valid;
};

EatResult eat_number_c(str input)
{
    char *c = input.data;
    char *ce = &input.data[input.length];
    bool hexadecimal = false;
    bool valid = true;

    if (c + 2 < ce && c[0] == '0' && (c[1] == 'x' || c[1] == 'X') && (is_hex_digit(c[2]) || c[2] == '.')) {
        c += 2;
        hexadecimal = true;
        while (c < ce && is_hex_digit(c[0])) c++;
        goto integer_suffix_or_float;
    }
    if (c < ce && c[0] == '0') {
        while (c < ce && is_octal_digit(c[0])) c++;
        if (c < ce && is_digit(c[0])) valid = false;
        goto done;
    }
    if (c < ce && is_digit(c[0])) {
        while (c < ce && is_digit(c[0])) c++;
        goto integer_suffix_or_float;
    }
    if (c + 1 < ce && c[0] == '.' && is_digit(c[1])) goto decimal_dot;
    goto done;

    integer_suffix_or_float:
    if (c < ce && c[0] == '.') goto decimal_dot;
    if (c < ce && (c[0] == 'u' || c[0] == 'U')) {
        c += 1;
        if (c < ce && (c[0] == 'l' || c[0] == 'L')) c += 1;
        if (c < ce && (c[0] == 'l' || c[0] == 'L')) c += 1;
        goto done;
    }
    if (c < ce && (c[0] == 'l' || c[0] == 'L')) {
        c += 1;
        if (c < ce && (c[0] == 'l' || c[0] == 'L')) c += 1;
        if (c < ce && (c[0] == 'u' || c[0] == 'U')) c += 1;
        goto done;
    }
    goto float_exponent_or_suffix;

    decimal_dot:
    c++;
    if (hexadecimal) {
        while (c < ce && is_hex_digit(c[0])) c++;
    } else {
        while (c < ce && is_digit(c[0])) c++;
    }
    goto float_exponent_or_suffix;

    float_exponent_or_suffix:
    if (c < ce && ((!hexadecimal && (c[0] == 'e' || c[0] == 'E')) ||
                   (hexadecimal && (c[0] == 'p' || c[0] == 'P'))) &&
        ((c + 1 < ce && is_digit(c[1])) ||
         (c + 2 < ce && (c[1] == '+' || c[1] == '-') && is_digit(c[2]))))
    {
        c++;
        if (*c == '+' || *c == '-') c++;
        while (c < ce && is_digit(c[0])) c++;
    }
    if (c < ce && (c[0] == 'f' || c[0] == 'F' || c[0] == 'l' || c[0] == 'L')) c++;
    goto done;

    done:
    EatResult result = {};
    result.length = c - input.data;
    result.valid = valid && (result.length > 0);
    return(result);
}

bool is_identifier_start_c(s32 c)
{
    return(c == '_' || is_ascii_letter(c));
}

bool is_identifier_character_c(s32 c)
{
    return(c == '_' || is_ascii_letter(c) || is_digit(c));
}

HighlightList *highlight_c(str line, HighlightState *state)
{
    static const
    struct { char *name; bool other_kind; }
    KEYWORDS[] = {
        { "break", 0 },
        { "case", 0 },
        { "class", 0 },
        { "const", 0 },
        { "continue", 0 },
        { "default", 0 },
        { "delete", 0 },
        { "do", 0 },
        { "else", 0 },
        { "enum", 0 },
        { "extern", 0 },
        { "false", 1 },
        { "for", 0 },
        { "goto", 0 },
        { "if", 0 },
        { "inline", 0 },
        { "namespace", 0 },
        { "new", 0 },
        { "null", 1 },
        { "nullptr", 1 },
        { "register", 0 },
        { "restrict", 0 },
        { "return", 0 },
        { "static", 0 },
        { "struct", 0 },
        { "switch", 0 },
        { "template", 0 },
        { "this", 0 },
        { "true", 1 },
        { "typedef", 0 },
        { "union", 0 },
        { "volatile", 0 },
        { "while", 0 },
        { "NULL", 1 },
    };
    enum { KEYWORD_FIRST_MIN = 'b', KEYWORD_FIRST_MAX = 'w' };
    static s32 ORDERED_KEYWORD_OFFSETS[KEYWORD_FIRST_MAX - KEYWORD_FIRST_MIN + 2] = {0};
    static bool ORDERED_KEYWORDS_INITIALIZED = false;
    if (!ORDERED_KEYWORDS_INITIALIZED) {
        ORDERED_KEYWORDS_INITIALIZED = true;
        s32 i = 0;
        char c = KEYWORD_FIRST_MIN;
        while (i < array_length(KEYWORDS) && KEYWORDS[i].name[0] != c) ++i;
        while (c <= KEYWORD_FIRST_MAX) {
            ORDERED_KEYWORD_OFFSETS[c - KEYWORD_FIRST_MIN] = i;
            while (i < array_length(KEYWORDS) && KEYWORDS[i].name[0] == c) ++i;
            ++c;
        }
        ORDERED_KEYWORD_OFFSETS[array_length(ORDERED_KEYWORD_OFFSETS) - 1] = i;
    }


    HighlightList *result = null;
    HighlightList *last = null;
    #define list_add() (last = ((last? last->next : result) = stack_alloc(HighlightList, 1)), &last->highlight) // c is nice :)

    s32 i = 0;
    bool at_start_of_line = true;
    bool block_comment_started_on_this_line = false;

    while (i < line.length) {
        if (is_newline(line[i])) {
            i = line.length;
        } else if (*state == HighlightState::BlockComment) {
            s32 i0 = i;

            if (block_comment_started_on_this_line) i += 2;
            block_comment_started_on_this_line = false;

            while (i < line.length && !is_newline(line[i]) && !(i + 1 < line.length && line[i] == '*' && line[i + 1] == '/')) ++i;

            if (i + 1 < line.length && line[i] == '*') {
                i += 2;
                *state = HighlightState::Default;
            }

            if (i != i0) {
                Highlight *entry = list_add();
                entry->kind = HighlightKind::Comment;
                entry->start = i0;
                entry->end = i;
            }
        } else {
            char a = (i + 0 < line.length)? line[i + 0] : 0;
            char b = (i + 1 < line.length)? line[i + 1] : 0;
            char c = (i + 2 < line.length)? line[i + 2] : 0;

            if (a == '/' && b == '*') {
                *state = HighlightState::BlockComment;
                block_comment_started_on_this_line = true;

            } else if (a == '/' && b == '/') {
                Highlight *entry = list_add();
                entry->start = i;
                entry->kind = HighlightKind::Comment;
                i += 2;
                while (i < line.length && !(line[i] == '\r' || line[i] == '\n')) ++i;
                entry->end = i;

            } else if (at_start_of_line && a == '#') {
                Highlight *entry = list_add();
                entry->start = i;
                entry->kind = HighlightKind::Preprocessor;
                ++i;
                while (i < line.length && is_identifier_character_c(line[i])) ++i;
                entry->end = i;

            } else if (a == '\'' || a == '"' ||
                       ((a == 'u' || a == 'U' || a == 'L') && (b == '\'' || b == '"')) ||
                       (a == 'u' && b == '8' && (c == '\'' || c == '"')))
            {
                s32 start = i;
                s32 end = -1;

                HighlightKind kind;
                char quote;
                if (a == '"' || (a != '\'' && b == '"') || (a != '\'' && b != '\'' && c == '"')) {
                    quote = '"';
                    kind = HighlightKind::StringLiteral;
                } else {
                    quote = '\'';
                    kind = HighlightKind::CharacterLiteral;
                }

                while (i < line.length && line[i] != quote) ++i;
                ++i;
                while (i < line.length && end == -1) {
                    if (line[i] == quote) end = i + 1;
                    if (i + 1 < line.length && line[i] == '\\') ++i;
                    ++i;
                }

                if (end == -1) {
                    i = start + 1;
                } else {
                    Highlight *entry = list_add();
                    entry->kind = kind;
                    entry->start = start;
                    entry->end = end;
                }

            // TODO Raw strings

            } else if (is_identifier_start_c(a)) {
                // TODO Unicode characters in identifiers

                Highlight *entry = list_add();
                entry->start = i;
                i += 1;
                while (i < line.length && is_identifier_character_c(line[i])) i++;
                entry->end = i;
                entry->kind = HighlightKind::Identifier;

                str identifier = slice(line, entry->start, entry->end);

                s32 index_min, index_max;
                char first = identifier[0];
                if (first >= KEYWORD_FIRST_MIN && first <= KEYWORD_FIRST_MAX) {
                    index_min = ORDERED_KEYWORD_OFFSETS[first - KEYWORD_FIRST_MIN];
                    index_max = ORDERED_KEYWORD_OFFSETS[first - KEYWORD_FIRST_MIN + 1];
                } else {
                    index_min = ORDERED_KEYWORD_OFFSETS[array_length(ORDERED_KEYWORD_OFFSETS) - 1];
                    index_max = array_length(KEYWORDS);
                }
                for (s32 i = index_min; i < index_max; ++i) {
                    char *keyword = KEYWORDS[i].name;
                    if (keyword == identifier) {
                        entry->kind = KEYWORDS[i].other_kind? HighlightKind::OtherLiteral : HighlightKind::Keyword;
                        break;
                    }
                }
            } else if (is_digit(a) || (a == '.' && is_digit(b))) {
                EatResult number = eat_number_c(slice(line, i));
                if (number.valid) {
                    Highlight *entry = list_add();
                    entry->start = i;
                    entry->end = i + number.length;
                    entry->kind = HighlightKind::NumberLiteral;
                }
                i += max(number.length, 1);

            } else {
                ++i;
            }

            if (a != ' ' && a != '\t') at_start_of_line = false;
        }
    }

    #undef list_add

    return(result);
}

HighlightList *highlight_odin(str line, HighlightState *state)
{
    static const
    struct { char *name; bool other_kind; }
    KEYWORDS[] = {
        //{ "align_of", 0 },
        { "auto_cast", 0 },
        { "bit_field", 0 },
        { "bit_set", 0 },
        { "break", 0 },
        { "case", 0 },
        { "cast", 0 },
        { "context", 0 },
        { "continue", 0 },
        { "defer", 0 },
        { "distinct", 0 },
        { "do", 0 },
        { "dynamic", 0 },
        { "else", 0 },
        { "enum", 0 },
        { "fallthrough", 0 },
        { "false", 1 },
        { "for", 0 },
        { "foreign", 0 },
        { "if", 0 },
        { "import", 0 },
        { "in", 0 },
        { "inline", 0 },
        { "nil", 1 },
        { "no_inline", 0 },
        { "not_in", 0 },
        //{ "offset_of", 0 },
        { "opaque", 0 },
        { "package", 0 },
        { "proc", 0 },
        { "return", 0 },
        //{ "size_of", 0 },
        { "struct", 0 },
        { "switch", 0 },
        { "transmute", 0 },
        { "true", 1 },
        //{ "type_of", 0 },
        { "union", 0 },
        { "using", 0 },
        { "when", 0 },
    };
    enum { KEYWORD_FIRST_MIN = 'a', KEYWORD_FIRST_MAX = 'w' };
    static s32 ORDERED_KEYWORD_OFFSETS[KEYWORD_FIRST_MAX - KEYWORD_FIRST_MIN + 2] = {0};
    static bool ORDERED_KEYWORDS_INITIALIZED = false;
    if (!ORDERED_KEYWORDS_INITIALIZED) {
        ORDERED_KEYWORDS_INITIALIZED = true;
        s32 i = 0;
        char c = KEYWORD_FIRST_MIN;
        while (i < array_length(KEYWORDS) && KEYWORDS[i].name[0] != c) ++i;
        while (c <= KEYWORD_FIRST_MAX) {
            ORDERED_KEYWORD_OFFSETS[c - KEYWORD_FIRST_MIN] = i;
            while (i < array_length(KEYWORDS) && KEYWORDS[i].name[0] == c) ++i;
            ++c;
        }
        ORDERED_KEYWORD_OFFSETS[array_length(ORDERED_KEYWORD_OFFSETS) - 1] = i;
    }


    HighlightList *result = null;
    HighlightList *last = null;
    #define list_add() (last = ((last? last->next : result) = stack_alloc(HighlightList, 1)), &last->highlight) // c is nice :)

    s32 i = 0;
    bool at_start_of_line = true;
    bool block_comment_started_on_this_line = false;

    while (i < line.length) {
        if (is_newline(line[i])) {
            i = line.length;
        } else if (*state == HighlightState::BlockComment) {
            s32 i0 = i;

            if (block_comment_started_on_this_line) i += 2;
            block_comment_started_on_this_line = false;

            while (i < line.length && !is_newline(line[i]) && !(i + 1 < line.length && line[i] == '*' && line[i + 1] == '/')) ++i;

            if (i + 1 < line.length && line[i] == '*') {
                i += 2;
                *state = HighlightState::Default;
            }

            if (i != i0) {
                Highlight *entry = list_add();
                entry->kind = HighlightKind::Comment;
                entry->start = i0;
                entry->end = i;
            }
        } else {
            char a = (i + 0 < line.length)? line[i + 0] : 0;
            char b = (i + 1 < line.length)? line[i + 1] : 0;
            char c = (i + 2 < line.length)? line[i + 2] : 0;

            if (a == '/' && b == '*') {
                *state = HighlightState::BlockComment;
                block_comment_started_on_this_line = true;

            } else if (a == '/' && b == '/') {
                Highlight *entry = list_add();
                entry->start = i;
                entry->kind = HighlightKind::Comment;
                i += 2;
                while (i < line.length && !(line[i] == '\r' || line[i] == '\n')) ++i;
                entry->end = i;

            } else if (a == '\'' || a == '"' ||
                       ((a == 'u' || a == 'U' || a == 'L') && (b == '\'' || b == '"')) ||
                       (a == 'u' && b == '8' && (c == '\'' || c == '"')))
            {
                s32 start = i;
                s32 end = -1;

                HighlightKind kind;
                char quote;
                if (a == '"' || (a != '\'' && b == '"') || (a != '\'' && b != '\'' && c == '"')) {
                    quote = '"';
                    kind = HighlightKind::StringLiteral;
                } else {
                    quote = '\'';
                    kind = HighlightKind::CharacterLiteral;
                }

                while (i < line.length && line[i] != quote) ++i;
                ++i;
                while (i < line.length && end == -1) {
                    if (line[i] == quote) end = i + 1;
                    if (i + 1 < line.length && line[i] == '\\') ++i;
                    ++i;
                }

                if (end == -1) {
                    i = start + 1;
                } else {
                    Highlight *entry = list_add();
                    entry->kind = kind;
                    entry->start = start;
                    entry->end = end;
                }

            } else if (is_identifier_start_c(a)) {
                // TODO Unicode characters in identifiers

                Highlight *entry = list_add();
                entry->start = i;
                i += 1;
                while (i < line.length && is_identifier_character_c(line[i])) i++;
                entry->end = i;
                entry->kind = HighlightKind::Identifier;

                str identifier = slice(line, entry->start, entry->end);

                s32 index_min, index_max;
                char first = identifier[0];
                if (first >= KEYWORD_FIRST_MIN && first <= KEYWORD_FIRST_MAX) {
                    index_min = ORDERED_KEYWORD_OFFSETS[first - KEYWORD_FIRST_MIN];
                    index_max = ORDERED_KEYWORD_OFFSETS[first - KEYWORD_FIRST_MIN + 1];
                } else {
                    index_min = ORDERED_KEYWORD_OFFSETS[array_length(ORDERED_KEYWORD_OFFSETS) - 1];
                    index_max = array_length(KEYWORDS);
                }
                for (s32 i = index_min; i < index_max; ++i) {
                    char *keyword = KEYWORDS[i].name;
                    if (keyword == identifier) {
                        entry->kind = KEYWORDS[i].other_kind? HighlightKind::OtherLiteral : HighlightKind::Keyword;
                        break;
                    }
                }
            } else if (is_digit(a) || (a == '.' && is_digit(b))) {
                EatResult number = eat_number_c(slice(line, i));
                if (number.valid) {
                    Highlight *entry = list_add();
                    entry->start = i;
                    entry->end = i + number.length;
                    entry->kind = HighlightKind::NumberLiteral;
                }
                i += max(number.length, 1);

            } else {
                ++i;
            }

            if (a != ' ' && a != '\t') at_start_of_line = false;
        }
    }

    #undef list_add

    return(result);
}

HighlightList *highlight_bat(str line, HighlightState *state)
{
    HighlightList *result = null;
    HighlightList *last = null;
    #define list_add() (last = ((last? last->next : result) = stack_alloc(HighlightList, 1)), &last->highlight) // c is nice :)

    s32 i = 0;
    bool at_start_of_line = true;
    while (i < line.length) {
        if (is_newline(line[i])) break;

        char c = line[i];

        if (at_start_of_line && line.length - i >= 4 && (c == 'R' || c == 'r') && (memcmp(line.data + i, "rem ", 4) == 0 || memcmp(line.data + i, "REM ", 4) == 0)) {
            Highlight *entry = list_add();
            entry->start = i;
            entry->kind = HighlightKind::Comment;
            i += 2;
            while (i < line.length && !(line[i] == '\r' || line[i] == '\n')) ++i;
            entry->end = i;

        } else if (c == '"' || c == '\'') {
            s32 start = i;
            s32 end = -1;
            char quote = c;

            while (i < line.length && line[i] != quote) ++i;
            ++i;
            while (i < line.length && end == -1) {
                if (line[i] == quote) end = i + 1;
                if (i + 1 < line.length && line[i] == '\\') ++i;
                ++i;
            }

            if (end == -1) {
                i = start + 1;
            } else {
                Highlight *entry = list_add();
                entry->kind = HighlightKind::StringLiteral;
                entry->start = start;
                entry->end = end;
            }

        } else if (is_ascii_letter(c) || c == '%') {
            while (i < line.length && (is_ascii_letter(line[i]) || is_digit(line[i]) || line[i] == '_' || line[i] == '%')) ++i;

        } else if (is_digit(c)) {
            Highlight *entry = list_add();
            entry->start = i;
            while (i < line.length && is_digit(line[i])) ++i;
            entry->end = i;
            entry->kind = HighlightKind::NumberLiteral;

        } else {
            ++i;
        }

        if (!is_whitespace(c)) at_start_of_line = false;
    }

    #undef list_add
    return(result);
}


HighlightList *highlight_python(str line, HighlightState *state)
{
    static const
    struct { char *name; bool other_kind; }
    KEYWORDS[] = {
        { "and", 0 },
        { "as", 0 },
        { "assert", 0 },
        { "async", 0 },
        { "await", 0 },
        { "break", 0 },
        { "class", 0 },
        { "continue", 0 },
        { "def", 0 },
        { "del", 0 },
        { "elif", 0 },
        { "else", 0 },
        { "except", 0 },
        { "False", 1 },
        { "finally", 0 },
        { "for", 0 },
        { "from", 0 },
        { "global", 0 },
        { "if", 0 },
        { "import", 0 },
        { "in", 0 },
        { "is", 0 },
        { "lambda", 0 },
        { "None", 1 },
        { "nonlocal", 0 },
        { "not", 0 },
        { "or", 0 },
        { "pass", 0 },
        { "raise", 0 },
        { "return", 0 },
        { "True", 1 },
        { "try", 0 },
        { "while", 0 },
        { "with", 0 },
        { "yield", 0 },
    };
    enum { KEYWORD_FIRST_MIN = 'a', KEYWORD_FIRST_MAX = 'y' };
    static s32 ORDERED_KEYWORD_OFFSETS[KEYWORD_FIRST_MAX - KEYWORD_FIRST_MIN + 2] = {0};
    static bool ORDERED_KEYWORDS_INITIALIZED = false;
    if (!ORDERED_KEYWORDS_INITIALIZED) {
        ORDERED_KEYWORDS_INITIALIZED = true;
        s32 i = 0;
        char c = KEYWORD_FIRST_MIN;
        while (i < array_length(KEYWORDS) && KEYWORDS[i].name[0] != c) ++i;
        while (c <= KEYWORD_FIRST_MAX) {
            ORDERED_KEYWORD_OFFSETS[c - KEYWORD_FIRST_MIN] = i;
            while (i < array_length(KEYWORDS) && ascii_lowercase(KEYWORDS[i].name[0]) == c) ++i;
            ++c;
        }
        ORDERED_KEYWORD_OFFSETS[array_length(ORDERED_KEYWORD_OFFSETS) - 1] = i;
    }

    HighlightList *result = null;
    HighlightList *last = null;
    #define list_add() (last = ((last? last->next : result) = stack_alloc(HighlightList, 1)), &last->highlight) // c is nice :)

    for (s32 i = 0; i < line.length; ) {
        char a = line[i];
        if (is_newline(a)) {
            i = line.length;
        } else if (a == '#') {
            Highlight *entry = list_add();
            entry->kind = HighlightKind::Comment;
            entry->start = i;
            entry->end = line.length;
            while (entry->end > entry->start && is_newline(line[entry->end - 1])) --entry->end;
            i = line.length;

        // NB (Morten, 2020-07-08) There are string prefixes in python, but we ignore those for now
        } else if (a == '\'' || a == '"') {
            // TODO (Morten, 2020-07-08) triple-quoted multiline strings

            s32 start = i;
            ++i;
            while (i < line.length && line[i] != a) {
                if (line[i] == '\\') ++i;
                ++i;
            }

            if (i < line.length) {
                ++i;
                Highlight *entry = list_add();
                entry->kind = HighlightKind::StringLiteral;
                entry->start = start;
                entry->end = i;
            }
        } else if ((a >= 'a' && a <= 'z') || (a >= 'A' && a <= 'Z') || a == '_') {
            Highlight *entry = list_add();
            entry->start = i;
            ++i;
            while (i < line.length && ((line[i] >= 'a' && line[i] <= 'z') || (line[i] >= 'A' && line[i] <= 'Z') || (line[i] >= '0' && line[i] <= '9') || line[i] == '_')) ++i;
            entry->end = i;
            entry->kind = HighlightKind::Identifier;

            str identifier = slice(line, entry->start, entry->end);

            s32 index_min = 0, index_max = 0;
            char first = ascii_lowercase(identifier[0]);
            if (first >= KEYWORD_FIRST_MIN && first <= KEYWORD_FIRST_MAX) {
                index_min = ORDERED_KEYWORD_OFFSETS[first - KEYWORD_FIRST_MIN];
                index_max = ORDERED_KEYWORD_OFFSETS[first - KEYWORD_FIRST_MIN + 1];
            }
            for (s32 i = index_min; i < index_max; ++i) {
                char *keyword = KEYWORDS[i].name;
                if (keyword == identifier) {
                    entry->kind = KEYWORDS[i].other_kind? HighlightKind::OtherLiteral : HighlightKind::Keyword;
                    break;
                }
            }
        } else if (is_digit(a) || (a == '.' && i + 1 < line.length && is_digit(line[i + 1]))) {
            // TODO (Morten, 2020-07-08) This is wrong for sure, but its not that wrong, so hey ho

            EatResult number = eat_number_c(slice(line, i));
            if (number.valid) {
                Highlight *entry = list_add();
                entry->start = i;
                entry->end = i + number.length;
                entry->kind = HighlightKind::NumberLiteral;
            }
            i += max(number.length, 1);

        } else {
            ++i;
        }
    }

    #undef list_add
    return(result);
}

HighlightList *highlight_asm(str line, HighlightState *state)
{
    HighlightList *result = null;
    HighlightList *last = null;
    #define list_add() (last = ((last? last->next : result) = stack_alloc(HighlightList, 1)), &last->highlight) // c is nice :)

    for (s32 i = 0; i < line.length; ) {
        char a = line[i];
        if (is_newline(a)) {
            i = line.length;
        } else if (a == ';') {
            Highlight *entry = list_add();
            entry->kind = HighlightKind::Comment;
            entry->start = i;
            entry->end = line.length;
            while (entry->end > entry->start && is_newline(line[entry->end - 1])) --entry->end;
            i = line.length;
        } else {
            ++i;
        }
    }

    #undef list_add
    return(result);
}