
#include "highlighting.hpp"

enum NewlineMode
{
    LF,
    CR,
    CRLF,
    _COUNT,
};
str NEWLINE[3] = { { "\n", 1 }, { "\r", 1 }, { "\r\n", 2 } };
str NEWLINE_ESCAPED[3] = { lit_to_str("\\n (unix)"), lit_to_str("\\r"), lit_to_str("\\r\\n (ms-dos)") };

enum struct TabMode
{
    SOFT,
    HARD,
};

enum
{
    SOFTWRAP_PRE_CODEPOINT = '>',
    SOFTWRAP_POST_CODEPOINT = '-',
    SOFTWRAP_PRE_SPACES = 3,
};

struct VirtualLine
{
    s32 start;
    s32 end;

    s32 physical_line_index;

    s32 virtual_indent;
    u32 flags;
    enum {
        MARK_CONTINUATION = 1 << 0,
        ENDS_IN_ACTUAL_NEWLINE = 1 << 1,
    };

    // Only used for the first line with a given physical_line_index
    s32 first_highlight_index;
    HighlightState prev_highlight_state;
};


struct Caret
{
    s32 offset;
    s32 target_line_offset;
};

struct Selection
{
    union
    {
        struct {
            Caret start;
            Caret end;
        };
        Caret carets[2];
    };

    s32 focused_end;
};


enum {
    SEARCH_RESULT_HIDE = 1 << 0,
    SEARCH_RESULT_CASE_MATCH = 1 << 1,
    SEARCH_RESULT_IDENTIFIER_MATCH = 1 << 2,
};

struct SearchResult
{
    s32 min;
    s32 max;
    u32 flags;
};

enum {
    TAB_WIDTH_DEFAULT = 2,
    TAB_WIDTH_MIN = 2,
    TAB_WIDTH_MAX = 16,
};

struct View
{
    s32 revision;
    s32 revision_at_last_repeatable_expand;

    Array<Selection> selections;
    s32 focused_selection;

    struct
    {
        u32 filters;
        SearchResult *list;
        s32 active, total, capacity;
        s32 focused;
    } search;

    s32 focus_offset;

    s32 focus_line_offset_target;
    s64 focus_line_offset_current;

    s32v2 layout_offset;
};
enum { FOCUS_LINE_OFFSET_SUBSTEPS = 128 };

struct Buffer
{
    char *data;
    s32 cap, a, b;

    u8 *history;
    s32 history_length;
    s32 history_capacity;
    s32 history_caret;

    Font *font;
    s32 max_glyphs_per_line;
    s32 visible_lines, margin_lines;
    bool show_special_characters;
    s32 highlight_function_index;
    GapArray<VirtualLine> lines;
    GapArray<Highlight> highlights;
    s32 physical_line_count;

    bool no_user_input;

    Path path;
    str path_display_string;
    bool path_display_string_static;

    s64 revision;
    s64 last_saved_revision;

    u64 last_save_time;
    bool last_save_time_valid;
    enum { EXT_SAME, EXT_MODIFIED, EXT_DELETED } external_status;

    NewlineMode newline_mode;
    TabMode tab_mode;
    s8 tab_width;

    View views[2]; // We support up to two side-by-side views in the editor, so we need to store information for at most two views
    s32 last_show_time[2]; // Set whenever the buffer is shown in a given split
};

void buffer_free(Buffer *buffer);
void buffer_reset(Buffer *buffer);
void buffer_normalize(Buffer *buffer, View *view);

void buffer_set_tab_width(Buffer *buffer, s32 tab_width);
void buffer_set_layout_parameters(Buffer *buffer, Font *font, s32 max_glyphs_per_line, s32 visible_lines, s32 margin_lines, bool show_special_characters);
void buffer_override_highlight_mode(Buffer *buffer, s32 highlight_function_index);

enum { BUFFER_SHOW_ANYWHERE, BUFFER_SHOW_ANYWHERE_DONT_SMOOTHSCROLL_ONE_LINE_AFTER_INSERT, BUFFER_SHOW_CENTER, BUFFER_SHOW_TOP, BUFFER_SHOW_BOTTOM };
void buffer_view_scroll(Buffer *buffer, View *view, s32 delta);
void buffer_view_show(Buffer *buffer, View *view, int where_on_screen);
void buffer_view_set_focus(Buffer *buffer, View *view, s32 offset);
void buffer_view_set_focus_to_focused_selection(Buffer *buffer, View *view);
void buffer_view_set_focus_to_focused_search_result(Buffer *buffer, View *view);

s32 buffer_length(Buffer *buffer);
str buffer_move_gap_to_end(Buffer *buffer);
str buffer_get_slice(Buffer *buffer, s32 min, s32 max);
str buffer_get_virtual_line_text(Buffer *buffer, s32 virtual_line_index);
s32 buffer_offset_to_virtual_line_index(Buffer *buffer, s32 offset);
s32v2 buffer_offset_to_layout_offset(Buffer *buffer, View *view, s32 offset);
s32 buffer_layout_offset_to_offset(Buffer *buffer, View *view, s32v2 layout_offset);

void buffer_next_selection(Buffer *buffer, View *view, s32 direction);
void buffer_remove_focused_selection(Buffer *buffer, View *view);
void buffer_collapse_selections(Buffer *buffer, View *view);
void buffer_remove_nonfocused_selection(Buffer *buffer, View *view);
void buffer_change_selection_focused_end(Buffer *buffer, View *view);
enum { BUFFER_SET_SELECTION_SET, BUFFER_SET_SELECTION_ADD, BUFFER_SET_SELECTION_REPLACE };
void buffer_set_selection(Buffer *buffer, View *view, Selection new_selection, int action);
void buffer_add_caret_on_each_selected_line(Buffer *buffer, View *view);

void buffer_step_all_carets_codepoints(Buffer *buffer, View *view, s32 direction, bool stop_at_eol, bool split_carets);
void buffer_step_all_carets_both_ways(Buffer *buffer, View *view, s32 direction, bool stop_at_eol);
void buffer_step_all_carets_lines(Buffer *buffer, View *view, s32 direction, bool grow_selections, bool add_new_carets);
void buffer_step_all_carets_empty_lines(Buffer *buffer, View *view, s32 direction);
void buffer_jump_to_physical_line(Buffer *buffer, View *view, s32 physical_line, bool split_carets);
void buffer_step_all_carets_to_boundary(Buffer *buffer, View *view, s32 direction, s32 boundary);
void buffer_step_all_carets_words(Buffer *buffer, View *view, s32 direction, bool start);
void buffer_step_all_carets_past_whitespace(Buffer *buffer, View *view, s32 direction);
enum {
    BUFFER_EXPAND_LINE,
    BUFFER_EXPAND_LINE_WITHOUT_LEADING_SPACES,
    BUFFER_EXPAND_ALL,
    BUFFER_EXPAND_WORD,
    BUFFER_EXPAND_BLOCK,
    BUFFER_EXPAND_PARENTHESIS = '()',
    BUFFER_EXPAND_BRACKET = '[]',
    BUFFER_EXPAND_BRACE = '{}',
    BUFFER_EXPAND_TAG = '<>',
    BUFFER_EXPAND_DOUBLE_QUOTE = '\"',
    BUFFER_EXPAND_SINGLE_QUOTE = '\'',
};
void buffer_expand_all_carets(Buffer *buffer, View *view, s32 delimiter);
void buffer_expand(Buffer *buffer, Selection *selection, s32 delimiter);

void buffer_search(Buffer *buffer, View *view, str text);
void buffer_search_filter(Buffer *buffer, View *view, u32 filters);
void buffer_next_search_result(Buffer *buffer, View *view, s32 direction, bool show);
void buffer_clear_search_results(Buffer *buffer, View *view);
void buffer_add_selection_at_active_search_result(Buffer *buffer, View *view, s32 direction);
void buffer_expand_all_carets_to_next_search_result(Buffer *buffer, View *view, s32 direction);
void buffer_add_selection_at_all_search_results(Buffer *buffer, View *view);
void buffer_remove_all_search_results(Buffer *buffer, View *view);


void buffer_change_line_endings(Buffer *buffer, NewlineMode new_mode);
void buffer_insert_at_all_carets(Buffer *buffer, View *view, str text);
void buffer_insert_tabs_at_all_carets(Buffer *buffer, View *view);
void buffer_insert_on_new_line_at_all_carets(Buffer *buffer, View *view, s32 direction, str text);
void buffer_insert_at_caret(Buffer *buffer, View *view, s32 selection_index, str text);
void buffer_insert_at_end(Buffer *buffer, str text);
enum {
    EMPTY_ALL_SELECTIONS_AND_THATS_IT,
    EMPTY_ALL_SELECTIONS_OR_DELETE_ONE_LEFT,
    EMPTY_ALL_SELECTIONS_OR_DELETE_ONE_RIGHT,
};
void buffer_empty_all_selections(Buffer *buffer, View *view, int action = EMPTY_ALL_SELECTIONS_AND_THATS_IT);
void buffer_remove_trailing_spaces(Buffer *buffer, View *view);
void buffer_indent_selected_lines(Buffer *buffer, View *view, bool indent);
void buffer_indent_all_carets_to_same_level(Buffer *buffer, View *view);

void history_insert_sentinel(Buffer *buffer);
void history_scroll(Buffer *buffer, View *view, s32 direction);

bool buffer_set_path(Buffer *buffer, Path path, Path relative_to);
void buffer_update_display_string(Buffer *buffer, Path relative_to);
IoError buffer_load(Buffer *buffer);
IoError buffer_save(Buffer *buffer, Path alternative_path = null);
bool buffer_check_for_external_changes(Buffer *buffer);
bool buffer_has_internal_changes(Buffer *buffer);

s32 buffer_length(Buffer *buffer)
{
    return(buffer->a + buffer->cap - buffer->b);
}

static
void _buffer_make_space(Buffer *buffer, s32 extra)
{
    s32 free = buffer->b - buffer->a;
    if (extra > free) {
        s32 new_cap = next_power_of_two(max(buffer->cap + extra, 8*1024));
        assert(new_cap > buffer->cap);

        char *new_data = (char *) MemBigAlloc(new_cap);
        s32 new_b = new_cap - buffer->cap + buffer->b;

        if (buffer->data) {
            memcpy(new_data, buffer->data, buffer->a);
            memcpy(new_data + new_b, buffer->data + buffer->b, buffer->cap - buffer->b);
            MemBigFree(buffer->data);
        }

        buffer->data = new_data;
        buffer->cap = new_cap;
        buffer->b = new_b;
    }
}

static
void _buffer_move_gap(Buffer *buffer, s32 target)
{
    assert(0 <= target && target <= buffer_length(buffer));

    if (target == buffer->a) {
        // nice
    } else if (target > buffer->a) {
        s32 delta = target - buffer->a;
        memcpy(buffer->data + buffer->a, buffer->data + buffer->b, delta);
        buffer->a += delta;
        buffer->b += delta;
    } else {
        s32 delta = buffer->a - target;
        buffer->a -= delta;
        buffer->b -= delta;
        memcpy(buffer->data + buffer->b, buffer->data + buffer->a, delta);
    }
}

static
void _buffer_move_gap_to_next_sensible_boundary(Buffer *buffer)
{
    s32 steps = 0;
    while (buffer->b + steps < buffer->cap && steps < 3 && utf8_is_continuation(buffer->data[buffer->b + steps])) ++steps;
    if (steps == 0 && buffer->a > 0 && buffer->b < buffer->cap && buffer->data[buffer->a - 1] == '\r' && buffer->data[buffer->b] == '\n') steps = 1;
    if (steps) _buffer_move_gap(buffer, buffer->a + steps);
}

str buffer_move_gap_to_end(Buffer *buffer)
{
    s32 length = buffer->a + buffer->cap - buffer->b;
    _buffer_move_gap(buffer, length);
    str full_content = {};
    full_content.data = buffer->data;
    full_content.length = buffer->a;
    return(full_content);
}

enum {
    HISTORY_INSERT = 0xf0,
    HISTORY_DELETE = 0xf1,
    HISTORY_BLOCK_START = 0xf2,
    HISTORY_REPEAT_AT = 0xf3,
    HISTORY_SENTINEL = 0xf4,
    HISTORY_BOUNDARY = 0xf5,
};

static
void _history_make_space(Buffer *buffer, bool at_end, s32 extra)
{
    if (at_end && buffer->history_caret) {
        buffer->history_length = buffer->history_caret;
    }

    if (buffer->history_length + extra > buffer->history_capacity) {
        buffer->history_capacity = max(4096, next_power_of_two(buffer->history_length + extra));
        buffer->history = (u8 *) heap_grow(buffer->history, buffer->history_capacity);
        if (!buffer->history_length) {
            buffer->history[buffer->history_length++] = HISTORY_BOUNDARY;
        }
    }
}

static
void _history_write_u8(Buffer *buffer, u8 value)
{
    buffer->history[buffer->history_length++] = value;
}

static
void _history_write_s32(Buffer *buffer, s32 value)
{
    memcpy(buffer->history + buffer->history_length, &value, 4);
    buffer->history_length += 4;
}

static
u8 _history_read_u8(Buffer *buffer, bool backward)
{
    u8 result;
    if (backward) {
        assert(buffer->history_caret > 0);
        result = buffer->history[--buffer->history_caret];
    } else {
        if (buffer->history_caret >= buffer->history_length) {
            result = HISTORY_BOUNDARY;
        } else {
            result = buffer->history[buffer->history_caret++];
        }
    }
    return(result);
}

static
s32 _history_read_s32(Buffer *buffer, bool backward)
{
    s32 result = 0;
    if (backward) {
        assert(buffer->history_caret >= 4);
        memcpy(&result, buffer->history + buffer->history_caret - 4, 4);
        buffer->history_caret -= 4;
    } else {
        assert(buffer->history_caret + 4 <= buffer->history_length);
        memcpy(&result, buffer->history + buffer->history_caret, 4);
        buffer->history_caret += 4;
    }
    return(result);
}

static
str _history_read_str(Buffer *buffer, s32 length, bool backward)
{
    str result = {};
    result.length = length;
    if (backward) {
        buffer->history_caret -= length;
        assert(buffer->history_caret >= 0);
        result.data = (char *) (buffer->history + buffer->history_caret);
    } else {
        result.data = (char *) (buffer->history + buffer->history_caret);
        buffer->history_caret += length;
        assert(buffer->history_caret <= buffer->history_length);
    }
    return(result);
}

void history_insert_sentinel(Buffer *buffer)
{
    u8 last = buffer->history_length > 0? buffer->history[buffer->history_caret - 1] : 0;
    if (!(last == HISTORY_SENTINEL || last == HISTORY_BOUNDARY)) {
        _history_make_space(buffer, true, 1);
        _history_write_u8(buffer, HISTORY_SENTINEL);
        buffer->history_caret = buffer->history_length;
    }
}

static
void _history_add(Buffer *buffer, bool insert, s32 offset, str text)
{
    u8 our_kind = insert? HISTORY_INSERT : HISTORY_DELETE;
    bool did_merge = false;
    s32 historical_offset = offset;

    s32 initial_caret = buffer->history_caret;
    while (buffer->history_caret > 0 && !did_merge) {
        u8 other_kind = _history_read_u8(buffer, true);

        // Note (Morten, 2020-08-04) I haven't bothered implementing cases for 'other_kind == HISTORY_DELETE', because I in practice usually delete text in larger blocks.
        // We do still try to merge deletes into preceding inserts when possible though
        if (other_kind != HISTORY_INSERT) break;

        s32 other_length = _history_read_s32(buffer, true);
        s32 other_offset = _history_read_s32(buffer, true);
        str other_text = _history_read_str(buffer, other_length, true);
        assert(_history_read_s32(buffer, true) == other_length);
        assert(_history_read_u8(buffer, true) == HISTORY_BLOCK_START);

        if ((our_kind == HISTORY_INSERT && other_offset <= historical_offset && historical_offset <= other_offset + other_length) ||
            (our_kind == HISTORY_DELETE && other_offset <= historical_offset && historical_offset + text.length<= other_offset + other_length))
        {
            s32 delta = text.length;
            if (our_kind == HISTORY_DELETE) delta = -delta;
            if (delta > 0) _history_make_space(buffer, false, delta);

            s32 new_length = other_text.length + delta;

            if (new_length == 0) {
                s32 start = buffer->history_caret;
                s32 end = buffer->history_caret + 14 + other_text.length;
                memmove(buffer->history + start, buffer->history + end, initial_caret - end);
                buffer->history_length = initial_caret + start - end;
            } else {
                s32 adr_length_a = buffer->history_caret + 1;
                s32 adr_length_b = buffer->history_caret + 5 + other_length + 4;
                memcpy(buffer->history + adr_length_a, &new_length, sizeof(new_length));
                memcpy(buffer->history + adr_length_b, &new_length, sizeof(new_length));

                s32 adr_insert = buffer->history_caret + 5 + (historical_offset - other_offset);
                if (delta > 0) {
                    memmove(buffer->history + adr_insert + delta, buffer->history + adr_insert, initial_caret - adr_insert);
                    memcpy(buffer->history + adr_insert, text.data, delta);
                } else {
                    assert(memcmp(buffer->history + adr_insert, text.data, text.length) == 0);
                    memmove(buffer->history + adr_insert, buffer->history + adr_insert - delta, initial_caret - adr_insert + delta);
                }

                buffer->history_length = initial_caret + delta;
            }

            while (buffer->history_caret < buffer->history_length) {
                u8 forward_kind = _history_read_u8(buffer, false);
                assert(forward_kind == HISTORY_BLOCK_START);
                s32 forward_length = _history_read_s32(buffer, false);
                _history_read_str(buffer, forward_length, false);
                s32 forward_offset = _history_read_s32(buffer, false);
                if (forward_offset > offset) {
                    forward_offset += delta;
                    memcpy(buffer->history + buffer->history_caret - 4, &forward_offset, 4);
                }
                _history_read_s32(buffer, false);
                _history_read_u8(buffer, false);
            }
            did_merge = true;
        }

        if (historical_offset >= other_offset + other_length) historical_offset -= other_length;
    }

    if (!did_merge) {
        buffer->history_caret = initial_caret;
        _history_make_space(buffer, true, text.length + 14);
        _history_write_u8(buffer, HISTORY_BLOCK_START);
        _history_write_s32(buffer, text.length);
        memcpy(buffer->history + buffer->history_length, text.data, text.length);
        buffer->history_length += text.length;
        _history_write_s32(buffer, offset);
        _history_write_s32(buffer, text.length);
        _history_write_u8(buffer, our_kind);
    }

    buffer->history_caret = buffer->history_length;
}

static
void _buffer_search_refilter(Buffer *buffer, View *view);

static
void _buffer_view_scroll_clamp(Buffer *buffer, View *view)
{
    s32 line = buffer_offset_to_virtual_line_index(buffer, view->focus_offset);
    s32 global_min_line = -line - buffer->margin_lines;
    s32 global_max_line = buffer->lines.length - line - buffer->visible_lines + buffer->margin_lines;
    global_max_line = max(global_max_line, global_min_line);
    view->focus_line_offset_target = min(max(global_min_line, view->focus_line_offset_target), global_max_line);
}

void buffer_view_scroll(Buffer *buffer, View *view, s32 delta)
{
    view->focus_line_offset_target += delta;
    _buffer_view_scroll_clamp(buffer, view);
}

void buffer_view_show(Buffer *buffer, View *view, int where_on_screen)
{
    switch (where_on_screen) {
        case BUFFER_SHOW_TOP: view->focus_line_offset_target = -buffer->margin_lines; break;
        case BUFFER_SHOW_CENTER: view->focus_line_offset_target = -buffer->visible_lines/2; break;
        case BUFFER_SHOW_BOTTOM: view->focus_line_offset_target = -buffer->visible_lines + buffer->margin_lines; break;

        case BUFFER_SHOW_ANYWHERE:
        case BUFFER_SHOW_ANYWHERE_DONT_SMOOTHSCROLL_ONE_LINE_AFTER_INSERT: // Top 1 identifier names 2020
        {
            s32 a = view->focus_line_offset_target;
            if (view->focus_line_offset_target > -buffer->margin_lines) {
                view->focus_line_offset_target = -buffer->margin_lines;
            } else if (view->focus_line_offset_target < -buffer->visible_lines + buffer->margin_lines) {
                view->focus_line_offset_target = -buffer->visible_lines + buffer->margin_lines;
            }
            s32 b = view->focus_line_offset_target;

            if (where_on_screen == BUFFER_SHOW_ANYWHERE_DONT_SMOOTHSCROLL_ONE_LINE_AFTER_INSERT) {
                s32 delta = a - b;
                if (abs(delta) == 1) {
                    view->focus_line_offset_current -= delta*FOCUS_LINE_OFFSET_SUBSTEPS;
                }
            }
        } break;

        default: assert(false);
    }
    _buffer_view_scroll_clamp(buffer, view);
}

void buffer_view_set_focus(Buffer *buffer, View *view, s32 offset)
{
    s32 old_line = buffer_offset_to_virtual_line_index(buffer, view->focus_offset);
    s32 new_line = buffer_offset_to_virtual_line_index(buffer, offset);
    s32 line_delta = old_line - new_line;
    view->focus_line_offset_target += line_delta;
    view->focus_line_offset_current += line_delta*FOCUS_LINE_OFFSET_SUBSTEPS;
    view->focus_offset = offset;
    // NB (Morten, 2020-07-25) We used to reset 'focus_line_offset_current' to 'focus_line_offset_target*FOCUS_LINE_OFFSET_SUBSTEPS' if 'line_delta != 0'. As far as I can tell, the current approach is strictly better than that though. Maybe I'm missing something about why we used to do that though, I am not quite sure...

}

void buffer_view_set_focus_to_focused_selection(Buffer *buffer, View *view)
{
    s32 offset = 0;
    if (view->selections.length > 0) {
        view->search.focused = -1;
        Selection *focused = &view->selections[view->focused_selection];
        offset = focused->carets[focused->focused_end].offset;
    }
    buffer_view_set_focus(buffer, view, offset);
}

void buffer_view_set_focus_to_focused(Buffer *buffer, View *view)
{
    if (view->search.focused >= 0 && view->search.focused < view->search.active) {
        SearchResult search_result = view->search.list[view->search.focused];
        s32 offset = search_result.min;
        buffer_view_set_focus(buffer, view, offset);
    } else {
        buffer_view_set_focus_to_focused_selection(buffer, view);
    }
}

struct VirtualLineList
{
    VirtualLine line;
    VirtualLineList *next;
};

static noinline_function
VirtualLineList *_layout_physical_line(Buffer *buffer, s32 global_offset, str text)
{
    // TODO if the text.length is low enough it is impossible for the line to wrap, so we don't have to do anything here. This probably could improve our performance a lot

    s32 max_virtual_indent = buffer->max_glyphs_per_line/2 - SOFTWRAP_PRE_SPACES;
    s32 tab_width = buffer->tab_width? buffer->tab_width : TAB_WIDTH_DEFAULT;

    s32 relative_offset = 0;
    s32 current_width = 0;
    s32 width_at_last_wrap_option = 0;
    s32 offset_at_last_wrap_option = 0;
    bool set_new_wrap_option = false;
    s32 virtual_indent = 0;
    bool in_leading_spaces = true;

    VirtualLineList *line_list = null;
    VirtualLineList *line_list_last = null;

    line_list = stack_alloc(VirtualLineList, 1);
    line_list_last = line_list;
    line_list->line.start = global_offset + relative_offset;
    line_list->line.end = global_offset + relative_offset;

    while (relative_offset < text.length) {
        if (set_new_wrap_option) {
            set_new_wrap_option = false;
            width_at_last_wrap_option = current_width;
            offset_at_last_wrap_option = global_offset + relative_offset;
        }

        DecodedCodepoint decoded = decode_utf8((u8 *) (text.data + relative_offset), text.length - relative_offset);
        s32 codepoint_offset = global_offset + relative_offset;
        relative_offset += decoded.length;

        bool hide = (decoded.codepoint == '\r' || decoded.codepoint == '\n') && !buffer->show_special_characters;
        if (!hide) {
            s32 glyph_count = glyph_count_for_codepoint(buffer->font, decoded);
            if (decoded.codepoint == '\t') glyph_count = 1;

            if ((current_width + glyph_count - width_at_last_wrap_option)*3 > buffer->max_glyphs_per_line) {
                // Don't wrap when it would leave a significant portion of the preceding line blank
                width_at_last_wrap_option = 0;
            }

            s32 available_width = buffer->max_glyphs_per_line;
            if (width_at_last_wrap_option == 0) --available_width; // Extra space for the hyphen marking that we force-wrapped

            if (current_width + glyph_count > available_width) {
                s32 new_width = 0;

                VirtualLineList *next = stack_alloc(VirtualLineList, 1);
                next->line.start = codepoint_offset;

                if (width_at_last_wrap_option > 0) {
                    next->line.start = offset_at_last_wrap_option;
                    line_list_last->line.end = offset_at_last_wrap_option;
                    new_width += current_width - width_at_last_wrap_option;
                } else {
                    line_list_last->line.flags |= VirtualLine::MARK_CONTINUATION;
                }
                width_at_last_wrap_option = 0;

                next->line.virtual_indent = virtual_indent;
                new_width += next->line.virtual_indent;

                next->line.end = next->line.start;

                line_list_last->next = next;
                line_list_last = next;

                current_width = new_width;
            }

            if (in_leading_spaces) {
                virtual_indent = min(current_width, max_virtual_indent) + SOFTWRAP_PRE_SPACES + 1;
            }
            if (decoded.valid && (decoded.codepoint == ' ' || decoded.codepoint == '\t')) {
                set_new_wrap_option = true;
            } else {
                in_leading_spaces = false;
            }

            if (decoded.codepoint == '\t') {
                s32 base_offset = (line_list_last != line_list)? virtual_indent : 0;
                current_width = current_width + tab_width - ((current_width - base_offset)%tab_width);
            } else {
                current_width += glyph_count;
            }
        }

        if (!hide) line_list_last->line.end = global_offset + relative_offset;
    }

    #undef list_add

    if (text.length > 0 && is_newline(text.data[text.length - 1])) line_list_last->line.flags |= VirtualLine::ENDS_IN_ACTUAL_NEWLINE;
    return(line_list);
}

static
void _buffer_offset_single(s32 *pos, s32 from, s32 to, bool insert)
{
    if (insert) {
        if (*pos >= from) *pos += to - from;
    } else {
        if (*pos >= from && *pos <= to) {
            *pos = from;
        } else if (*pos > to) {
            *pos += from - to;
        }
    }
}

static
void _buffer_redo_highlighting(Buffer *buffer, s32 from_virtual, HighlightState highlight_state)
{
    if (HIGHLIGHT_FUNCTIONS[buffer->highlight_function_index].function) {
        s32 i = from_virtual;
        assert(i == 0 || buffer->lines[i - 1].physical_line_index != buffer->lines[i].physical_line_index);

        s32 highlight_index_delta = 0;
        s32 insert_offset = buffer->lines[i].first_highlight_index;

        while (i < buffer->lines.length && buffer->lines[i].prev_highlight_state != highlight_state) {
            VirtualLine *line = &buffer->lines[i];
            line->first_highlight_index = insert_offset;
            line->prev_highlight_state = highlight_state;

            s32 start = line->start;
            s32 physical_index = buffer->lines[i].physical_line_index;
            while (i + 1 < buffer->lines.length && buffer->lines[i + 1].physical_line_index == physical_index) ++i;
            ++i;
            s32 end;
            s32 insert_offset_end;
            if (i < buffer->lines.length) {
                end = buffer->lines[i].start;
                insert_offset_end = buffer->lines[i].first_highlight_index + highlight_index_delta;
            } else {
                end = buffer_length(buffer);
                insert_offset_end = buffer->highlights.length;
            }
            
            buffer->highlights.remove_range(insert_offset, insert_offset_end);
            highlight_index_delta -= insert_offset_end - insert_offset;

            stack_enter_frame();
            str text = buffer_get_slice(buffer, start, end);
            while (text.length > 0 && is_newline(text.data[text.length - 1])) --text.length;

            HighlightList *highlights = (HIGHLIGHT_FUNCTIONS[buffer->highlight_function_index].function)(text, &highlight_state);
            for (HighlightList *highlight = highlights; highlight; highlight = highlight->next) {
                assert(highlight->highlight.end <= text.length);
                highlight->highlight.start += start;
                highlight->highlight.end += start;
                buffer->highlights.insert(insert_offset, highlight->highlight);
                ++insert_offset;
                ++highlight_index_delta;
            }

            stack_leave_frame();
        }

        for (; i < buffer->lines.length; ++i) {
            if (buffer->lines[i].prev_highlight_state != HighlightState::Invalid) {
                buffer->lines[i].first_highlight_index += highlight_index_delta;
            }
        }
    } else if (buffer->highlights.length > 0) {
        buffer->highlights.clear();
        for (s32 i = 0; i < buffer->lines.length; ++i) {
            buffer->lines[i].first_highlight_index = 0;
            buffer->lines[i].prev_highlight_state = HighlightState::Invalid;
        }
    }
}

static
void _buffer_on_change(Buffer *buffer, s32 edit_min, s32 edit_max, bool insert)
{
    _buffer_move_gap_to_next_sensible_boundary(buffer);

    for (s32 view_index = 0; view_index < array_length(buffer->views); ++view_index) {
        View *view = &buffer->views[view_index];

        _buffer_offset_single(&view->focus_offset, edit_min, edit_max, insert);

        for_each (selection, view->selections) {
            for (s32 i = 0; i < 2; i += 1) {
                _buffer_offset_single(&selection->carets[i].offset, edit_min, edit_max, insert);
            }
        }

        s32 delta = insert? (edit_max - edit_min) : (edit_min - edit_max);
        for (s32 i = 0; i < view->search.total; ++i) {
            SearchResult *range = &view->search.list[i];

            if ((!insert && (range->min < edit_max && range->max > edit_min)) ||
                (insert && (range->min < edit_min && range->max > edit_min)))
            {
                if (view->search.focused == i) {
                    view->search.focused = 0;
                } else if (view->search.focused > i) {
                    --view->search.focused;
                }

                view->search.list[i] = view->search.list[view->search.total - 1];
                --view->search.total;
                --view->search.active;
                --i;
            }
        }

        _buffer_search_refilter(buffer, view);

        for (s32 i = 0; i < view->search.total; ++i) {
            SearchResult *range = &view->search.list[i];
            if (range->min >= edit_min) {
                range->min += delta;
                range->max += delta;
            }
        }
    }

    if (buffer->font && buffer->max_glyphs_per_line > 0) {
        for (s32 i = 0; i < buffer->lines.length; ++i) {
            VirtualLine *line = &buffer->lines[i];
            _buffer_offset_single(&line->start, edit_min, edit_max, insert);
            _buffer_offset_single(&line->end, edit_min, edit_max, insert);
        }
        for (s32 i = 0; i < buffer->highlights.length; ++i) {
            Highlight *highlight = &buffer->highlights[i];
            _buffer_offset_single(&highlight->start, edit_min, edit_max, insert);
            _buffer_offset_single(&highlight->end, edit_min, edit_max, insert);
        }

        if (buffer->lines.length > 0) {
            buffer->lines[0].start = 0;
            buffer->lines[buffer->lines.length - 1].end = buffer_length(buffer);
        } else {
            buffer->highlights.clear();
        }

        s32 line_insert_offset;
        s32 highlight_insert_offset;
        s32 physical_line_index;
        s32 physical_line_delta;
        s32 virtual_line_delta;
        HighlightState highlight_state;

        s32 start_offset, end_offset;
        if (buffer->lines.length == 0) {
            line_insert_offset = 0;
            highlight_insert_offset = 0;
            physical_line_index = 0;
            physical_line_delta = 0;
            virtual_line_delta = 0;
            highlight_state = HighlightState::Default;

            start_offset = 0;
            end_offset = buffer_length(buffer);
        } else {
            s32 edit_end = insert? edit_max : edit_min;

            s32 n0 = buffer_offset_to_virtual_line_index(buffer, edit_min);
            while (n0 > 0 && buffer->lines[n0 - 1].end >= edit_min) --n0;
            while (n0 > 0 && buffer->lines[n0 - 1].physical_line_index == buffer->lines[n0].physical_line_index) --n0;
            s32 n1 = n0;
            physical_line_delta = -1;
            while (n1 + 1 < buffer->lines.length && buffer->lines[n1 + 1].start <= edit_end) {
                if (buffer->lines[n1].physical_line_index != buffer->lines[n1 + 1].physical_line_index) {
                    --physical_line_delta;
                }
                ++n1;
            }
            while (n1 + 1 < buffer->lines.length && buffer->lines[n1 + 1].physical_line_index == buffer->lines[n1].physical_line_index) ++n1;
            ++n1;


            line_insert_offset = n0;
            physical_line_index = buffer->lines[n0].physical_line_index - 1;

            highlight_insert_offset = buffer->lines[n0].first_highlight_index;
            highlight_state = buffer->lines[n0].prev_highlight_state;

            start_offset = buffer->lines[n0].start;
            end_offset = n1 < buffer->lines.length? buffer->lines[n1].start : buffer_length(buffer);

            virtual_line_delta = n1 - n0;
            buffer->lines.remove_range(n0, n1);
        }

        s32 initial_line_insert_offset = line_insert_offset;
        s32 offset = start_offset;
        while (offset < end_offset) {
            s32 line_start = offset;

            bool done = false;
            while (offset < end_offset && !done) {
                s32 actual_offset = offset;
                if (actual_offset >= buffer->a) actual_offset += buffer->b - buffer->a;
                ++offset;

                char c0 = buffer->data[actual_offset];
                if (c0 == '\r' || c0 == '\n') {
                    if (c0 == '\r') {
                        char c1 = 0;
                        s32 c1_offset = actual_offset + 1;
                        if (c1_offset == buffer->a) c1_offset = buffer->b;
                        if (c1_offset < buffer->cap) c1 = buffer->data[c1_offset];
                        if (c1 == '\n') ++offset;
                    }

                    done = true;
                }
            }


            stack_enter_frame();
            str physical_line_text = buffer_get_slice(buffer, line_start, offset);

            VirtualLineList *new_lines = _layout_physical_line(buffer, line_start, physical_line_text);

            if (new_lines) {
                new_lines->line.first_highlight_index = highlight_insert_offset;
            }

            ++physical_line_index;
            ++physical_line_delta;
            for (VirtualLineList *new_line = new_lines; new_line; new_line = new_line->next) {
                new_line->line.physical_line_index = physical_line_index;
                buffer->lines.insert(line_insert_offset, new_line->line);
                ++line_insert_offset;
                ++virtual_line_delta;
            }

            stack_leave_frame();
        }

        for (s32 i = line_insert_offset; i < buffer->lines.length; ++i) {
            buffer->lines[i].physical_line_index += physical_line_delta;
        }

        if (line_insert_offset == buffer->lines.length && (buffer->lines.length == 0 || (buffer->lines[buffer->lines.length - 1].flags & VirtualLine::ENDS_IN_ACTUAL_NEWLINE))) {
            VirtualLine empty_line = {};
            empty_line.start = buffer_length(buffer);
            empty_line.end = empty_line.start;
            empty_line.physical_line_index = buffer->lines.length == 0? 1 : buffer->lines[buffer->lines.length - 1].physical_line_index + 1;
            empty_line.first_highlight_index = highlight_insert_offset;
            buffer->lines.insert(buffer->lines.length, empty_line);
        }

        if (line_insert_offset <= buffer->lines.length) {
            _buffer_redo_highlighting(buffer, initial_line_insert_offset, highlight_state);
        }

        buffer->physical_line_count = buffer->lines[buffer->lines.length - 1].physical_line_index;
    }
}

static
void _buffer_insert(Buffer *buffer, s32 offset, str text, bool ignore_history)
{
    if (text.length > 0) {
        ++buffer->revision;
        for (s32 i = 0; i < array_length(buffer->views); ++i) ++buffer->views[i].revision;
    }

    assert(0 <= offset && offset <= buffer_length(buffer));
    _buffer_make_space(buffer, text.length);
    _buffer_move_gap(buffer, offset);
    memcpy(buffer->data + buffer->a, text.data, text.length);
    buffer->a += text.length;
    _buffer_on_change(buffer, offset, offset + text.length, true);

    if (!ignore_history) _history_add(buffer, true, offset, text);
}

static
str _buffer_delete(Buffer *buffer, s32 from, s32 to, bool ignore_history)
{
    if (from != to) {
        ++buffer->revision;
        for (s32 i = 0; i < 2; ++i) ++buffer->views[i].revision;
    }

    assert(0 <= from && from <= to && to <= buffer_length(buffer));
    _buffer_move_gap(buffer, to);
    buffer->a = from;
    _buffer_on_change(buffer, from, to, false);

    str deleted = {};
    deleted.data = buffer->data + buffer->a;
    deleted.length = to - from;
    if (!ignore_history) _history_add(buffer, false, from, deleted);
    return(deleted);
}

void history_scroll(Buffer *buffer, View *view, s32 direction)
{
    if (buffer->history_length <= 0) return;

    s32 mark_offset = -1;
    s32 previous_line = buffer_offset_to_virtual_line_index(buffer, view->focus_offset);

    s32 temp = buffer->history_caret;
    if (_history_read_u8(buffer, direction < 0) == HISTORY_SENTINEL) {
        // nice, we skipped the first sentinel
    } else {
        buffer->history_caret = temp;
    }

    while (direction != 0) {
        if (direction < 0) {
            u8 kind = _history_read_u8(buffer, true);
            switch (kind) {
                case HISTORY_BOUNDARY: direction = 0; ++buffer->history_caret; break;
                case HISTORY_SENTINEL: ++direction; break;
                case HISTORY_INSERT:
                case HISTORY_DELETE:
                {
                    s32 length = _history_read_s32(buffer, true);
                    s32 offset = _history_read_s32(buffer, true);
                    str text = _history_read_str(buffer, length, true);
                    assert(_history_read_s32(buffer, true) == length);
                    assert(_history_read_u8(buffer, true) == HISTORY_BLOCK_START);

                    if (kind == HISTORY_DELETE) {
                        _buffer_insert(buffer, offset, text, true);
                        mark_offset = offset + text.length;
                    } else {
                        str deleted_text = _buffer_delete(buffer, offset, offset + text.length, true);
                        assert_soft(deleted_text == text);
                        mark_offset = offset;
                    }
                } break;
                case HISTORY_REPEAT_AT: {
                    unimplemented(); // TODO bro
                } break;
                default: assert(false);
            }
        } else {
            u8 kind = _history_read_u8(buffer, false);
            switch (kind) {
                case HISTORY_BOUNDARY: direction = 0; break;
                case HISTORY_SENTINEL: --direction; break;
                case HISTORY_BLOCK_START: {
                    s32 length = _history_read_s32(buffer, false);
                    str text = _history_read_str(buffer, length, false);
                    s32 offset = _history_read_s32(buffer, false);
                    assert(_history_read_s32(buffer, false) == length);
                    u8 kind = _history_read_u8(buffer, false);

                    if (kind == HISTORY_DELETE) {
                        str deleted_text = _buffer_delete(buffer, offset, offset + text.length, true);
                        assert_soft(deleted_text == text);
                        mark_offset = offset;
                    } else if (kind == HISTORY_INSERT) {
                        _buffer_insert(buffer, offset, text, true);
                        mark_offset = offset + text.length;
                    } else {
                        assert(false);
                    }
                } break;
                case HISTORY_REPEAT_AT: {
                    unimplemented(); // TODO bro
                } break;
                default: assert(false);
            }
        }
    }

    if (view && mark_offset != -1) {
        Selection new_selection = {};
        new_selection.start.offset = mark_offset;
        new_selection.end.offset = mark_offset;
        buffer_set_selection(buffer, view, new_selection, BUFFER_SET_SELECTION_SET);

        view->focus_offset = mark_offset;

        // NB (Morten, 2020-06-12) This isn't completly ideal. When we delete/insert multiple disjoint sets of lines in a single step, we only want to compute the line delta based on the last set of lines. That would match how the caret behaves when actually doing the insertions/deletions. In most cases, this is just fine though.
        s32 new_line = buffer_offset_to_virtual_line_index(buffer, view->focus_offset);
        view->focus_line_offset_target += previous_line - new_line;
        view->focus_line_offset_current = view->focus_line_offset_target*FOCUS_LINE_OFFSET_SUBSTEPS;
        buffer_view_show(buffer, view, BUFFER_SHOW_ANYWHERE);
    }
}

void buffer_free(Buffer *buffer)
{
    if (buffer->data) MemBigFree(buffer->data);
    if (buffer->path) heap_free(buffer->path);
    if (buffer->path_display_string.data && !buffer->path_display_string_static) heap_free(buffer->path_display_string.data);
    if (buffer->history) heap_free(buffer->history);
    buffer->lines.free();
    buffer->highlights.free();
    for (s32 i = 0; i < array_length(buffer->views); ++i) {
        buffer->views[i].selections.free();
        if (buffer->views[i].search.list) heap_free(buffer->views[i].search.list);
    }
    *buffer = {};
}

static
void _buffer_reset_view(Buffer *buffer, View *view)
{
    Array<Selection> selections = view->selections;
    SearchResult *search_list = view->search.list;
    s32 search_capacity = view->search.capacity;
    s32 revision = view->revision;
    memset(view, 0, sizeof(View));
    selections.clear();
    view->selections = selections;
    view->selections.append({});
    view->search.list = search_list;
    view->search.capacity = search_capacity;
    view->revision = revision + 1;

    buffer_view_set_focus_to_focused(buffer, view);
    buffer_view_show(buffer, view, BUFFER_SHOW_ANYWHERE);
    view->focus_line_offset_current = view->focus_line_offset_target*FOCUS_LINE_OFFSET_SUBSTEPS;
}

void buffer_reset(Buffer *buffer)
{
    buffer->a = 0;
    buffer->b = buffer->cap;

    buffer->history_length = 0;
    buffer->history_caret = 0;

    buffer->max_glyphs_per_line = 0;
    buffer->show_special_characters = false;
    buffer->lines.clear();

    buffer->highlights.clear();
    buffer->highlight_function_index = 0;

    if (buffer->path) heap_free(buffer->path);
    buffer->path = null;

    if (buffer->path_display_string.data && !buffer->path_display_string_static) {
        heap_free(buffer->path_display_string.data);
        buffer->path_display_string = {};
    }

    ++buffer->revision;
    buffer->last_saved_revision = -1;
    buffer->external_status = Buffer::EXT_SAME;
    buffer->last_save_time = 0;
    buffer->last_save_time_valid = false;

    buffer->newline_mode = (NewlineMode) 0;

    for (s32 i = 0; i < array_length(buffer->views); ++i) {
        _buffer_reset_view(buffer, &buffer->views[i]);
    }
}

void _buffer_redo_full_layout(Buffer *buffer)
{
    s32 focus_offsets[alen(buffer->views)];
    s32 focus_line_offsets[alen(buffer->views)];
    for (s32 i = 0; i < alen(buffer->views); ++i) {
        focus_offsets[i] = buffer->views[i].focus_offset;
        focus_line_offsets[i] = buffer->views[i].focus_line_offset_target;
    }

    _buffer_on_change(buffer, 0, 0, false);

    for (s32 i = 0; i < alen(buffer->views); ++i) {
        buffer->views[i].focus_offset = focus_offsets[i];
        buffer->views[i].focus_line_offset_target = focus_line_offsets[i];
        _buffer_view_scroll_clamp(buffer, &buffer->views[i]);
        buffer->views[i].focus_line_offset_current = buffer->views[i].focus_line_offset_target*FOCUS_LINE_OFFSET_SUBSTEPS;
    }
}

void buffer_set_tab_width(Buffer *buffer, s32 tab_width)
{
    assert(TAB_WIDTH_MIN <= tab_width && tab_width <= TAB_WIDTH_MAX);
    if (tab_width != buffer->tab_width) {
        buffer->tab_width = tab_width;
        buffer->lines.clear();
        _buffer_redo_full_layout(buffer);
    }
}

void buffer_set_layout_parameters(Buffer *buffer, Font *font, s32 max_glyphs_per_line, s32 visible_lines, s32 margin_lines, bool show_special_characters)
{
    bool changed = false;
    changed |= font != buffer->font;
    changed |= max_glyphs_per_line != buffer->max_glyphs_per_line;
    changed |= show_special_characters != buffer->show_special_characters;
    if (changed) {
        buffer->font = font;
        buffer->max_glyphs_per_line = max_glyphs_per_line;
        buffer->show_special_characters = show_special_characters;
        buffer->lines.clear();

        _buffer_redo_full_layout(buffer);
    }

    if (buffer->visible_lines != visible_lines || buffer->margin_lines != margin_lines) {
        buffer->visible_lines = visible_lines;
        buffer->margin_lines = margin_lines;
        for (s32 i = 0; i < alen(buffer->views); ++i) {
            buffer_view_set_focus_to_focused(buffer, &buffer->views[i]);
            buffer_view_show(buffer, &buffer->views[i], BUFFER_SHOW_ANYWHERE);
            buffer->views[i].focus_line_offset_current = buffer->views[i].focus_line_offset_target*FOCUS_LINE_OFFSET_SUBSTEPS;
        }
    }
}

void buffer_override_highlight_mode(Buffer *buffer, s32 highlight_function_index)
{
    buffer->highlight_function_index = highlight_function_index;
    if (buffer->lines.length > 0) {
        buffer->highlights.clear();
        for (s32 i = 0; i < buffer->lines.length; ++i) {
            buffer->lines[i].first_highlight_index = 0;
            buffer->lines[i].prev_highlight_state = HighlightState::Invalid;
        }
        _buffer_redo_highlighting(buffer, 0, HighlightState::Default);
    }
}

str buffer_get_slice(Buffer *buffer, s32 min, s32 max)
{
    assert(0 <= min && min <= max && max <= buffer_length(buffer));

    str result = {};
    result.length = max - min;

    if (max <= buffer->a) {
        result.data = (char *) buffer->data + min;
    } else if (min >= buffer->a) {
        result.data = (char *) buffer->data + buffer->b + (min - buffer->a);
    } else {
        result.data = stack_alloc(char, result.length);
        s32 first_half = buffer->a - min;
        memcpy(result.data, buffer->data + min, first_half);
        memcpy(result.data + first_half, buffer->data + buffer->b, result.length - first_half);
    }
    return(result);
}

str buffer_get_virtual_line_text(Buffer *buffer, s32 virtual_line_index)
{
    VirtualLine line = buffer->lines[virtual_line_index];
    str text = buffer_get_slice(buffer, line.start, line.end);
    return(text);
}

s32 buffer_offset_to_virtual_line_index(Buffer *buffer, s32 offset)
{
    Slice<VirtualLine> parts[2];
    buffer->lines.get_parts(parts);
    s32 part_index = (parts[1].length > 0 && offset >= parts[1][0].start)? 1 : 0;
    s32 index_offset = part_index == 1? (s32) parts[0].length : 0;
    Slice<VirtualLine> relevant = parts[part_index];

    s32 min = 0;
    s32 max = (s32) relevant.length;
    while (min != max) {
        s32 mid = (min + max)/2;
        if (offset >= relevant[mid].start) {
            if (mid + 1 >= relevant.length || relevant[mid + 1].start > offset) {
                min = mid;
                max = mid;
            } else {
                min = mid + 1;
            }
        } else {
            max = mid;
        }
    }
    return(index_offset + min);
}

s32v2 buffer_offset_to_layout_offset(Buffer *buffer, View *view, s32 offset)
{
    s32 virtual_line_index = buffer_offset_to_virtual_line_index(buffer, offset);
    assert(virtual_line_index >= 0 && virtual_line_index < buffer->lines.length);
    VirtualLine *virtual_line = &buffer->lines[virtual_line_index];
    s32 glyph_count = virtual_line->virtual_indent;

    s32 tab_width = buffer->tab_width? buffer->tab_width : TAB_WIDTH_DEFAULT;

    stack_enter_frame();
    str text = buffer_get_slice(buffer, virtual_line->start, virtual_line->end);
    s32 global_offset = virtual_line->start;
    s32 line_offset = 0;
    while (global_offset < offset && line_offset < text.length) {
        DecodedCodepoint decoded = decode_utf8((u8 *) text.data + line_offset, text.length - line_offset);
        global_offset += decoded.length;
        line_offset += decoded.length;

        if (decoded.codepoint == '\t') {
            glyph_count = virtual_line->virtual_indent + round_up(glyph_count - virtual_line->virtual_indent + 1, tab_width);
        } else {
            s32 extra_glyph_count = glyph_count_for_codepoint(buffer->font, decoded);
            glyph_count += extra_glyph_count;
        }

    }
    stack_leave_frame();

    s32v2 pos = {};
    pos.y = virtual_line_index * buffer->font->metrics.line_height;
    pos.x = glyph_count * buffer->font->metrics.advance;
    pos.x += view->layout_offset.x;
    pos.y += view->layout_offset.y;
    return(pos);
}

static
bool _buffer_step(Buffer *buffer, s32 direction, s32 *offset, s32 *codepoint, bool unify_newlines)
{
    bool could_step = false;
    if (direction == 1) {
        s32 o = *offset;
        s32 l = buffer->a - o;
        if (o >= buffer->a) {
            o += buffer->b - buffer->a;
            l = buffer->cap - o;
        }
        if (l) {
            could_step = true;
            if (unify_newlines && is_newline(buffer->data[o])) {
                s32 newline_length = (l >= 2 && buffer->data[o + 0] == '\r' && buffer->data[o + 1] == '\n')? 2 : 1;
                *offset += newline_length;
                if (codepoint) *codepoint = '\n';
            } else {
                DecodedCodepoint decoded = decode_utf8((u8 *) buffer->data + o, l);
                *offset += decoded.length;
                if (codepoint) *codepoint = decoded.codepoint;
            }
        }
    } else if (direction == -1) {
        s32 o = *offset;
        s32 l = o;
        if (o > buffer->a) {
            o += buffer->b - buffer->a;
            l = o - buffer->b;
        }

        if (l) {
            could_step = true;

            u8 data[4];
            if (l > 4) l = 4;
            memcpy(data, buffer->data + o - l, l);

            if (unify_newlines && is_newline(data[l - 1])) {
                s32 newline_length = (l >= 2 && data[l - 2] == '\r' && data[l - 1] == '\n')? 2 : 1;
                *offset -= newline_length;
                if (codepoint) *codepoint = '\n';
            } else {
                DecodedCodepoint decoded = {};
                for (s32 i = 1; i <= l; ++i) {
                    if (utf8_expected_length(data[l - i]) == i) {
                        decoded = decode_utf8(data + l - i, i);
                        break;
                    }
                }

                if (decoded.valid) {
                    *offset -= decoded.length;
                    if (codepoint) *codepoint = decoded.codepoint;
                } else {
                    *offset -= 1;
                    if (codepoint) *codepoint = data[l - 1];
                }
            }
        }
    } else {
        assert(false);
    }
    return(could_step);
}

s32 buffer_layout_offset_to_offset(Buffer *buffer, View *view, s32v2 layout_offset)
{
    layout_offset.x -= view->layout_offset.x;
    layout_offset.y -= view->layout_offset.y;

    s32 virtual_line_index = layout_offset.y / buffer->font->metrics.line_height;
    s32 offset;

    if (virtual_line_index < 0) {
        offset = 0;
    } else if (virtual_line_index >= buffer->lines.length) {
        offset = buffer_length(buffer);
    } else {
        stack_enter_frame();
        VirtualLine *virtual_line = &buffer->lines[virtual_line_index];
        str text = buffer_get_virtual_line_text(buffer, virtual_line_index);

        offset = virtual_line->start;

        s32 advance = buffer->font->metrics.advance;
        s32 tab_width = (buffer->tab_width? buffer->tab_width : TAB_WIDTH_DEFAULT)*advance;
        s32 x_offset_initial = virtual_line->virtual_indent*advance;
        s32 x_offset = x_offset_initial;
        s32 target_x_offset = layout_offset.x - advance/2;
        while (x_offset < target_x_offset && text.length > 0) {
            DecodedCodepoint decoded = decode_utf8((u8 *) text.data, text.length);
            text = slice(text, decoded.length);
            offset += decoded.length;

            if (decoded.codepoint == '\t') {
                x_offset = x_offset_initial + round_up(x_offset - x_offset_initial + 1, tab_width);
            } else {
                s32 length = glyph_count_for_codepoint(buffer->font, decoded);
                x_offset += length*advance;
            }
        }

        if (offset > virtual_line->start && offset >= virtual_line->end && !(virtual_line->flags & VirtualLine::ENDS_IN_ACTUAL_NEWLINE) && virtual_line_index + 1 != buffer->lines.length) {
            _buffer_step(buffer, -1, &offset, null, true);
        }

        stack_leave_frame();
    }

    return(offset);
}

static
s32 _buffer_step_codepoints(Buffer *buffer, s32 *offset, s32 steps, bool stop_at_eol)
{
    s32 sign = steps > 0? 1 : -1;
    s32 actual_steps = 0;
    while (steps) {
        s32 codepoint = -1;
        s32 new_offset = *offset;
        bool could_step = _buffer_step(buffer, sign, &new_offset, &codepoint, true);
        if (could_step && stop_at_eol && is_newline(codepoint)) could_step = false;

        if (could_step) {
            *offset = new_offset;
            steps -= sign;
            actual_steps += sign;
        } else {
            steps = 0;
        }
    }
    return(actual_steps);
}

static
s32 _buffer_step_caret_codepoints(Buffer *buffer, Caret *caret, s32 steps, bool stop_at_eol)
{
    caret->target_line_offset = (stop_at_eol && steps == S32_MAX)? S32_MAX : 0;
    return(_buffer_step_codepoints(buffer, &caret->offset, steps, stop_at_eol));
}

static
bool _buffer_line_is_blank(Buffer *buffer, s32 offset)
{
    s32 i, c;
    i = offset;
    while (_buffer_step(buffer, -1, &i, &c, true) && c != '\n') if (!is_whitespace(c)) return(false);
    i = offset;
    while (_buffer_step(buffer, 1, &i, &c, true) && c != '\n') if (!is_whitespace(c)) return(false);
    return(true);
}

static
s32 _buffer_get_indent_size(Buffer *buffer, s32 offset, bool from_start)
{
    if (!buffer->tab_width) buffer->tab_width = TAB_WIDTH_DEFAULT;

    s32 direction = -1;
    if (from_start) {
        _buffer_step_codepoints(buffer, &offset, S32_MIN, true);
        direction = 1;
    }
    s32 indent_size = 0;
    s32 c;
    while (_buffer_step(buffer, direction, &offset, &c, false) && is_whitespace(c)) {
        if (c == ' ') {
            ++indent_size;
        } else {
            indent_size = round_up(indent_size + 1, buffer->tab_width);
        }
    }
    return(indent_size);
}

static
s32 _buffer_step_lines(Buffer *buffer, s32 *offset, s32 steps)
{
    s32 actual_lines = 0;
    s32 sign = steps > 0? 1 : -1;

    while (steps) {
        s32 codepoint = -1;
        bool could_step = _buffer_step(buffer, sign, offset, &codepoint, true);
        if (!could_step) {
            steps = 0;
        } else if (is_newline(codepoint)) {
            steps -= sign;
            actual_lines += sign;
        }
    }

    return(actual_lines);
}

static
s32 _buffer_step_caret_lines(Buffer *buffer, Caret *caret, s32 steps)
{
    if (!caret->target_line_offset) {
        s32 temp = caret->offset;
        caret->target_line_offset = -_buffer_step_codepoints(buffer, &temp, S32_MIN, true);
    }

    s32 offset = caret->offset;
    s32 actual_lines = _buffer_step_lines(buffer, &offset, steps);
    _buffer_step_codepoints(buffer, &offset, S32_MIN, true);
    _buffer_step_codepoints(buffer, &offset, caret->target_line_offset, true);
    caret->offset = offset;

    return(actual_lines);
}

static
void _buffer_normalize_add_indent(Buffer *buffer, str *normalized, s32 count)
{
    s32 extra_bytes = count;
    if (buffer->tab_mode == TabMode::HARD) {
        extra_bytes -= (count/buffer->tab_width)*(buffer->tab_width - 1);
    }
    stack_grow_allocation(normalized->data, normalized->length + extra_bytes);

    while (count > 0) {
        if (count >= buffer->tab_width && buffer->tab_mode == TabMode::HARD) {
            normalized->data[normalized->length++] = '\t';
            count -= buffer->tab_width;
        } else {
            normalized->data[normalized->length++] = ' ';
            --count;
        }
    }
}

static
void _buffer_normalize_count_indent(Buffer *buffer, str text, s32 *indent, s32 *indent_bytes)
{
    for (s32 i = 0; i < text.length; ++i) {
        if (text.data[i] == ' ') {
            ++*indent;
        } else if (text.data[i] == '\t') {
            *indent += buffer->tab_width;
        } else {
            break;
        }
        if (indent_bytes) ++*indent_bytes;
    }
}

static
str _buffer_normalize_newlines_for_insert(Buffer *buffer, str text, s32 indent, s32 add_newline)
{
    if (!buffer->tab_width) buffer->tab_width = TAB_WIDTH_DEFAULT;

    if (text.length >= 1 && is_newline(text[0])) {
        s32 i = 1;
        if (text.length >= 2 && text[0] == '\r' && text[1] == '\n') i = 2;
        text = slice(text, i);
        add_newline = -1;
    }

    s32 newlines = 0;
    for (s32 i = 0; i < text.length; ++i) if (is_newline(text[i])) ++newlines;

    str newline = NEWLINE[(s32) buffer->newline_mode];

    s32 first_actual_indent = 0;
    _buffer_normalize_count_indent(buffer, text, &first_actual_indent, 0);

    str normalized = {};
    normalized.data = stack_alloc(char, 0);

    if (add_newline == -1) {
        stack_append(&normalized, newline);
    }

    s32 i0 = 0;
    while (i0 < text.length) {
        s32 i1 = i0;
        while (i1 < text.length && !is_newline(text[i1])) ++i1;
        str line = slice(text, i0, i1);

        if (i0 > 0 || add_newline != 0) {
            s32 actual_indent = 0;
            s32 actual_indent_bytes = 0;
            _buffer_normalize_count_indent(buffer, line, &actual_indent, &actual_indent_bytes);
            line = slice(line, actual_indent_bytes);

            s32 delta = indent + actual_indent - first_actual_indent;
            if (delta > 0 && line.length > 0) {
                _buffer_normalize_add_indent(buffer, &normalized, delta);
            }
        }

        stack_append(&normalized, line);

        i0 = i1;
        if (i0 < text.length) {
            i0 += (text[i0] == '\r' && i0 + 1 < text.length && text[i0 + 1] == '\n')? 2 : 1;
            stack_append(&normalized, newline);
        }
    }

    if (text.length == 0 && indent && add_newline != 0) {
        _buffer_normalize_add_indent(buffer, &normalized, indent);
    }

    if (add_newline == 1) {
        stack_append(&normalized, newline);
    }

    assert(normalized.length == stack_get_allocation_size(normalized.data));

    return(normalized);
}

bool buffer_set_path(Buffer *buffer, Path path, Path relative_to)
{
    stack_enter_frame();
    Path full_path = path_make_absolute(path, relative_to);
    bool ok = !!full_path;
    if (ok) {
        if (buffer->path) heap_free(buffer->path);
        buffer->path = heap_copy(full_path);
        buffer_update_display_string(buffer, relative_to);

        stack_enter_frame();
        str extension = path_extension(buffer->path);
        buffer->highlight_function_index = highlight_get_function_index(extension);
        stack_leave_frame();
    }
    stack_leave_frame();

    return(ok);
}

void buffer_update_display_string(Buffer *buffer, Path relative_to)
{
    if (buffer->path && !buffer->path_display_string_static) {
        if (buffer->path_display_string.data) heap_free(buffer->path_display_string.data);
        stack_enter_frame();
        str new_string = path_to_str_relative(buffer->path, relative_to);
        buffer->path_display_string = heap_copy(new_string);
        stack_leave_frame();
    }
}


void _buffer_update_last_save_time(Buffer *buffer)
{
    buffer->last_save_time_valid = false;

    stack_enter_frame();
    wchar_t *open_path  = _path_to_extended_format(buffer->path);
    void *handle = win32::CreateFileW(open_path, 0, win32::FILE_SHARE_READ | win32::FILE_SHARE_WRITE | win32::FILE_SHARE_DELETE, null, win32::OPEN_EXISTING, win32::FILE_ATTRIBUTE_NORMAL, null);
    stack_leave_frame();

    if (handle != ((void *) -1)) {
        win32::by_handle_file_information info;
        s32 result = win32::GetFileInformationByHandle(handle, &info);
        if (result) {
            buffer->last_save_time_valid = true;
            buffer->last_save_time = (((u64) info.LastWriteTime.High) << 32ull) | ((u64) info.LastWriteTime.Low);
            buffer->external_status = Buffer::EXT_SAME;
        }
        win32::CloseHandle(handle);
    }
}

bool buffer_check_for_external_changes(Buffer *buffer)
{
    int old_status = buffer->external_status;
    if (buffer->last_save_time_valid) {
        stack_enter_frame();
        wchar_t *open_path  = _path_to_extended_format(buffer->path);
        void *handle = win32::CreateFileW(open_path, 0, win32::FILE_SHARE_READ | win32::FILE_SHARE_WRITE | win32::FILE_SHARE_DELETE, null, win32::OPEN_EXISTING, win32::FILE_ATTRIBUTE_NORMAL, null);
        stack_leave_frame();

        u32 open_error = win32::GetLastError();

        bool last_write_time_ok = false;
        u64 last_write_time;
        if (handle != ((void *) -1)) {
            win32::by_handle_file_information info = {0};
            s32 result = win32::GetFileInformationByHandle(handle, &info);
            last_write_time_ok = result != 0;
            last_write_time = (((u64) info.LastWriteTime.High) << 32ull) | ((u64) info.LastWriteTime.Low);
            win32::CloseHandle(handle);
        }

        if (last_write_time_ok && last_write_time != buffer->last_save_time) {
            buffer->external_status = Buffer::EXT_MODIFIED;
        } else if (open_error) {
            buffer->external_status = Buffer::EXT_DELETED;
        } else {
            buffer->external_status = Buffer::EXT_SAME;
        }
    }
    return(buffer->external_status != old_status);
}

IoError buffer_load(Buffer *buffer)
{
    assert(!buffer->no_user_input && buffer->path);

    IoError error = IoError::OK;

    stack_enter_frame();
    wchar_t *open_path = _path_to_extended_format(buffer->path);
    void *handle = win32::CreateFileW(open_path, win32::GENERIC_READ,
                                      win32::FILE_SHARE_READ, null,
                                      win32::OPEN_EXISTING,
                                      win32::FILE_ATTRIBUTE_NORMAL, null);
    stack_leave_frame();

    if (handle == ((void *) -1)) {
        u32 error_code = win32::GetLastError();
        switch (error_code) {
            case win32::ERROR_FILE_NOT_FOUND: error = IoError::FILE_NOT_FOUND; break;
            case win32::ERROR_PATH_NOT_FOUND: error = IoError::DIRECTORY_NOT_FOUND; break;
            default: error = IoError::UNKNOWN_ERROR; break;
        }
    } else {
        s64 size;
        if (win32::GetFileSizeEx(handle, &size) == 0) {
            error = IoError::UNKNOWN_ERROR;
        } else if (size > S32_MAX) {
            error = IoError::UNKNOWN_ERROR;
        } else {
            s64 allocation_size = round_up(max(size, 1), 64*1024);
            char *data = (char *) MemBigAlloc(allocation_size);

            u32 temp;
            s32 read_result = win32::ReadFile(handle, data, (u32) size, &temp, null);
            if (!read_result) {
                error = IoError::UNKNOWN_ERROR;
            } else {
                if (buffer->data) MemBigFree(buffer->data);
                buffer->cap = (s32) allocation_size;
                buffer->a = (s32) size;
                buffer->b = buffer->cap;
                buffer->data = data;
            }
        }

        win32::CloseHandle(handle);
    }

    if (error == IoError::OK) {
        buffer->last_saved_revision = buffer->revision;

        for (s32 i = 0; i < array_length(buffer->views); ++i) {
            _buffer_reset_view(buffer, &buffer->views[i]);
        }

        _buffer_update_last_save_time(buffer);

        buffer->lines.clear();
        buffer->highlights.clear();

        _buffer_on_change(buffer, 0, 0, false);

        buffer->history_length = 0;
        buffer->history_caret = 0;


        // Decide on newline mode and tab width
        s32 tab_count = 0;
        s32 leading_space_count[TAB_WIDTH_MAX*2] = {0};

        s32 lf_count = 0;
        s32 cr_count = 0;
        s32 crlf_count = 0;

        {
            char *start = buffer->data;
            char *end = buffer->data + buffer->a; // Note (Morten): This loop is ok because we manually set up the data for the buffer further up in this function.

            while (start < end) {
                // Count spaces or tab at line start
                if (*start == '\t') {
                    ++tab_count;
                } else {
                    s32 space_count = 0;
                    while (space_count <= alen(leading_space_count) && start < end && *start == ' ') ++start, ++space_count;
                    if (space_count) leading_space_count[space_count - 1] += 1;
                }

                // Find line ending
                while (start < end) {
                    char c = *start++;
                    if (c == '\n') {
                        ++lf_count;
                        break;
                    } else if (c == '\r') {
                        if (start < end && *start == '\n') {
                            ++crlf_count;
                            ++start;
                        } else {
                            ++lf_count;
                        }
                        break;
                    }
                }
            }
        }

        buffer->tab_width = TAB_WIDTH_DEFAULT;
        if (tab_count > 0) {
            buffer->tab_mode = TabMode::HARD;
        } else {
            buffer->tab_mode = TabMode::SOFT;

            s32 indent_width_likeliness = S32_MIN;
            for (s32 width = TAB_WIDTH_MIN; width <= TAB_WIDTH_MAX; ++width) {
                s32 likeliness = 0;
                for (s32 i = 0; i < alen(leading_space_count); ++i) {
                    s32 count = leading_space_count[i];
                    if ((i + 1) % width == 0) {
                        likeliness += count / ((i + 1) / width);
                    } else {
                        likeliness -= count;
                    }
                }
                if (likeliness > indent_width_likeliness) {
                    buffer->tab_width = width;
                    indent_width_likeliness = likeliness;
                }
            }
        }

        s32 newline_modes = 0;
        if (lf_count > 0) {
            ++newline_modes;
            buffer->newline_mode = NewlineMode::LF;
        }
        if (cr_count > 0) {
            ++newline_modes;
            buffer->newline_mode = NewlineMode::CR;
        }
        if (crlf_count > 0) {
            ++newline_modes;
            buffer->newline_mode = NewlineMode::CRLF;
        }
        if (newline_modes > 1) {
            debug_printf("Multiple different line ending styles seen (%i LF, %i CR, %i CRLF)\n", lf_count, cr_count, crlf_count);
        }
    }

    return(error);
}

IoError buffer_save(Buffer *buffer, Path alternative_path)
{
    Path path = alternative_path;
    if (!path) path = buffer->path;

    assert(path);

    str content = buffer_move_gap_to_end(buffer);
    IoError result = write_entire_file(path, content);
    if (result == IoError::OK) {
        buffer->last_saved_revision = buffer->revision;
        if (!alternative_path) _buffer_update_last_save_time(buffer);
    }

    return(result);
}

bool buffer_has_internal_changes(Buffer *buffer)
{
    return(!buffer->no_user_input && buffer->last_saved_revision != buffer->revision);
}


void buffer_change_line_endings(Buffer *buffer, NewlineMode new_mode)
{
    buffer->newline_mode = new_mode;
    str newline = NEWLINE[(s32) new_mode];

    buffer_move_gap_to_end(buffer);
    s32 old_length = buffer->a;
    s32 max_new_length = old_length;
    for (s32 i = 0; i < old_length; ++i) if (is_newline(buffer->data[i])) ++max_new_length;
    _buffer_make_space(buffer, max_new_length - old_length);

    stack_enter_frame();
    char *old = stack_alloc(char, old_length + 1);
    memcpy(old, buffer->data, old_length);
    old[old_length] = 0;

    buffer->a = 0;
    for (s32 i = 0; i < old_length; ++i) {
        char c = old[i];
        if (c == '\r' || c == '\n') {
            if (c == '\r' && old[i + 1] == '\n') ++i;
            memcpy(&buffer->data[buffer->a], newline.data, newline.length);
            buffer->a += newline.length;
        } else {
            buffer->data[buffer->a++] = c;
        }
    }
    assert(buffer->a <= buffer->cap && buffer->b == buffer->cap);

    stack_leave_frame();

    buffer->lines.clear();
    for (s32 i = 0; i < alen(buffer->views); ++i) {
        _buffer_reset_view(buffer, &buffer->views[i]);
    }
    _buffer_on_change(buffer, 0, 0, false);
    for (s32 i = 0; i < alen(buffer->views); ++i) {
        buffer_view_set_focus_to_focused_selection(buffer, &buffer->views[i]);
        buffer_view_show(buffer, &buffer->views[i], BUFFER_SHOW_ANYWHERE);
    }

    ++buffer->revision;
    for (s32 i = 0; i < 2; ++i) ++buffer->views[i].revision;
}

void buffer_insert_at_all_carets(Buffer *buffer, View *view, str text)
{
    for (s32 i = 0; i < view->selections.length; ++i) {
        buffer_insert_at_caret(buffer, view, i, text);
    }
    buffer_view_show(buffer, view, BUFFER_SHOW_ANYWHERE_DONT_SMOOTHSCROLL_ONE_LINE_AFTER_INSERT);


}

void buffer_insert_at_caret(Buffer *buffer, View *view, s32 selection_index, str text)
{
    assert(!buffer->no_user_input);

    s32 previous_line = -1;
    if (selection_index == view->focused_selection) {
        previous_line = buffer_offset_to_virtual_line_index(buffer, view->focus_offset);
    }

    stack_enter_frame();
    Selection selection = view->selections[selection_index];
    s32 offset = selection.carets[selection.focused_end].offset;
    s32 indent = _buffer_get_indent_size(buffer, offset, true);
    str normalized = _buffer_normalize_newlines_for_insert(buffer, text, indent, 0);
    _buffer_insert(buffer, offset, normalized, false);
    if (normalized.length > 0 && is_newline(normalized.data[0]) && _buffer_line_is_blank(buffer, offset)) {
        s32 line_start = offset;
        s32 line_end = offset;
        s32 n, c;
        n = line_start;
        while (_buffer_step(buffer, -1, &n, &c, true) && c != '\n') line_start = n;
        n = line_end;
        while (_buffer_step(buffer, 1, &n, &c, true) && c != '\n') line_end = n;
        _buffer_delete(buffer, line_start, line_end, false);
    }
    stack_leave_frame();

    if (previous_line != -1) {
        Selection *focused = &view->selections[view->focused_selection];
        view->focus_offset = focused->carets[focused->focused_end].offset;
        s32 new_line = buffer_offset_to_virtual_line_index(buffer, view->focus_offset);
        s32 delta = previous_line - new_line;
        view->focus_line_offset_target += delta;
        view->focus_line_offset_current += delta*FOCUS_LINE_OFFSET_SUBSTEPS;
    }

    buffer_normalize(buffer, view);
}

void buffer_insert_at_end(Buffer *buffer, str text)
{
    s32 old_focus_lines[alen(buffer->views)];
    for (s32 i = 0; i < alen(buffer->views); ++i) {
        old_focus_lines[i] = buffer_offset_to_virtual_line_index(buffer, buffer->views[i].focus_offset);
    }

    _buffer_insert(buffer, buffer_length(buffer), text, false);

    for (s32 i = 0; i < alen(buffer->views); ++i) {
        View *view = &buffer->views[i];

        // Note (Morten, 2020-07-26) I'm not sure whether I like the effect this produces when the caret is placed at the bottom of a buffer which repeatedly is having text appended (e.g. when looking at <script output>). It also ends up smooth-scrolling the end-of-file symbol below the caret, which looks hella messed up.
        if (view->focus_offset == buffer_length(buffer)) {
            s32 new_focus_line = buffer_offset_to_virtual_line_index(buffer, view->focus_offset);
            s32 delta = old_focus_lines[i] - new_focus_line;
            view->focus_line_offset_target += delta;
            buffer_view_show(buffer, view, BUFFER_SHOW_ANYWHERE);
            view->focus_line_offset_current += delta*FOCUS_LINE_OFFSET_SUBSTEPS;
        }
    }
}

void buffer_insert_on_new_line_at_all_carets(Buffer *buffer, View *view, s32 direction, str text)
{
    s32 old_focus_line = -1;
    for (s32 i = 0; i < view->selections.length; ++i) {
        if (i == view->focused_selection) old_focus_line = buffer_offset_to_virtual_line_index(buffer, view->focus_offset);

        Selection *selection = &view->selections[i];
        s32 offset = selection->carets[selection->focused_end].offset;

        s32 temp_offset = offset;
        while (_buffer_line_is_blank(buffer, temp_offset) && _buffer_step_lines(buffer, &temp_offset, -direction));
        s32 indent = _buffer_get_indent_size(buffer, temp_offset, true);

        _buffer_step_codepoints(buffer, &offset, direction < 0? S32_MIN : S32_MAX, true);

        stack_enter_frame();
        str normalized = _buffer_normalize_newlines_for_insert(buffer, text, indent, -direction);
        _buffer_insert(buffer, offset, normalized, false);
        stack_leave_frame();

        s32 start = offset;
        s32 end = offset + normalized.length;
        s32 newline_length = NEWLINE[(s32) buffer->newline_mode].length;
        if (direction == -1) {
            end -= newline_length;
        } else {
            start += newline_length;
        }

        selection->start = { start, 0 };
        selection->end = { end, S32_MAX };
        selection->focused_end = 1;
    }
    buffer_normalize(buffer, view);

    Selection *focused = &view->selections[view->focused_selection];
    view->focus_offset = focused->carets[focused->focused_end].offset;
    s32 new_focus_line = buffer_offset_to_virtual_line_index(buffer, view->focus_offset);
    s32 delta = old_focus_line - new_focus_line;
    view->focus_line_offset_target += delta;
    view->focus_line_offset_current += delta*FOCUS_LINE_OFFSET_SUBSTEPS;
    buffer_view_show(buffer, view, BUFFER_SHOW_ANYWHERE_DONT_SMOOTHSCROLL_ONE_LINE_AFTER_INSERT);
}

void buffer_insert_tabs_at_all_carets(Buffer *buffer, View *view)
{
    if (!buffer->tab_width) buffer->tab_width = TAB_WIDTH_DEFAULT;
    char spaces[] = "                ";
    static_assert(sizeof(spaces) - 1 >= TAB_WIDTH_MAX);

    s32 old_focus_line = -1;
    for (s32 i = 0; i < view->selections.length; ++i) {
        if (i == view->focused_selection) old_focus_line = buffer_offset_to_virtual_line_index(buffer, view->focus_offset);

        Selection *selection = &view->selections[i];
        s32 offset = selection->carets[selection->focused_end].offset;

        str space_string;
        if (buffer->tab_mode == TabMode::SOFT) {
            s32 temp = offset;
            s32 depth = _buffer_step_codepoints(buffer, &temp, S32_MIN, true);
            depth = -depth;
            s32 space_count = buffer->tab_width - (depth%buffer->tab_width);
            space_string = { spaces, space_count };
        } else {
            space_string = lit_to_str("\t");
        }
        _buffer_insert(buffer, offset, space_string, false);
    }
    buffer_normalize(buffer, view);

    Selection *focused = &view->selections[view->focused_selection];
    view->focus_offset = focused->carets[focused->focused_end].offset;
    s32 new_focus_line = buffer_offset_to_virtual_line_index(buffer, view->focus_offset);
    s32 delta = old_focus_line - new_focus_line;
    view->focus_line_offset_target += delta;
    view->focus_line_offset_current += delta*FOCUS_LINE_OFFSET_SUBSTEPS;
    buffer_view_show(buffer, view, BUFFER_SHOW_ANYWHERE);
}

static
bool _cmp_search_result(SearchResult *left, SearchResult *right)
{
    return(left->min <= right->min);
}

static
void _buffer_search_refilter(Buffer *buffer, View *view)
{
    s32 old_focus_min = -1;
    if (view->search.focused >= 0 && view->search.focused < view->search.active) {
        old_focus_min = view->search.list[view->search.focused].min;
    }

    view->search.focused = -1;
    view->search.active = 0;

    s32 i = 0;
    s32 j = view->search.total - 1;
    while (1) {
        while (i < view->search.total && (view->search.list[i].flags & view->search.filters) == view->search.filters) ++i;
        while (j >= 0 && (view->search.list[j].flags & view->search.filters) != view->search.filters) --j;
        view->search.active = i;
        if (i >= j) break;
        swap(view->search.list[i], view->search.list[j]);
    }

    stable_sort({ view->search.list, view->search.active }, _cmp_search_result);

    for (s32 i = 0; i < view->search.active; ++i) {
        if (old_focus_min != -1 && view->search.list[i].min >= old_focus_min) {
            view->search.focused = i;
            old_focus_min = -1;
        }
        view->search.list[i].flags &= ~SEARCH_RESULT_HIDE;
    }
    if (view->search.active > 0 && old_focus_min != -1) {
        view->search.focused = 0;
    }
}

void buffer_search_filter(Buffer *buffer, View *view, u32 filters)
{
    view->search.filters = filters;
    _buffer_search_refilter(buffer, view);
}

void buffer_search(Buffer *buffer, View *view, str needle)
{
    view->search.active = 0;
    view->search.total = 0;
    view->search.focused = -1;
    view->search.filters = SEARCH_RESULT_CASE_MATCH;

    if (needle.length > 0) {
        stack_enter_frame();
        str lowercase = utf8_map(needle, &unicode_lowercase);
        str uppercase = utf8_map(needle, &unicode_uppercase);
        assert(lowercase.length == uppercase.length && lowercase.length == needle.length);

        s32 l0 = 0;
        while (l0 < buffer->lines.length) {
            s32 l1 = l0;
            s32 physical_line = buffer->lines[l0].physical_line_index;
            while (l1 + 1 < buffer->lines.length && buffer->lines[l1 + 1].physical_line_index == physical_line) ++l1;
            s32 start_offset = buffer->lines[l0].start;
            str line = buffer_get_slice(buffer, start_offset, buffer->lines[l1].end);
            l0 = l1 + 1;

            s64 search_offset = 0;
            while (1) {
                s64 match = str_search_ignore_case_internal(slice(line, search_offset), lowercase, uppercase);
                if (match == -1) break;
                search_offset += match;

                SearchResult range = {0};
                range.min = start_offset + search_offset;
                range.max = start_offset + search_offset + (s32) needle.length;

                if (memcmp(line.data + search_offset, needle.data, needle.length) == 0) {
                    range.flags |= SEARCH_RESULT_CASE_MATCH;
                }

                char a = search_offset > 0? line[search_offset - 1] : 0;
                char b = search_offset + needle.length < line.length? line[search_offset + needle.length] : 0;
                if (!(is_ascii_letter(a) || a == '_' || is_ascii_letter(b) || b == '_')) {
                    // Note (Morten, 2020-08-15) This isn't ideal, because we don't account for unicode identifiers
                    range.flags |= SEARCH_RESULT_IDENTIFIER_MATCH;
                }

                if (view->search.total + 1 > view->search.capacity) {
                    view->search.capacity = max(view->search.capacity*2, 64);
                    view->search.list = (SearchResult *) heap_grow(view->search.list, view->search.capacity*sizeof(SearchResult));
                }
                view->search.list[view->search.total++] = range;

                search_offset += needle.length;
            }
        }
        stack_leave_frame();

        _buffer_search_refilter(buffer, view);
    }
}


void buffer_next_search_result(Buffer *buffer, View *view, s32 direction, bool show)
{
    bool any = false;
    for (s32 i = 0; i < view->search.active; ++i) {
        if (!(view->search.list[i].flags & SEARCH_RESULT_HIDE)) {
            any = true;
            break;
        }
    }

    if (any) {
        if (view->search.focused >= 0) {
            view->search.focused += direction;
        } else {
            s32 base_offset = view->selections[view->focused_selection].carets[direction == -1? 1 : 0].offset;
            if (direction == -1) {
                view->search.focused = view->search.active - 1;
                for (s32 i = 0; i < view->search.active; ++i) {
                    if (view->search.list[i].min <= base_offset) {
                        view->search.focused = i;
                    }
                }
            } else {
                view->search.focused = 0;
                for (s32 i = view->search.active - 1; i >= 0; --i) {
                    if (view->search.list[i].min >= base_offset) {
                        view->search.focused = i;
                    }
                }
            }
        }

        if (!direction) direction = 1;

        while (1) {
            if (view->search.focused < 0)  view->search.focused += view->search.active;
            if (view->search.focused >= view->search.active)  view->search.focused -= view->search.active;
            if (!(view->search.list[view->search.focused].flags & SEARCH_RESULT_HIDE)) break;
            view->search.focused += direction;
        }

        if (show) {
            SearchResult focused = view->search.list[view->search.focused];
            buffer_view_set_focus(buffer, view, focused.min);
            buffer_view_show(buffer, view, BUFFER_SHOW_ANYWHERE);
        }
    }
}

void buffer_add_selection_at_active_search_result(Buffer *buffer, View *view, s32 direction)
{
    if (view->search.active > 0) {
        if (view->search.focused < 0) {
            buffer_next_search_result(buffer, view, direction, false);
        }

        bool first = true;
        for (s32 i = 0; i < view->search.active; ++i) {
            if (view->search.list[i].flags & SEARCH_RESULT_HIDE) {
                first = false;
                break;
            }
        }

        SearchResult *active = &view->search.list[view->search.focused];
        active->flags |= SEARCH_RESULT_HIDE;

        Selection added = {};
        added.start.offset = active->min;
        added.end.offset = active->max;
        added.focused_end = 1;

        if (first) view->selections.clear();
        view->focused_selection = view->selections.length;
        view->selections.append(added);

        buffer_next_search_result(buffer, view, direction, false);
        buffer_normalize(buffer, view);

        buffer_view_set_focus(buffer, view, active->max);
        buffer_view_show(buffer, view, BUFFER_SHOW_ANYWHERE);
    }
}

void buffer_expand_all_carets_to_next_search_result(Buffer *buffer, View *view, s32 direction)
{
    if (view->search.active > 0) {
        for_each (selection, view->selections) {
            Caret *caret = &selection->carets[selection->focused_end];
            caret->target_line_offset = 0;

            s32 old = caret->offset;
            if (direction == -1) {
                caret->offset = 0;
                for (s32 i = 0; i < view->search.active; ++i) {
                    if (view->search.list[i].min >= old) break;
                    caret->offset = view->search.list[i].min;
                    if (view->search.list[i].max >= old) break;
                    caret->offset = view->search.list[i].max;
                }
            } else {
                caret->offset = buffer_length(buffer);
                for (s32 i = view->search.active - 1; i >= 0; --i) {
                    if (view->search.list[i].max <= old) break;
                    caret->offset = view->search.list[i].max;
                    if (view->search.list[i].min <= old) break;
                    caret->offset = view->search.list[i].min;
                }
            }
        }
        buffer_normalize(buffer, view);
    }
    buffer_view_set_focus_to_focused_selection(buffer, view);
    buffer_view_show(buffer, view, BUFFER_SHOW_ANYWHERE);
}

void buffer_add_selection_at_all_search_results(Buffer *buffer, View *view)
{
    stack_enter_frame();

    for (s32 i = 0; i < view->search.total; ++i) {
        view->search.list[i].flags &= ~SEARCH_RESULT_HIDE;
    }

    s32 focused_range = 0;
    Slice<Range> select_ranges = {};
    select_ranges.data = stack_alloc(Range, view->selections.length);
    for_each (selection, view->selections) {
        if (selection->start.offset != selection->end.offset) {
            if (for_each_index(selection, view->selections) == view->focused_selection) focused_range = select_ranges.length;
            Range range = { selection->start.offset, selection->end.offset };
            select_ranges.data[select_ranges.length++] = range;
        }
    }
    if (select_ranges.length == 0) {
        Range range = { 0, buffer_length(buffer) };
        select_ranges.data[select_ranges.length++] = range;
    }

    Slice<Selection> new_selections = {};
    new_selections.data = stack_alloc(Selection, view->search.active);

    for_each (range, select_ranges) {
        for (s32 i = 0; i < view->search.active; ++i) {
            SearchResult *search_result = &view->search.list[i];
            if (!(search_result->flags & SEARCH_RESULT_HIDE) && search_result->max > range->min && search_result->min < range->max) {
                search_result->flags |= SEARCH_RESULT_HIDE;
                Selection selection = {};
                selection.start.offset = search_result->min;
                selection.end.offset = search_result->max;
                new_selections.data[new_selections.length++] = selection;
            }
        }
    }

    if (new_selections.length) {
        view->focused_selection = 0;
        view->selections.clear();
        for_each (new_selection, new_selections) view->selections.append(*new_selection);
        buffer_normalize(buffer, view);

        buffer_view_set_focus_to_focused_selection(buffer, view);
        buffer_view_show(buffer, view, BUFFER_SHOW_ANYWHERE);
    }

    stack_leave_frame();
}

void buffer_remove_all_search_results(Buffer *buffer, View *view)
{
    view->search.active = 0;
    view->search.total = 0;
    view->search.focused = -1;
}

void buffer_next_selection(Buffer *buffer, View *view, s32 direction)
{
    view->focused_selection += direction;
    if (view->focused_selection < 0) view->focused_selection += view->selections.length;
    if (view->focused_selection >= view->selections.length) view->focused_selection -= view->selections.length;
    buffer_normalize(buffer, view);

    buffer_view_set_focus_to_focused_selection(buffer, view);
    buffer_view_show(buffer, view, BUFFER_SHOW_ANYWHERE);
}

void buffer_remove_focused_selection(Buffer *buffer, View *view)
{
    if (view->selections.length > 1) {
        view->selections.remove_ordered(view->focused_selection);
        if (view->focused_selection >= view->selections.length) view->focused_selection = 0;
    }
    buffer_normalize(buffer, view);
}

void buffer_collapse_selections(Buffer *buffer, View *view)
{
    for_each (selection, view->selections) {
        Caret *a = &selection->carets[1];
        Caret *b = &selection->carets[0];
        if (selection->focused_end == 1) swap(a, b);
        *a = *b;
        selection->focused_end = 0;
    }
    buffer_normalize(buffer, view);

    buffer_view_set_focus_to_focused_selection(buffer, view);
    buffer_view_show(buffer, view, BUFFER_SHOW_ANYWHERE);
}

void buffer_remove_nonfocused_selection(Buffer *buffer, View *view)
{
    buffer_set_selection(buffer, view, view->selections[view->focused_selection], BUFFER_SET_SELECTION_SET);
    buffer_view_set_focus_to_focused_selection(buffer, view);
    buffer_view_show(buffer, view, BUFFER_SHOW_ANYWHERE);
}

void buffer_change_selection_focused_end(Buffer *buffer, View *view)
{
    s32 focused_end = view->selections[view->focused_selection].focused_end;
    focused_end ^= 1;
    for_each (selection, view->selections) {
        selection->focused_end = focused_end;
    }

    buffer_normalize(buffer, view);
    buffer_view_set_focus_to_focused_selection(buffer, view);
    buffer_view_show(buffer, view, BUFFER_SHOW_ANYWHERE);
}

void buffer_set_selection(Buffer *buffer, View *view, Selection new_selection, int action)
{
    switch (action) {
        case BUFFER_SET_SELECTION_SET:
            view->selections.clear();
        case BUFFER_SET_SELECTION_ADD:
            view->focused_selection = view->selections.length;
            view->selections.append(new_selection);
        break;

        case BUFFER_SET_SELECTION_REPLACE:
            view->selections[view->focused_selection] = new_selection;
        break;

        default: assert(false);
    }

    view->search.focused = -1;
    buffer_normalize(buffer, view);
}

void buffer_add_caret_on_each_selected_line(Buffer *buffer, View *view)
{
    stack_enter_frame();
    s32 new_offsets_capacity = 64;
    Slice<s32> new_offsets = {};
    new_offsets.data = stack_alloc(s32, new_offsets_capacity);

    s32 new_focused = S32_MAX;

    for (s32 selection_index = 0; selection_index < view->selections.length; ++selection_index) {
        if (selection_index == view->focused_selection) new_focused = new_offsets.length;

        s32 offset = view->selections[selection_index].start.offset;
        s32 offset_end = view->selections[selection_index].end.offset;

        _buffer_step_codepoints(buffer, &offset, S32_MIN, true);
        while (offset < offset_end) {
            if (new_offsets.length + 1 > new_offsets_capacity) {
                new_offsets_capacity *= 2;
                stack_grow_allocation(new_offsets.data, new_offsets_capacity * sizeof(s32));
            }

            s32 n = offset;
            s32 c;
            bool add = true;
            while (add) {
                if (!_buffer_step(buffer, 1, &n, &c, false)) add = false;
                if (is_newline(c)) add = false;
                if (is_whitespace(c)) offset = n;
                else                  break;
            }

            if (add) {
                new_offsets.data[new_offsets.length++] = offset;
            }

            _buffer_step_lines(buffer, &offset, 1);
        }
    }

    view->selections.clear();
    view->selections.ensure_length(new_offsets.length);
    for_each (new_offset, new_offsets) {
        Selection s = {};
        s.start.offset = *new_offset;
        s.end.offset = *new_offset;
        view->selections.append(s);
    }
    stack_leave_frame();

    view->focused_selection = min(new_focused, view->selections.length);

    buffer_normalize(buffer, view);
    buffer_view_set_focus_to_focused_selection(buffer, view);
    buffer_view_show(buffer, view, BUFFER_SHOW_ANYWHERE);
}

void buffer_step_all_carets_codepoints(Buffer *buffer, View *view, s32 direction, bool stop_at_eol, bool split_carets)
{
    for_each (selection, view->selections) {
        Caret new_caret = selection->carets[selection->focused_end];
        _buffer_step_caret_codepoints(buffer, &new_caret, direction, stop_at_eol);

        if (split_carets || selection->start.offset != selection->end.offset) {
            selection->carets[selection->focused_end] = new_caret;
        } else {
            selection->start = new_caret;
            selection->end = new_caret;
        }
    }
    buffer_normalize(buffer, view);
    buffer_view_set_focus_to_focused_selection(buffer, view);
    buffer_view_show(buffer, view, BUFFER_SHOW_ANYWHERE);
}

void buffer_step_all_carets_both_ways(Buffer *buffer, View *view, s32 direction, bool stop_at_eol)
{
    for_each (selection, view->selections) {
        Caret start = selection->start;
        Caret end = selection->end;
        _buffer_step_caret_codepoints(buffer, &start, -direction, stop_at_eol);
        _buffer_step_caret_codepoints(buffer, &end, direction, stop_at_eol);
        if (start.offset <= end.offset) {
            selection->start = start;
            selection->end = end;
        } else {
            selection->start = selection->carets[selection->focused_end];
            selection->end = selection->start;
        }
    }
    buffer_normalize(buffer, view);
    buffer_view_set_focus_to_focused_selection(buffer, view);
    buffer_view_show(buffer, view, BUFFER_SHOW_ANYWHERE);
}

void buffer_step_all_carets_lines(Buffer *buffer, View *view, s32 direction, bool grow_selections, bool add_new_carets)
{
    s32 initial_length = view->selections.length;
    for (s32 selection_index = 0; selection_index < initial_length; ++selection_index) {
        Selection *selection = &view->selections[selection_index];
        if (add_new_carets) {
            if (selection_index == view->focused_selection) view->focused_selection = view->selections.length;
            Selection *new_selection = view->selections.push();
            *new_selection = *selection;
            _buffer_step_caret_lines(buffer, &new_selection->start, direction);
            _buffer_step_caret_lines(buffer, &new_selection->end, direction);
        } else {
            bool were_joined = selection->start.offset == selection->end.offset;
            Caret caret = selection->carets[selection->focused_end];
            _buffer_step_caret_lines(buffer, &caret, direction);
            if (were_joined && !grow_selections) {
                selection->start = caret;
                selection->end = caret;
            } else {
                selection->carets[selection->focused_end] = caret;
            }
        }
    }
    buffer_normalize(buffer, view);
    buffer_view_set_focus_to_focused_selection(buffer, view);
    buffer_view_show(buffer, view, BUFFER_SHOW_ANYWHERE);
}

void buffer_step_all_carets_empty_lines(Buffer *buffer, View *view, s32 direction)
{
    s32 initial_length = view->selections.length;
    for (s32 selection_index = 0; selection_index < initial_length; ++selection_index) {
        Selection *selection = &view->selections[selection_index];
        Caret *caret = &selection->carets[selection->focused_end];

        _buffer_step_caret_lines(buffer, caret, direction);
        if (_buffer_line_is_blank(buffer, caret->offset)) {
            while (_buffer_step_caret_lines(buffer, caret, direction) && _buffer_line_is_blank(buffer, caret->offset));
        } else {
            Caret temp = *caret;
            while (_buffer_step_caret_lines(buffer, &temp, direction) && !_buffer_line_is_blank(buffer, temp.offset)) *caret = temp;
        }
        
        if (selection->start.offset > selection->end.offset) {
            swap(selection->start, selection->end);
            selection->focused_end = (selection->focused_end + 1)%2;
        }
        _buffer_step_caret_codepoints(buffer, &selection->start, S32_MIN, true);
        _buffer_step_caret_codepoints(buffer, &selection->end, S32_MAX, true);
    }
    buffer_normalize(buffer, view);
    buffer_view_set_focus_to_focused_selection(buffer, view);
    buffer_view_show(buffer, view, BUFFER_SHOW_ANYWHERE);
}

void buffer_jump_to_physical_line(Buffer *buffer, View *view, s32 physical_line, bool split_carets)
{
    s32 i0 = 0;
    while (i0 < buffer->lines.length && buffer->lines[i0].physical_line_index < physical_line) ++i0;
    s32 i1 = i0;
    while (i1 + 1 < buffer->lines.length && buffer->lines[i1 + 1].physical_line_index == physical_line) ++i1;

    s32 start = buffer->lines[i0].start;
    s32 end = buffer->lines[i1].end;
    s32 n = end, c;
    while (n > start && _buffer_step(buffer, -1, &n, &c, false) && is_newline(c)) end = n;

    if (split_carets) {
        Selection *selection = &view->selections[view->focused_selection];
        Caret *caret = &selection->carets[selection->focused_end];
        s32 other_offset = selection->carets[(selection->focused_end + 1) % 2].offset;
        if (start >= other_offset) {
            caret->offset = end;
            caret->target_line_offset = S32_MAX;
        } else {
            caret->offset = start;
            caret->target_line_offset = 0;
        }
    } else {
        Selection selection = {};
        selection.start.offset = start;
        selection.end.offset = end;
        selection.end.target_line_offset = S32_MAX;
        selection.focused_end = 1;
        buffer_set_selection(buffer, view, selection, BUFFER_SET_SELECTION_SET);
    }
    buffer_normalize(buffer, view);
    buffer_view_set_focus_to_focused_selection(buffer, view);
    buffer_view_show(buffer, view, BUFFER_SHOW_ANYWHERE);
}

void buffer_step_all_carets_to_boundary(Buffer *buffer, View *view, s32 direction, s32 boundary)
{
    for_each (selection, view->selections) {
        Caret *caret = &selection->carets[selection->focused_end];
        caret->target_line_offset = 0;

        _buffer_step(buffer, direction, &caret->offset, null, true);
        s32 n = caret->offset;
        s32 c;
        while (_buffer_step(buffer, direction, &n, &c, true) && c != boundary) caret->offset = n;
    }
    buffer_normalize(buffer, view);
}

void buffer_step_all_carets_words(Buffer *buffer, View *view, s32 direction, bool start)
{
    u64 syms[4] = {};
    #define set(codepoint) syms[codepoint >> 6] |= 1ull << (codepoint & 63);
    set('!'); set('"'); set('#'); set('~');
    set('%'); set('&'); set('\''); set('(');
    set(')'); set('*'); set('+'); set(',');
    set('-'); set('/'); set(':'); set(';');
    set('<'); set('='); set('>'); set('?');
    set('@'); set('['); set(']'); set('\\');
    set('^'); set('{'); set('}'); set('.');
    set('$');
    #undef set
    u64 whitespace = (1ull << ' ') | (1ull << '\t') | (1ull << '\n') | (1ull << '\r');
    #define is_symbol(codepoint) (codepoint < 256? ((syms[codepoint >> 6] >> (codepoint & 63)) & 1) : 0)
    #define is_whitespace(codepoint) (codepoint < 64? ((whitespace >> (codepoint & 63)) & 1) : 0)

    for_each (selection, view->selections) {
        s32 offset = selection->carets[selection->focused_end].offset;

        s32 n = offset;
        s32 c = 0;
        bool s;
        if (start) {
            _buffer_step(buffer, direction, &n, &c, true);
            s = is_symbol(c);
            n = offset;
            while (_buffer_step(buffer, direction, &n, &c, true) && !is_whitespace(c) && (s == is_symbol(c))) offset = n;
            n = offset;
            while (_buffer_step(buffer, direction, &n, &c, true) && is_whitespace(c)) offset = n;
        } else {
            while (_buffer_step(buffer, direction, &n, &c, true) && is_whitespace(c)) offset = n;
            s = is_symbol(c);
            n = offset;
            while (_buffer_step(buffer, direction, &n, &c, true) && !is_whitespace(c) && (s == is_symbol(c))) offset = n;
        }

        selection->carets[selection->focused_end] = { offset };
    }

    #undef is_symbol
    #undef is_whitespace

    buffer_normalize(buffer, view);
    buffer_view_set_focus_to_focused_selection(buffer, view);
    buffer_view_show(buffer, view, BUFFER_SHOW_ANYWHERE);
}


void buffer_step_all_carets_past_whitespace(Buffer *buffer, View *view, s32 direction)
{
    for_each (selection, view->selections) {
        Caret *caret = &selection->carets[selection->focused_end];
        caret->target_line_offset = 0;

        s32 n = caret->offset;
        s32 c;
        while (_buffer_step(buffer, 1, &n, &c, false) && is_whitespace(c)) caret->offset = n;
    }

    buffer_normalize(buffer, view);
}

static
void _buffer_expand(Buffer *buffer, View *view, Selection *selection, s32 delimiter)
{
    if (selection->start.offset == selection->end.offset) selection->focused_end = 1;

    if (delimiter == BUFFER_EXPAND_LINE || delimiter == BUFFER_EXPAND_LINE_WITHOUT_LEADING_SPACES) {
        _buffer_step_caret_codepoints(buffer, &selection->start, S32_MIN, true);
        _buffer_step_caret_codepoints(buffer, &selection->end, S32_MAX, true);

        if (delimiter == BUFFER_EXPAND_LINE_WITHOUT_LEADING_SPACES) {
            s32 n = selection->start.offset;
            s32 c;
            while (_buffer_step(buffer, 1, &n, &c, false) && is_whitespace(c)) selection->start.offset = n;
        }
    } else if (delimiter == BUFFER_EXPAND_ALL) {
        selection->start = { 0, 0 };
        selection->end = { buffer_length(buffer), S32_MAX };
    } else if (delimiter == BUFFER_EXPAND_WORD) {
        // TODO (Morten, 2020-05-10) I guess we want to use language-specific "is identifier" functions

        s32 l = selection->carets[selection->focused_end].offset;
        s32 l0 = l;
        s32 r = l;

        s32 c, n;

        n = l;
        while (_buffer_step(buffer, -1, &n, &c, false) && is_identifier_character_c(c)) l = n;
        n = l;
        while (_buffer_step(buffer, 1, &n, &c, false) && !is_identifier_start_c(c) && n <= l0) l = n;
        n = r;
        while (_buffer_step(buffer, 1, &n, &c, false) && is_identifier_character_c(c)) r = n;

        selection->start = { l, 0 };
        selection->end = { r, 0 };

    } else if (delimiter == BUFFER_EXPAND_BLOCK) {
        s32 base = selection->carets[selection->focused_end].offset;
        s32 previous, last_chance, n;

        n = base;
        while (_buffer_line_is_blank(buffer, n) && _buffer_step_lines(buffer, &n, 1)) base = n;
        s32 target_indent = _buffer_get_indent_size(buffer, base, true);

        s32 l = base;
        s32 r = base;

        n = l;
        previous = S32_MAX;
        last_chance = -1;
        while (true) {
            bool blank = _buffer_line_is_blank(buffer, n);
            s32 indent = blank? -1 : _buffer_get_indent_size(buffer, n, true);
            if (blank && previous != -1) last_chance = l;
            if ((!blank || target_indent == 0) && indent < previous && previous <= target_indent) break;
            if ((!blank && indent < target_indent) || (target_indent == 0 && previous == -1 && indent == 0)) {
                l = (last_chance == -1)? l : last_chance;
                break;
            }
            previous = indent;
            l = n;
            if (!_buffer_step_lines(buffer, &n, -1)) break;
        }

        n = r;
        previous = S32_MAX;
        last_chance = -1;
        while (true) {
            bool blank = _buffer_line_is_blank(buffer, n);
            s32 indent = blank? -1 : _buffer_get_indent_size(buffer, n, true);
            if (blank && previous != -1) last_chance = r;
            if ((!blank || target_indent == 0) && indent < previous && previous <= target_indent) break;
            if ((!blank && indent < target_indent) || (target_indent == 0 && previous == -1 && indent == 0)) {
                r = (last_chance == -1)? r : last_chance;
                break;
            }
            previous = indent;
            r = n;
            if (!_buffer_step_lines(buffer, &n, 1)) break;
        }

        _buffer_step_codepoints(buffer, &l, S32_MIN, true);
        _buffer_step_codepoints(buffer, &r, S32_MAX, true);

        selection->start = { l, 0 };
        selection->end = { r, S32_MAX };


    } else if (delimiter == BUFFER_EXPAND_SINGLE_QUOTE || delimiter == BUFFER_EXPAND_DOUBLE_QUOTE) {
        s32 quote = delimiter & 0xff;
        s32 offset = selection->carets[selection->focused_end].offset;
        s32 n = offset;
        _buffer_step_codepoints(buffer, &n, S32_MIN, true);
        s32 c;
        bool skip_next = false;
        s32 min = -1;
        s32 max = -1;
        while (_buffer_step(buffer, 1, &n, &c, true) && c != '\n') {
            if (c == quote && !skip_next) {
                if (n > offset) {
                    if (min != -1) max = n - 1;
                    break;
                }
                min = min == -1? n : -1;
            }
            skip_next = c == '\\';
        }

        if (max != -1) {
            selection->start = { min };
            selection->end   = { max };
        }
    } else {
        s32 left = (delimiter >> 8) & 0xff;
        s32 right = delimiter & 0xff;

        s32 offset_left, offset_right;

        s32 n, h, c, count;
        bool ok = true;

        if (view->revision == view->revision_at_last_repeatable_expand && selection->start.offset != selection->end.offset) {
            offset_left = selection->start.offset;
            offset_right = selection->end.offset;

            n = offset_left;
            c = -1;
            while (c != left && _buffer_step(buffer, -1, &n, &c, true)) offset_left = n;
            n = offset_right;
            c = -1;
            while (c != right && _buffer_step(buffer, 1, &n, &c, true)) offset_right = n;
        } else {
            s32 offset = selection->carets[selection->focused_end].offset;
            offset_left = offset;
            offset_right = offset;
        }

        s32 highlight_index = -1;
        if (buffer->highlights.length > 0) {
            s32 v = buffer_offset_to_virtual_line_index(buffer, offset_left);
            while (v > 0 && buffer->lines[v - 1].physical_line_index == buffer->lines[v].physical_line_index) --v;
            highlight_index = buffer->lines[v].first_highlight_index;
            while (highlight_index < buffer->highlights.length && buffer->highlights[highlight_index].end < offset_left) ++highlight_index;
        }

        n = offset_left;
        h = highlight_index;
        count = 1;
        while (ok && _buffer_step(buffer, -1, &n, &c, true)) {
            if ((c == left || c == right) && h >= 0 && h < buffer->highlights.length) {
                while (h > 0 && buffer->highlights[h].start > n) --h;
                Highlight highlight = buffer->highlights[h];
                if (highlight.start <= n && n < highlight.end && highlight.kind != HighlightKind::Comment) c = 0;
            }

            if (c == left) --count;
            if (c == right) ++count;
            if (count == 0) {
                offset_left = n + 1;
                break;
            }
        }
        ok &= count == 0;

        n = offset_right;
        h = highlight_index;
        count = 1;
        while (ok && _buffer_step(buffer, 1, &n, &c, true)) {
            if ((c == left || c == right) && h >= 0 && h < buffer->highlights.length) {
                while (h + 1 < buffer->highlights.length && buffer->highlights[h].end < n) ++h;
                Highlight highlight = buffer->highlights[h];
                if (highlight.start < n && n <= highlight.end && highlight.kind != HighlightKind::Comment) c = 0; // NB (Morten, 2020-07-07) n is the index of the character after c, hence this check is different than the one in the loop above
            }

            if (c == left) ++count;
            if (c == right) --count;
            if (count == 0) {
                offset_right = n - 1;
                break;
            }
        }
        ok &= count == 0;

        if (ok) {
            selection->start = { offset_left };
            selection->end = { offset_right };
        }
    }
}

void buffer_expand_all_carets(Buffer *buffer, View *view, s32 delimiter)
{
    for_each (selection, view->selections) {
        _buffer_expand(buffer, view, selection, delimiter);
    }
    buffer_normalize(buffer, view);
    view->revision_at_last_repeatable_expand = view->revision;
    buffer_view_set_focus_to_focused_selection(buffer, view);
    buffer_view_show(buffer, view, BUFFER_SHOW_ANYWHERE);
}

void buffer_expand(Buffer *buffer, View *view, Selection *selection, s32 delimiter)
{
    _buffer_expand(buffer, view, selection, delimiter);
    buffer_normalize(buffer, view);
    view->revision_at_last_repeatable_expand = view->revision;
    buffer_view_set_focus_to_focused_selection(buffer, view);
    buffer_view_show(buffer, view, BUFFER_SHOW_ANYWHERE);
}

void buffer_normalize(Buffer *buffer, View *view)
{
    ++view->revision;

    if (view->selections.length <= 0) {
        view->selections.push();
        view->focused_selection = 0;
    } else {
        for_each (selection, view->selections) {
            if (selection->start.offset > selection->end.offset) {
                swap(selection->start, selection->end);
                selection->focused_end ^= 1;
            }
        }

        bool sorted = false;
        s64 unsorted_length = view->selections.length;
        Selection *unsorted_data = view->selections.data;
        while (!sorted) {
            sorted = true;
            unsorted_length -= 1;
            for (s64 i = 0; i < unsorted_length; i += 1) {
                if (unsorted_data[i].start.offset > unsorted_data[i + 1].start.offset) {
                    swap(unsorted_data[i], unsorted_data[i + 1]);
                    if (view->focused_selection == i) view->focused_selection = i + 1;
                    if (view->focused_selection == i + 1) view->focused_selection = i;
                    sorted = false;
                }
            }
        }

        for (s64 i = 0; i + 1 < view->selections.length;) {
            Selection *a = &view->selections[i + 0];
            Selection *b = &view->selections[i + 1];
            if (a->end.offset >= b->start.offset) {
                if (view->focused_selection >= i + 1) --view->focused_selection;
                a->end = a->end.offset > b->end.offset? a->end : b->end;
                view->selections.remove_ordered(i + 1);
            } else {
                i += 1;
            }
        }
    }

    SearchResult *search_result = view->search.list;
    SearchResult *search_result_end = view->search.list + view->search.active;
    Selection *selection = view->selections.data;
    Selection *selection_end = selection + view->selections.length;
    while (search_result < search_result_end) {
        if (search_result->flags & SEARCH_RESULT_HIDE) {
            while (selection + 1 < selection_end && selection->end.offset < search_result->min) ++selection;
            if (!(search_result->min <= selection->start.offset && selection->end.offset <= search_result->max) || selection->start.offset == selection->end.offset) {
                search_result->flags &= ~SEARCH_RESULT_HIDE;
            }
        }
        ++search_result;
    }
}

void buffer_empty_all_selections(Buffer *buffer, View *view, int action)
{
    s32 old_focus_line = -1;
    for (s32 i = 0; i < view->selections.length; ++i) {
        if (i == view->focused_selection) old_focus_line = buffer_offset_to_virtual_line_index(buffer, view->focus_offset);

        Selection *selection = &view->selections[i];

        if (selection->start.offset == selection->end.offset) {
            if (action == EMPTY_ALL_SELECTIONS_OR_DELETE_ONE_LEFT) {
                s32 delete_steps = 1;

                s32 step_offset = selection->start.offset;
                s32 leading_spaces = 0;
                s32 codepoint;
                while (_buffer_step(buffer, -1, &step_offset, &codepoint, false) && !is_newline(codepoint) && leading_spaces >= 0) {
                    if (codepoint == ' ') {
                        ++leading_spaces;
                    } else {
                        leading_spaces = -1;
                    }
                }
                if (leading_spaces > 0) {
                    if (!buffer->tab_width) buffer->tab_width = TAB_WIDTH_DEFAULT;
                    delete_steps = leading_spaces % buffer->tab_width;
                    if (!delete_steps) delete_steps = buffer->tab_width;
                }

                _buffer_step_codepoints(buffer, &selection->start.offset, -delete_steps, false);
            } else if (action == EMPTY_ALL_SELECTIONS_OR_DELETE_ONE_RIGHT) {
                _buffer_step(buffer, 1, &selection->end.offset, null, true);
            }
        }

        if (selection->start.offset != selection->end.offset) {
            _buffer_delete(buffer, selection->start.offset, selection->end.offset, false);
        }
    }
    buffer_normalize(buffer, view);

    Selection *focused = &view->selections[view->focused_selection];
    view->focus_offset = focused->carets[focused->focused_end].offset;
    s32 new_focus_line = buffer_offset_to_virtual_line_index(buffer, view->focus_offset);
    s32 delta = old_focus_line - new_focus_line;
    view->focus_line_offset_target += delta;
    view->focus_line_offset_current += delta*FOCUS_LINE_OFFSET_SUBSTEPS;
    buffer_view_set_focus_to_focused_selection(buffer, view);
    buffer_view_show(buffer, view, BUFFER_SHOW_ANYWHERE_DONT_SMOOTHSCROLL_ONE_LINE_AFTER_INSERT);
}

void buffer_remove_trailing_spaces(Buffer *buffer, View *view)
{
    assert(!buffer->no_user_input);

    s32 old_focus_line = -1;
    for (s32 i = 0; i < view->selections.length; ++i) {
        if (i == view->focused_selection) old_focus_line = buffer_offset_to_virtual_line_index(buffer, view->focus_offset);

        Selection *selection = &view->selections[i];
        s32 end_offset = selection->carets[selection->focused_end].offset;
        _buffer_step_codepoints(buffer, &end_offset, S32_MAX, true);

        s32 start_offset = end_offset;
        s32 codepoint;
        s32 n = end_offset;
        while (_buffer_step(buffer, -1, &n, &codepoint, false) && (codepoint == ' ' || codepoint == '\t')) start_offset = n;

        if (start_offset != end_offset) _buffer_delete(buffer, start_offset, end_offset, false);
    }
    buffer_normalize(buffer, view);

    Selection *focused = &view->selections[view->focused_selection];
    view->focus_offset = focused->carets[focused->focused_end].offset;
    s32 new_focus_line = buffer_offset_to_virtual_line_index(buffer, view->focus_offset);
    s32 delta = old_focus_line - new_focus_line;
    view->focus_line_offset_target += delta;
    view->focus_line_offset_current += delta*FOCUS_LINE_OFFSET_SUBSTEPS;
    buffer_view_set_focus_to_focused_selection(buffer, view);
    buffer_view_show(buffer, view, BUFFER_SHOW_ANYWHERE_DONT_SMOOTHSCROLL_ONE_LINE_AFTER_INSERT);
}


void buffer_indent_selected_lines(Buffer *buffer, View *view, bool indent)
{
    if (!buffer->tab_width) buffer->tab_width = TAB_WIDTH_DEFAULT;

    stack_enter_frame();

    s32 string_capacity = 64;
    str string = {};
    string.data = stack_alloc(char, string_capacity);

    s32 old_focus_line = -1;
    for (s32 selection_index = 0; selection_index < view->selections.length; ++selection_index) {
        if (selection_index == view->focused_selection) old_focus_line = buffer_offset_to_virtual_line_index(buffer, view->focus_offset);

        Selection *selection = &view->selections[selection_index];
        if (selection->start.offset == selection->end.offset) selection->focused_end = 1;

        s32 offset = selection->start.offset;
        _buffer_step_codepoints(buffer, &offset, S32_MIN, true);

        s32 final_offset = offset;
        s32 initial_offset = offset;
        s32 first_indent = _buffer_get_indent_size(buffer, offset, true);

        s32 delta;
        if (indent) {
            delta = buffer->tab_width - (first_indent % buffer->tab_width);
        } else {
            delta = -(first_indent % buffer->tab_width);
            if (delta == 0) delta = -buffer->tab_width;
        }

        while (offset <= selection->end.offset) {
            s32 end_offset = offset;

            s32 old_indent_start = offset;
            s32 old_indent = 0;

            s32 n = offset;
            s32 c;
            while (_buffer_step(buffer, 1, &n, &c, false) && is_whitespace(c)) {
                offset = n;
                if (c == ' ') {
                    ++old_indent;
                } else {
                    old_indent = round_up(old_indent + 1, buffer->tab_width);
                }
            }
            s32 old_indent_end = offset;

            n = offset;
            bool empty_line = !_buffer_step(buffer, 1, &n, &c, true) || is_newline(c);

            s32 new_indent = max(old_indent + delta, 0);
            if (empty_line) new_indent = 0;

            string.length = 0;
            s32 string_indent = 0;
            if (string_capacity < new_indent) {
                string_capacity = max(string_capacity*2, new_indent);
                stack_grow_allocation(string.data, string_capacity);
            }
            while (string_indent < new_indent) {
                if (buffer->tab_mode == TabMode::HARD && buffer->tab_width <= new_indent - string_indent) {
                    string.data[string.length++] = '\t';
                    string_indent += buffer->tab_width;
                } else {
                    string.data[string.length++] = ' ';
                    string_indent += 1;
                }
            }

            _buffer_delete(buffer, old_indent_start, old_indent_end, false);
            _buffer_insert(buffer, old_indent_start, string, false);

            offset += string.length - (old_indent_end - old_indent_start);
            _buffer_step_codepoints(buffer, &offset, S32_MAX, true);
            final_offset = offset;
            if (!_buffer_step(buffer, 1, &offset, null, true)) offset = S32_MAX;
        }

        selection->end.offset = final_offset;
        selection->start.offset = initial_offset;
    }
    buffer_normalize(buffer, view);

    Selection *focused = &view->selections[view->focused_selection];
    view->focus_offset = focused->carets[focused->focused_end].offset;
    s32 new_focus_line = buffer_offset_to_virtual_line_index(buffer, view->focus_offset);
    s32 delta = old_focus_line - new_focus_line;
    view->focus_line_offset_target += delta;
    view->focus_line_offset_current += delta*FOCUS_LINE_OFFSET_SUBSTEPS;
    buffer_view_show(buffer, view, BUFFER_SHOW_ANYWHERE);

    stack_leave_frame();
}

void buffer_indent_all_carets_to_same_level(Buffer *buffer, View *view)
{
    if (!buffer->tab_width) buffer->tab_width = TAB_WIDTH_DEFAULT;

    stack_enter_frame();

    struct LineInfo {
        s32 line_start_offset;
        s32 indent;
        bool at_start_of_line;
    };
    LineInfo *infos = stack_alloc(LineInfo, view->selections.length);

    s32 max_indent = 0;
    s32 min_indent = S32_MAX;

    for (s32 i = 0; i < view->selections.length; ++i) {
        Selection selection = view->selections[i];
        s32 offset = selection.carets[selection.focused_end].offset;
        selection = {};
        selection.start.offset = offset;
        selection.end.offset = offset;
        view->selections[i] = selection;

        bool at_start_of_line = true;
        s32 indent = 0;
        s32 n = offset;
        s32 c;
        while (_buffer_step(buffer, -1, &n, &c, false) && !is_newline(c)) {
            ++indent;
            offset = n;
            if (!is_whitespace(c)) at_start_of_line = false;
        }

        max_indent = max(max_indent, indent);
        min_indent = min(min_indent, indent);

        LineInfo info = {};
        info.line_start_offset = offset;
        info.indent = indent;
        info.at_start_of_line = at_start_of_line;
        infos[i] = info;
    }

    s32 space_count = max(0, max_indent - min_indent);
    char *spaces = stack_alloc(char, space_count);
    for (s32 i = 0; i < space_count; ++i) spaces[i] = ' ';

    for (s32 i = 0; i < view->selections.length; ++i) {
        s32 offset = view->selections[i].start.offset;
        LineInfo info = infos[i];
        if (i > 0 && info.line_start_offset == infos[i - 1].line_start_offset) continue;

        s32 extra_indent = max_indent - info.indent;
        if (extra_indent > 0) {
            str string = { spaces, extra_indent };
            _buffer_insert(buffer, offset, string, false);
        }
    }

    stack_leave_frame();

    buffer_normalize(buffer, view);
    buffer_view_set_focus_to_focused_selection(buffer, view);
    buffer_view_show(buffer, view, BUFFER_SHOW_ANYWHERE);
}