#define COLORS_X \
    X(foreground,            0x8b856d,   0x504f45) \
    X(comment,               0x616c6d,   0x7c6f64) \
    X(keyword,               0xd54542,   0xcd3027) \
    X(background,            0x1f1c18,   0xebd5a7) \
    X(literal,               0x967c9c,   0xa9547a) \
    X(string,                0xbebe50,   0xb07213) \
    X(preprocessor,          0x3da071,   0x40883c) \
    X(special,               0xac6531,   0xe2833d) \
    X(support,               0x403c35,   0xae9e86) \
    X(caret,                 0x50d050,   0x1566b7) \
    X(caret_inactive,        0x508050,   0x6d7c98) \
    X(caret_insert,          0xd05050,   0xd65d0e) \
    X(selection,             0x2b2c27,   0xe6ca91) \
    X(selection_unfocused,   0x23231f,   0xead19f) \
    X(selection_border,      0x75756a,   0x4e4e4e) \
    X(status_inactive,       0x505040,   0x7c6f64) \
    X(status_prompt,         0xfe8019,   0xaf3a03) \
    X(status_active,         0x50609d,   0x105f85) \
    X(border,                0x14110e,   0xccb88c) \
    X(script_running,        0x1e83d5,   0x1e83d5) \
    X(script_good,           0x0fa644,   0x0fa644) \
    X(script_bad,            0xef7110,   0xef7110) \
    X(search_match,          0x602fa92c, 0x8057a237) \
    X(search_match_focused,  0x60c97232, 0x809f6f39) \


struct Colors
{
    #define X(name, a, b) u32 name;
    COLORS_X
    #undef X
};

static const
Colors COLORS_DARK = {
    #define X(name, a, b) a,
    COLORS_X
    #undef X
};

static const
Colors COLORS_BRIGHT = {
    #define X(name, a, b) b,
    COLORS_X
    #undef X
};

static const
str COLOR_NAMES[] = {
    #define X(name, a, b) lit_to_str(#name),
    COLORS_X
    #undef X
};

#undef COLORS_X