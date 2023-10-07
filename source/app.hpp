#include "util.hpp"
#include "graphics.hpp"
#include "gap_buffer.hpp"
#include "parse.hpp"
#include "colors.h"

char *BUILD_INFO =
    "Built " __DATE__ " " __TIME__
    #if !defined(OPTIMIZE)
        " (optimizations disabled)"
    #endif
    #if defined(DEBUG)
        " (debug symbols enabled)"
    #endif
    ;

enum {
    FONT_SIZE_MIN = 6,
    FONT_SIZE_MAX = 32,

    SPLIT_BORDER = 4,
    MARGIN = 4,

    LINES_PER_SCROLL = 4,
    BUFFER_END_MARKER = '~',
};

enum struct EditMode {
    MOVE = 0,
    MOVE_TO_NEXT_TYPED,
    MOVE_TO_PREVIOUS_TYPED,
    MOVE_TO_TYPED_LINE,
    EXPAND_SELECTION,
    JUMP_FOCUS,
    INSERT,
};


struct BuildError
{
    s32 script_output_line;
    s32 line;
    str path_string;
    Path path;
    bool path_valid;
    str error_message;
};
struct BuildErrorCachedPath
{
    str raw;
    str path_string;
    Path path;
    BuildErrorCachedPath *next;
};
void update_build_script_errors();


struct PromptSuggestion
{
    s32 sort_index;
    s32 match_offset;
    str display_string;
    str display_string_extra;

    void *user_pointer;
    s32 user_integer;
    bool user_boolean;
};

typedef void (*PromptExecuteFunction)(PromptSuggestion *selected, str typed);
typedef void (*PromptRefreshFunction)(str typed);


enum LastAction
{
    LAST_ACTION_DELETE = -1,
    LAST_ACTION_NONE = 0,
    LAST_ACTION_INSERT = 1,
};

struct App
{
    Font font;
    s32 max_glyphs_per_line;

    bool split_view;
    s32 focused_split;
    struct ViewSplit
    {
        s32 buffer_index;
        s32 view_index;

        s32 prev_buffer_index;
        s32 prev_view_index;

        Rect screen_rect;
    } splits[2];
    s32 show_time;

    Array<Buffer> buffers;
    Buffer command_buffer;
    Rect command_prompt_screen_rect;
    s32 log_buffer_index;
    s32 script_buffer_index;

    Buffer *drag_buffer;
    View *drag_view;
    s32 drag_start_offset;
    s32 drag_previous_offset;
    Rect drag_screen_rect;

    s32 build_error_active;
    Arena build_error_arena;
    BuildErrorCachedPath *build_error_path_cache;
    Array<BuildError> build_errors;

    Arena clipboard_arena;
    Slice<str> clipboard;
    u64 platform_clipboard_hash;

    EditMode repeat_mode;
    union {
        s32 repeat_action;
        s32 repeat_codepoint;
    };

    bool show_special_characters;
    LastAction last_action;
    EditMode edit_mode;

    bool jump_to_line_negative;
    s32 jump_to_line;

    s32 search_direction;
    str last_search_term;
    s32 last_search_term_capacity;

    Path diff_tool_path;

    bool show_build_script_info;
    Path build_script;

    Path browse_directory;

    struct {
        bool showing;

        Arena arena;

        PromptExecuteFunction execute_function;
        PromptRefreshFunction refresh_function;

        str text;
        bool highlight_text;

        bool no_suggestions_given;
        Slice<PromptSuggestion> suggestions;
        s32 suggestion_matching;
        s32 suggestion_active;

        s64 matches_for_command_buffer_content_revision;
    } prompt;
};

global_variable App app;
global_variable Colors colors;


void show_buffer(s32 buffer_index)
{
    assert(buffer_index >= 0 && buffer_index < app.buffers.length);

    App::ViewSplit *focused = &app.splits[app.focused_split];
    App::ViewSplit *unfocused = app.split_view? &app.splits[(app.focused_split + 1) % alen(app.splits)] : null;

    if (buffer_index != focused->buffer_index) {
        if (buffer_index == focused->prev_buffer_index) {
            swap(focused->prev_buffer_index, focused->buffer_index);
            swap(focused->prev_view_index, focused->view_index);
        } else {
            s32 view_index = 0;
            if (unfocused && ((unfocused->buffer_index == buffer_index && unfocused->view_index == 0) || (unfocused->prev_buffer_index == buffer_index && unfocused->prev_view_index == view_index))) {
                view_index = 1;
            }

            focused->prev_buffer_index = focused->buffer_index;
            focused->prev_view_index = focused->view_index;
            focused->buffer_index = buffer_index;
            focused->view_index = view_index;
        }

        Buffer *buffer = &app.buffers[focused->buffer_index];
        buffer->last_show_time[app.focused_split] = ++app.show_time;
        View *view = &buffer->views[focused->view_index];
        buffer_check_for_external_changes(buffer);
        buffer_normalize(buffer, view);
    }

    app.last_action = LAST_ACTION_NONE;
}

Buffer *buffer_for_path(Path path)
{
    Buffer *result = null;
    for (s32 i = 0; i < app.buffers.length && !result; ++i) {
        if (path_compare(path, app.buffers[i].path)) {
            result = &app.buffers[i];
        }
    }
    return(result);
}

bool show_path(Path path)
{
    s32 buffer_index = -1;

    for (s32 i = 0; i < app.buffers.length && buffer_index == -1; ++i) {
        if (path_compare(path, app.buffers[i].path)) {
            buffer_index = i;
        }
    }

    if (buffer_index == -1) {
        Buffer new_buffer = {};

        IoError result = IoError::UNKNOWN_ERROR;
        if (buffer_set_path(&new_buffer, path, app.browse_directory)) {
            buffer_set_layout_parameters(&new_buffer, &app.font, app.max_glyphs_per_line, 0, 0, app.show_special_characters);
            result = buffer_load(&new_buffer);
        }

        if (result != IoError::OK) {
            stack_enter_frame();
            char *path_string = path_to_str(path).data;
            debug_printf("Couldn't open from \"%s\" (%s)\n", path_string, io_error_to_str(result));
            stack_leave_frame();
            buffer_free(&new_buffer);
        } else if (new_buffer.path) {
            buffer_index = (s32) app.buffers.length;
            app.buffers.append(new_buffer);
        }
    }

    if (buffer_index != -1) {
        app.edit_mode = EditMode::MOVE;
        app.prompt.showing = false;
        show_buffer(buffer_index);
    }
    return(buffer_index != -1);
}

str get_status_mark(Buffer *buffer)
{
    bool internal = buffer_has_internal_changes(buffer);
    char *mark;
    switch (buffer->external_status) {
        case Buffer::EXT_DELETED:  mark = internal? " (*, file deleted)" : " (file deleted)"; break;
        case Buffer::EXT_MODIFIED: mark = internal? " (*, external changes)" : " (external changes)"; break;
        default:                   mark = internal? " (*)" : ""; break;
    }
    return(cstring_to_str(mark));
}

void prompt_show_error(str message);


void _command_toggle_split()
{
    app.split_view = !app.split_view;
    if (app.split_view) {
        app.focused_split = 1;
        app.splits[1].buffer_index = -1;
        show_buffer(app.splits[0].buffer_index);
        app.focused_split = 0;
    } else {
        if (app.focused_split == 1) {
            for_each (buffer, app.buffers) buffer->last_show_time[0] = buffer->last_show_time[1];
        }
        memcpy(&app.splits[0], &app.splits[app.focused_split], sizeof(app.splits[0]));
        app.focused_split = 0;
    }
}

void _command_toggle_hiden_symbols()
{
    app.show_special_characters = !app.show_special_characters;
}

void _command_exit()
{
    request_close();
}

void _command_select_all_search_results()
{
    Buffer *buffer = &app.buffers[app.splits[app.focused_split].buffer_index];
    View *view = &buffer->views[app.splits[app.focused_split].view_index];
    buffer_add_selection_at_all_search_results(buffer, view);
}

void _command_change_highlight_mode();
void _command_insert_special_character();
void _command_change_line_endings();
void _command_change_tab_width();
void _command_change_tab_mode();
void _command_save_as();
void _command_change_directory();
void _command_close_buffer();

struct Command
{
    str string;
    void (*function)();
    bool valid_for_read_only_buffers;
};
Command COMMANDS[] = {
    { lit_to_str("Select all search results"), &_command_select_all_search_results, true },
    { lit_to_str("Save as..."), &_command_save_as, true },
    { lit_to_str("Change directory"), &_command_change_directory, true },
    { lit_to_str("Close buffer"), &_command_close_buffer, false },
    { lit_to_str("Toggle split view"), &_command_toggle_split, true },
    { lit_to_str("Insert special character"), &_command_insert_special_character, false },
    { lit_to_str("Toggle hidden characters"), &_command_toggle_hiden_symbols, true },
    { lit_to_str("Change syntax coloring language"), &_command_change_highlight_mode, true },
    { lit_to_str("Change line endings"), &_command_change_line_endings, false },
    { lit_to_str("Change tab width"), &_command_change_tab_width, false },
    { lit_to_str("Change tab mode"), &_command_change_tab_mode, false },
    { lit_to_str("Exit editor"), &_command_exit, true },
};

struct SpecialCharacter
{
    char *display_name;
    s32 codepoint;
};
SpecialCharacter SPECIAL_CHARACTERS[] = {
    { "ß (sharp s)", 223 },
    { "α (alpha)", 945 },
    { "β (beta)", 946 },
    { "γ (gamma)", 947 },
    { "δ (delta)", 948 },
    { "ε (epsilon)", 949 },
    { "ζ (zeta)", 950 },
    { "η (eta)", 951 },
    { "θ (theta)", 952 },
    { "ι (iota)", 953 },
    { "κ (kappa)", 954 },
    { "λ (lambda)", 955 },
    { "μ (mu)", 956 },
    { "ν (nu)", 957 },
    { "ξ (xi)", 958 },
    { "ο (omicron)", 959 },
    { "π (pi)", 960 },
    { "ρ (rho)", 961 },
    { "ς (final sigma)", 962 },
    { "σ (sigma)", 963 },
    { "τ (tau)", 964 },
    { "υ (upsilon)", 965 },
    { "φ (phi)", 966 },
    { "χ (chi)", 967 },
    { "ψ (psi)", 968 },
    { "ω (omega)", 969 },
    { "Α (capital alpha)", 913 },
    { "Β (capital beta)", 914 },
    { "γ (capital gamma)", 915 },
    { "Δ (capital delta)", 916 },
    { "Ε (capital epsilon)", 917 },
    { "Ζ (capital zeta)", 918 },
    { "Η (capital eta)", 919 },
    { "Θ (capital theta)", 920 },
    { "Ι (capital iota)", 921 },
    { "Κ (capital kappa)", 922 },
    { "Λ (capital lambda)", 923 },
    { "Μ (capital mu)", 924 },
    { "Ν (capital nu)", 925 },
    { "Ξ (capital xi)", 926 },
    { "Ο (capital omicron)", 927 },
    { "Π (capital pi)", 928 },
    { "Ρ (capital rho)", 929 },
    { "Σ (capital sigma)", 931 },
    { "Τ (capital tau)", 932 },
    { "Υ (capital upsilon)", 933 },
    { "Φ (capital phi)", 934 },
    { "Χ (capital chi)", 935 },
    { "Ψ (capital psi)", 936 },
    { "Ω (capital omega)", 937 },
};

void set_browse_directory(Path new_directory)
{
    if (app.browse_directory) heap_free(app.browse_directory);
    app.browse_directory = heap_copy(new_directory);

    for_each (buffer, app.buffers) buffer_update_display_string(buffer, app.browse_directory);

    s32 previous_error_index = app.build_error_active;

    arena_reset(&app.build_error_arena);
    app.build_error_path_cache = null;
    update_build_script_errors();

    if (0 <= previous_error_index && previous_error_index < app.build_errors.length) {
        app.build_error_active = previous_error_index;

        BuildError *active_error = &app.build_errors[app.build_error_active];
        for (s32 i = 0; i < app.buffers.length; ++i) {
            if (path_compare(active_error->path, app.buffers[i].path)) {
                active_error->path_valid = true;
                break;
            }
        }
    }
}




void _reset_prompt()
{
    Arena arena = app.prompt.arena;
    memset(&app.prompt, 0, sizeof(app.prompt));
    arena_reset(&arena);
    app.prompt.arena = arena;

    app.edit_mode = EditMode::INSERT;
    buffer_reset(&app.command_buffer);

    app.prompt.showing = true;
}

bool cmp_PromptSuggestion(PromptSuggestion *left, PromptSuggestion *right)
{
    return(left->sort_index < right->sort_index);
}

void update_prompt_suggestions()
{
    if (app.command_buffer.revision != app.prompt.matches_for_command_buffer_content_revision) {
        app.prompt.matches_for_command_buffer_content_revision = app.command_buffer.revision;
        str typed = buffer_move_gap_to_end(&app.command_buffer);

        app.prompt.suggestion_active = 0;
        app.prompt.suggestion_matching = 0;

        stack_enter_frame();
        str typed_lower = utf8_map(typed, &unicode_lowercase);
        str typed_upper = utf8_map(typed, &unicode_uppercase);

        for_each (suggestion, app.prompt.suggestions) {
            if (suggestion->sort_index == 0) {
                suggestion->sort_index = 1 + for_each_index(suggestion, app.prompt.suggestions);
            }

            s64 offset = str_search_ignore_case_internal(suggestion->display_string, typed_lower, typed_upper);
            suggestion->match_offset = (s32) offset;
            if (offset != -1) ++app.prompt.suggestion_matching;
        }
        stack_leave_frame();

        for (s32 i = 0, j = app.prompt.suggestions.length - 1; i < j; ) {
            while (j >= 0 && app.prompt.suggestions[j].match_offset == -1) --j;
            while (i <= j && app.prompt.suggestions[i].match_offset != -1) ++i;
            if (i < j) swap(app.prompt.suggestions[i], app.prompt.suggestions[j]);
        }

        Slice<PromptSuggestion> all_matching = slice(app.prompt.suggestions, 0, app.prompt.suggestion_matching);
        stable_sort(all_matching, &cmp_PromptSuggestion);

        if (app.prompt.refresh_function) (*app.prompt.refresh_function)(typed);
    }
}

void execute_user_prompt(bool ignore_suggestions)
{
    app.prompt.showing = false;
    app.edit_mode = EditMode::MOVE;

    PromptSuggestion *suggestion = null;
    if (!ignore_suggestions && app.prompt.suggestion_matching > 0 && app.prompt.suggestion_active < app.prompt.suggestion_matching) {
        suggestion = &app.prompt.suggestions[app.prompt.suggestion_active];
    }

    str typed = buffer_move_gap_to_end(&app.command_buffer);
    if (app.prompt.execute_function) (*app.prompt.execute_function)(suggestion, typed);
}


void prompt_show_error(str message)
{
    _reset_prompt();
    app.prompt.text = arena_copy(&app.prompt.arena, message);
    app.prompt.highlight_text = true;
    app.prompt.no_suggestions_given = true;
}


void prompt_browse();


void _prompt_confirm_create_file_execute(PromptSuggestion *selected, str typed);
void prompt_confirm_create_file(Path path)
{
    _reset_prompt();

    stack_enter_frame();

    str path_str = path_to_str(path);
    str prompt_text = stack_printf("Create \"%.*s\"?", str_fmt(path_str));

    app.prompt.text = arena_copy(&app.prompt.arena, prompt_text);
    app.prompt.highlight_text = true;

    PromptSuggestion file_option = {}, dir_option = {}, no_option = {};
    file_option.display_string = lit_to_str("Create file");
    dir_option.display_string = lit_to_str("Create directory");
    no_option.display_string = lit_to_str("No");

    file_option.user_pointer = arena_copy(&app.prompt.arena, path);
    dir_option.user_pointer = file_option.user_pointer;
    dir_option.user_boolean = true;

    app.prompt.suggestions = arena_make_slice(&app.prompt.arena, PromptSuggestion, 3);
    app.prompt.suggestions[0] = no_option;
    app.prompt.suggestions[1] = file_option;
    app.prompt.suggestions[2] = dir_option;

    app.prompt.execute_function = &_prompt_confirm_create_file_execute;

    stack_leave_frame();
}
void _prompt_confirm_create_file_execute(PromptSuggestion *selected, str typed)
{
    if (selected && selected->user_pointer) {
        Path path = (Path) selected->user_pointer;

        IoError error = IoError::OK;
        if (selected->user_boolean) {
            error = create_directory(path);
        } else {
            error = create_file(path);
        }

        if (error != IoError::OK) {
            stack_enter_frame();
            str message = stack_printf("Couldn't create file (%s)", io_error_to_str(error));
            prompt_show_error(message);
            stack_leave_frame();
        } else {
            if (selected->user_boolean) {
                // We created a directory.
            } else {
                // We created a file.
                show_path(path);
            }
        }
    }
}

IoError show_save_as_dialog(Buffer *buffer)
{
    stack_enter_frame();
    Path new_path = show_save_file_dialog(app.browse_directory);
    IoError error = IoError::OK;
    if (new_path) error = buffer_save(buffer, new_path);
    stack_leave_frame();
    return(error);
}

void _command_save_as()
{
    Buffer *buffer = &app.buffers[app.splits[app.focused_split].buffer_index];
    IoError error = show_save_as_dialog(buffer);
    if (error != IoError::OK) {
        stack_enter_frame();
        str message = stack_printf("Couldn't save file (%s)", io_error_to_str(error));
        prompt_show_error(message);
        stack_leave_frame();
    }
}

void _command_close_buffer()
{
    stack_enter_frame();
    _reset_prompt();
    Buffer *buffer = &app.buffers[app.splits[app.focused_split].buffer_index];

    str prompt_text = stack_printf("Close \"%.*s\"? Unsaved changes will be lost!", str_fmt(path_to_str(buffer->path)));
    app.prompt.text = arena_copy(&app.prompt.arena, prompt_text);
    app.prompt.highlight_text = true;

    PromptSuggestion options[2] = {};
    options[0].display_string = lit_to_str("Do nothing");
    options[1].display_string = lit_to_str("Close buffer");
    options[1].user_boolean = true;
    app.prompt.suggestions = arena_copy(&app.prompt.arena, array_as_slice(options));

    app.prompt.execute_function = [](PromptSuggestion *selected, str typed) {
        if (selected && selected->user_boolean) {
            s32 buffer_index = app.splits[app.focused_split].buffer_index;
            buffer_free(&app.buffers[buffer_index]);
            s32 swapped_index = app.buffers.length - 1;

            assert(buffer_index != app.log_buffer_index && buffer_index != app.script_buffer_index &&
                   swapped_index != app.log_buffer_index && swapped_index != app.script_buffer_index);

            s32 initial_focused_split = app.focused_split;
            for (s32 i = 0; i < alen(app.splits); ++i) {
                if (app.splits[i].buffer_index == buffer_index) {
                    app.focused_split = i;
                    show_buffer(app.log_buffer_index);
                    app.splits[i].prev_buffer_index = app.splits[i].buffer_index;
                    app.splits[i].prev_view_index = app.splits[i].view_index;
                }
            }
            app.focused_split = initial_focused_split;

            if (swapped_index != buffer_index) {
                app.buffers[buffer_index] = app.buffers[swapped_index];
                for (s32 i = 0; i < alen(app.splits); ++i) {
                    if (app.splits[i].buffer_index == swapped_index) {
                        app.splits[i].buffer_index = buffer_index;
                    }
                    if (app.splits[i].prev_buffer_index == swapped_index) {
                        app.splits[i].prev_buffer_index = buffer_index;
                    }
                }
            }

            app.buffers.length -= 1;
        }
    };

    stack_leave_frame();
}


void prompt_overwrite_or_reload()
{
    stack_enter_frame();
    _reset_prompt();

    enum { WAIT, READ, WRITE, WRITE_TO_NEW_PATH };

    {
        Buffer *buffer = &app.buffers[app.splits[app.focused_split].buffer_index];
        str path_string = path_to_str(buffer->path);

        char *message = 0;
        switch (buffer->external_status) {
            case Buffer::EXT_MODIFIED: message = "been modified externally"; break;
            case Buffer::EXT_DELETED:  message = "been deleted"; break;
            case Buffer::EXT_SAME:     message = "*not* been modified externally"; break;
            default: assert(false);
        }

        str prompt_text = stack_printf("\"%.*s\" has %s", str_fmt(path_string), message);
        app.prompt.text = arena_copy(&app.prompt.arena, prompt_text);
        app.prompt.highlight_text = true;

        if (buffer->external_status == Buffer::EXT_DELETED) {
            PromptSuggestion options[3] = {};
            options[0].display_string = lit_to_str("Do nothing");
            options[0].user_integer = WAIT;
            options[1].display_string = lit_to_str("Rewrite file to disk");
            options[1].user_integer = WRITE;
            options[2].display_string = lit_to_str("Write file to a new path");
            options[2].user_integer = WRITE_TO_NEW_PATH;
            app.prompt.suggestions = arena_copy(&app.prompt.arena, array_as_slice(options));
        } else {
            PromptSuggestion options[4] = {};
            options[0].display_string = lit_to_str("Do nothing");
            options[0].user_integer = WAIT;
            options[1].display_string = lit_to_str("Read from disk");
            options[1].user_integer = READ;
            options[2].display_string = lit_to_str("Overwrite file on disk");
            options[2].user_integer = WRITE;
            options[3].display_string = lit_to_str("Write file to a new path");
            options[3].user_integer = WRITE_TO_NEW_PATH;
            app.prompt.suggestions = arena_copy(&app.prompt.arena, array_as_slice(options));
        }
    }

    app.prompt.execute_function = [](PromptSuggestion *selected, str typed) {
        Buffer *buffer = &app.buffers[app.splits[app.focused_split].buffer_index];
        IoError error = IoError::OK;

        if (selected && selected->user_integer == READ)  error = buffer_load(buffer);
        if (selected && selected->user_integer == WRITE) error = buffer_save(buffer);
        if (selected && selected->user_integer == WRITE_TO_NEW_PATH) error = show_save_as_dialog(buffer);

        if (error != IoError::OK) {
            stack_enter_frame();
            char *action = selected->user_integer == READ? (char *)"reload" : (char *)"save";
            str message = stack_printf("Couldn't %s file (%s)", action, io_error_to_str(error));
            prompt_show_error(message);
            stack_leave_frame();
        }
    };

    stack_leave_frame();
}


void prompt_open_buffers()
{
    _reset_prompt();

    app.prompt.text = lit_to_str("Currently open buffers");

    app.prompt.suggestions = arena_make_slice(&app.prompt.arena, PromptSuggestion, app.buffers.length);
    for (s32 i = 0; i < app.buffers.length; ++i) {
        Buffer *buffer = &app.buffers[i];
        bool focused = app.splits[app.focused_split].buffer_index == i;

        PromptSuggestion suggestion = {};
        suggestion.display_string = arena_copy(&app.prompt.arena, buffer->path_display_string);
        suggestion.display_string_extra = get_status_mark(buffer);
        suggestion.user_integer = i;
        suggestion.sort_index = focused? 1 : -app.buffers[i].last_show_time[app.focused_split];
        app.prompt.suggestions[i] = suggestion;
    }

    app.prompt.execute_function = [](PromptSuggestion *selected, str typed) {
        if (selected) {
            show_buffer(selected->user_integer);
        }
    };
}


void _command_insert_special_character()
{
    _reset_prompt();
    app.prompt.text = lit_to_str("Insert a special character");


    app.prompt.suggestions = arena_make_slice(&app.prompt.arena, PromptSuggestion, array_length(SPECIAL_CHARACTERS));
    for (s32 i = 0; i < array_length(SPECIAL_CHARACTERS); ++i) {
        PromptSuggestion suggestion = {};
        suggestion.display_string = cstring_to_str(SPECIAL_CHARACTERS[i].display_name);
        suggestion.user_integer = SPECIAL_CHARACTERS[i].codepoint;
        app.prompt.suggestions[i] = suggestion;
    }

    app.prompt.execute_function = [](PromptSuggestion *selected, str typed) {
        if (selected) {
            s32 codepoint = selected->user_integer;
            Buffer *buffer = &app.buffers[app.splits[app.focused_split].buffer_index];
            View *view = &buffer->views[app.splits[app.focused_split].view_index];

            u8 text_buffer[4];
            u8 length = encode_utf8(codepoint, text_buffer);
            str string = { (char *) text_buffer, length };
            buffer_insert_at_all_carets(buffer, view, string);
        }
    };
}

void _command_change_highlight_mode()
{
    _reset_prompt();
    app.prompt.text = lit_to_str("Choose a syntax coloring language");

    app.prompt.suggestions = arena_make_slice(&app.prompt.arena, PromptSuggestion, array_length(HIGHLIGHT_FUNCTIONS));
    for (s32 i = 0; i < array_length(HIGHLIGHT_FUNCTIONS); ++i) {
        PromptSuggestion suggestion = {};
        suggestion.display_string = cstring_to_str(HIGHLIGHT_FUNCTIONS[i].name);
        suggestion.user_integer = i;
        app.prompt.suggestions[i] = suggestion;
    }

    app.prompt.execute_function = [](PromptSuggestion *selected, str typed) {
        if (selected) {
            Buffer *buffer = &app.buffers[app.splits[app.focused_split].buffer_index];
            buffer_override_highlight_mode(buffer, selected->user_integer);
        }
    };
}

void _command_change_line_endings()
{
    _reset_prompt();
    app.prompt.text = lit_to_str("Choose line ending style");
    app.show_special_characters = true;

    app.prompt.suggestions = arena_make_slice(&app.prompt.arena, PromptSuggestion, (s32) NewlineMode::_COUNT);
    for (s32 i = 0; i < (s32) NewlineMode::_COUNT; ++i) {
        PromptSuggestion suggestion = {};
        suggestion.display_string = NEWLINE_ESCAPED[i];
        suggestion.user_integer = i;
        app.prompt.suggestions[i] = suggestion;
    }

    app.prompt.execute_function = [](PromptSuggestion *selected, str typed) {
        if (selected) {
            NewlineMode mode = (NewlineMode) selected->user_integer;
            Buffer *buffer = &app.buffers[app.splits[app.focused_split].buffer_index];
            buffer_change_line_endings(buffer, mode);
            history_insert_sentinel(buffer);
        }
    };
}

void _command_change_tab_width()
{
    stack_enter_frame();
    _reset_prompt();

    Buffer *focused_buffer = &app.buffers[app.splits[app.focused_split].buffer_index];
    str prompt = stack_printf("Choose tab width (Currently %i)", focused_buffer->tab_width? focused_buffer->tab_width : TAB_WIDTH_DEFAULT);
    app.prompt.text = arena_copy(&app.prompt.arena, prompt);

    app.prompt.no_suggestions_given = true;

    app.prompt.execute_function = [](PromptSuggestion *selected, str typed) {
        str t = typed;
        trim_spaces(&t);
        s32 new_width;
        bool ok = eat_s32(&t, &new_width) && t.length == 0 && TAB_WIDTH_MIN <= new_width && new_width <= TAB_WIDTH_MAX;
        if (ok) {
            Buffer *focused_buffer = &app.buffers[app.splits[app.focused_split].buffer_index];
            buffer_set_tab_width(focused_buffer, new_width);
        } else {
            stack_enter_frame();
            prompt_show_error(stack_printf("\"%.*s\" isn't a number between %i and %i", str_fmt(typed), TAB_WIDTH_MIN, TAB_WIDTH_MAX));
            stack_leave_frame();
        }
    };

    stack_leave_frame();
}

void _command_change_tab_mode()
{
    _reset_prompt();

    Buffer *focused_buffer = &app.buffers[app.splits[app.focused_split].buffer_index];
    str prompt = lit_to_str("Choose tab mode");
    app.prompt.text = arena_copy(&app.prompt.arena, prompt);

    PromptSuggestion soft = {}, hard = {};
    soft.display_string = lit_to_str("Soft");
    soft.user_integer = (s32) TabMode::SOFT;
    hard.display_string = lit_to_str("Hard");
    hard.user_integer = (s32) TabMode::HARD;

    app.prompt.no_suggestions_given = false;
    app.prompt.suggestions = arena_make_slice(&app.prompt.arena, PromptSuggestion, 2);
    app.prompt.suggestions[0] = soft;
    app.prompt.suggestions[1] = hard;

    app.prompt.execute_function = [](PromptSuggestion *selected, str typed) {
        if (selected) {
            Buffer *buffer = &app.buffers[app.splits[app.focused_split].buffer_index];
            buffer->tab_mode = (TabMode) selected->user_integer;
        }
    };
}

void _command_change_directory()
{
    _reset_prompt();

    app.prompt.no_suggestions_given = true;

    stack_enter_frame();
    str current_directory_string = path_to_str(app.browse_directory);
    str prompt = stack_printf("Current directory: %.*s", str_fmt(current_directory_string));
    app.prompt.text = arena_copy(&app.prompt.arena, prompt);
    stack_leave_frame();

    app.prompt.execute_function = [](PromptSuggestion *selected, str typed) {
        if (typed.length > 0) {
            stack_enter_frame();
            Path absolute_typed = path_make_absolute(str_to_path(typed), app.browse_directory);
            if (path_is_directory(absolute_typed)) {
                set_browse_directory(absolute_typed);
            }
            stack_leave_frame();
        }
    };
}

void prompt_command()
{
    _reset_prompt();
    app.prompt.text = lit_to_str("Execute a command");

    bool in_read_only_buffer = app.buffers[app.splits[app.focused_split].buffer_index].no_user_input;

    app.prompt.suggestions = arena_make_slice(&app.prompt.arena, PromptSuggestion, array_length(COMMANDS));
    app.prompt.suggestions.length = 0;
    for (s32 i = 0; i < array_length(COMMANDS); ++i) {
        if (COMMANDS[i].valid_for_read_only_buffers || !in_read_only_buffer) {
            PromptSuggestion suggestion = {};
            suggestion.display_string = COMMANDS[i].string;
            suggestion.user_integer = i;

            app.prompt.suggestions.data[app.prompt.suggestions.length++] = suggestion;
        }
    }

    app.prompt.execute_function = [](PromptSuggestion *selected, str typed) {
        if (selected) {
            (*COMMANDS[selected->user_integer].function)();
        }
    };
}


void prompt_search()
{
    _reset_prompt();
    if (app.search_direction == 1) {
        app.prompt.text = lit_to_str("Search forward in current buffer");
    } else {
        app.prompt.text = lit_to_str("Search backwards in current buffer");
    }
    app.prompt.no_suggestions_given = true;

    app.prompt.refresh_function = [](str typed) {
        if (typed.length > 0) {
            if (typed.length > app.last_search_term_capacity) {
                s32 NewCapacity = max(max(app.last_search_term_capacity*2, 256), next_power_of_two(typed.length));
                app.last_search_term.data = (char *) heap_grow(app.last_search_term.data, NewCapacity);
                app.last_search_term_capacity = NewCapacity;
            }
            memcpy(app.last_search_term.data, typed.data, typed.length);
            app.last_search_term.length = typed.length;
        }

        s32 buffer_index = app.splits[app.focused_split].buffer_index;
        Buffer *buffer = &app.buffers[buffer_index];
        View *view = &buffer->views[app.splits[app.focused_split].view_index];
        buffer_search(buffer, view, typed);
        buffer_next_search_result(buffer, view, app.search_direction, true);
    };
}

void prompt_change_search_mode()
{
    _reset_prompt();
    app.prompt.text = lit_to_str("Change match mode for current search");

    PromptSuggestion suggestions[3] = {};
    suggestions[0].display_string = lit_to_str("Normal");
    suggestions[0].user_integer = SEARCH_RESULT_CASE_MATCH;
    suggestions[1].display_string = lit_to_str("Ignore case");
    suggestions[1].user_integer = 0;
    suggestions[2].display_string = lit_to_str("Match only identifiers");
    suggestions[2].user_integer = SEARCH_RESULT_CASE_MATCH | SEARCH_RESULT_IDENTIFIER_MATCH;

    app.prompt.suggestions = arena_make_slice(&app.prompt.arena, PromptSuggestion, alen(suggestions));
    memcpy(app.prompt.suggestions.data, suggestions, sizeof(suggestions));

    app.prompt.execute_function = [](PromptSuggestion *selected, str typed) {
        if (selected) {
            Buffer *buffer = &app.buffers[app.splits[app.focused_split].buffer_index];
            View *view = &buffer->views[app.splits[app.focused_split].view_index];
            buffer_search_filter(buffer, view, (u32) selected->user_integer);
        }
    };
}


static bool
_path_string_is_directory(str path)
{
    for (s64 i = 0; i < path.length; i += 1) {
        if (path.data[i] == '/' || path.data[i] == '\\') {
            return(true);
        }
    }
    return(false);
}

static
str _path_string_get_extension(str path)
{
    str extension = {};
    for (s64 i = path.length; i > 0; i -= 1) {
        if (path.data[i - 1] == '.') {
            extension = { &path.data[i], path.length - i };
            break;
        } else if (path.data[i - 1] == '/' || path.data[i - 1] == '\\') {
            break;
        }
    }
    return(extension);
}

bool cmp_DirItem(DirItem **left, DirItem **right)
{
    str left_string = (**left).display_string;
    str right_string = (**right).display_string;

    // Sort code files first.
    // Using "do we have highlighting" captures what I care about:
    // I mostly edit files for which I have syntax highlighting, so I'm likely to want to see these first in the file listing.
    str left_extension = _path_string_get_extension(left_string);
    str right_extension = _path_string_get_extension(right_string);
    bool left_is_code = highlight_get_function_index(left_extension) != 0;
    bool right_is_code = highlight_get_function_index(right_extension) != 0;
    if (left_is_code != right_is_code) return(left_is_code && !right_is_code);

    //// Sort files starting with a dot last.
    bool left_dot = left_string.length > 0 && left_string[0] == '.';
    bool right_dot = right_string.length > 0 && right_string[0] == '.';
    if (left_dot != right_dot) return(!left_dot && right_dot);

    // Sort files at the top level before other files.
    bool left_is_root = !_path_string_is_directory(left_string);
    bool right_is_root = !_path_string_is_directory(right_string);
    if (left_is_root != right_is_root) return(left_is_root && !right_is_root);

    // Otherwise, sort files alphabetically.
    return(in_lexicographic_order(&left_string, &right_string));
}

void _prompt_browse_execute(PromptSuggestion *selected, str typed);
void prompt_browse()
{
    stack_enter_frame();

    _reset_prompt();
    app.prompt.execute_function = &_prompt_browse_execute;

    DirItemList items = path_list_directory(app.browse_directory);

    str prompt_text = path_to_str(app.browse_directory);
    app.prompt.text = arena_copy(&app.prompt.arena, prompt_text);

    Slice<DirItem *> sorted_items = {};
    sorted_items.data = stack_alloc(DirItem *, items.count);
    for (DirItem *item = items.first; item; item = item->next) sorted_items.data[sorted_items.length++] = item;
    assert(sorted_items.length == items.count);
    stable_sort(sorted_items, cmp_DirItem);

    app.prompt.suggestions = arena_make_slice(&app.prompt.arena, PromptSuggestion, items.count);
    s32 i = 0;

    for_each (item_ptr, sorted_items) {
        DirItem *item = *item_ptr;

        PromptSuggestion suggestion = {};
        suggestion.user_pointer = arena_copy(&app.prompt.arena, item->full_path);
        suggestion.display_string = arena_copy(&app.prompt.arena, item->display_string);

        Buffer *matching_buffer = buffer_for_path(item->full_path);
        if (matching_buffer) suggestion.display_string_extra = get_status_mark(matching_buffer);

        app.prompt.suggestions[i++] = suggestion;
    }

    assert(i == app.prompt.suggestions.length);
    stack_leave_frame();
}
void _prompt_browse_execute(PromptSuggestion *selected, str typed)
{
    stack_enter_frame();

    Path absolute_typed = null;
    if (typed.length > 0) {
        absolute_typed = path_make_absolute(str_to_path(typed), app.browse_directory);
    }

    if (typed.length > 0 && !absolute_typed) {
        str browse_directory_string = path_to_str(app.browse_directory);
        str user_message = stack_printf("\"%.*s\" doesn't seem to be a valid path", str_fmt(typed));
        debug_printf("Can't open \"%.*s\" relative to \"%.*s\"\n", str_fmt(typed), str_fmt(browse_directory_string));
        prompt_show_error(user_message);
    } else if (typed.length > 0 && path_is_normal_file(absolute_typed)) {
        show_path(absolute_typed);
    } else if (selected) {
        Path selected_path = (Path) selected->user_pointer;
        show_path(selected_path);
    } else if (typed.length > 0) {
        prompt_confirm_create_file(absolute_typed);
    }
    stack_leave_frame();
}



void app_debug_print_hook(str string)
{
    if (app.log_buffer_index < app.buffers.length) {
        Buffer *log_buffer = &app.buffers[app.log_buffer_index];
        buffer_insert_at_end(log_buffer, string);
    }
}

void app_init()
{
    stack_enter_frame();

    memcpy(&colors, &COLORS_DARK, sizeof(Colors));

    app.log_buffer_index = (s32) app.buffers.length;
    Buffer *log = app.buffers.push();
    log->no_user_input = true;
    log->path_display_string = lit_to_str("<log>");
    log->path_display_string_static = true;
    for (s32 i = 0; i < alen(log->views); ++i) {
        log->last_show_time[i] = ++app.show_time;
        buffer_normalize(log, &log->views[i]);
    }

    app.browse_directory = heap_copy(get_working_directory());
    set_working_directory(str_to_path(L"C:\\"));

    debug_printf("%s\n", BUILD_INFO);

    app.script_buffer_index = (s32) app.buffers.length;
    Buffer *script_buffer = app.buffers.push();
    script_buffer->no_user_input = true;
    script_buffer->path_display_string = lit_to_str("<script output>");
    script_buffer->path_display_string_static = true;
    for (s32 i = 0; i < 2; ++i) script_buffer->last_show_time[i] = ++app.show_time;

    s32 scratch_buffer_index = (s32) app.buffers.length;
    Buffer *scratch = app.buffers.push();
    scratch->path_display_string = lit_to_str("<scratch>");
    scratch->path_display_string_static = true;

    Path scratch_path = path_make_absolute(str_to_path("am7.txt"), get_appdata_path());
    assert(scratch_path);

    buffer_set_path(scratch, scratch_path, app.browse_directory);
    IoError scratch_load_error = buffer_load(scratch);
    if (scratch_load_error != IoError::OK) {
        debug_printf("Couldn't load scratch file (%s), trying to create it\n", io_error_to_str(scratch_load_error));
        IoError scratch_create_error = create_file(scratch_path);
        if (!(scratch_create_error == IoError::OK && scratch_create_error != IoError::ALREADY_EXISTS)) {
            debug_printf("Couldn't create scratch file (%s)\n", io_error_to_str(scratch_create_error));
        }
        scratch->path = heap_copy(scratch_path);
    }

    app.splits[app.focused_split].buffer_index = -1;
    show_buffer(scratch_buffer_index);

    buffer_normalize(&app.command_buffer, &app.command_buffer.views[0]);

    font_change_size(&app.font, 10);

    Slice<CmdArg> command_line_arguments = slice(get_command_line_arguments(), 1);
    for_each (arg, command_line_arguments) {
        debug_printf("Got command line argument: \"%.*s\"\n", str_fmt(arg->narrow));
        show_path(str_to_path(arg->wide));
    }

    #if 0
    Path new_script = str_to_path("C:\\Dev\\projects\\am7\\misc\\test script.bat");
    app.build_script = heap_copy(new_script);
    app.show_build_script_info = true;
    buffer_reset(&app.buffers[app.script_buffer_index]);
    start_script(app.build_script);
    show_buffer(app.script_buffer_index);
    #endif

    stack_leave_frame();
}

bool on_close_requested(bool force_prompt)
{
    Buffer *unsaved_changes_in = null;
    s64 unsaved_changes_in_count = 0;
    for (s64 i = 0; i < app.buffers.length; ++i) {
        Buffer *buffer = &app.buffers[i];
        if (buffer_has_internal_changes(buffer)) {
            if (!unsaved_changes_in) unsaved_changes_in = buffer;
            ++unsaved_changes_in_count;
        }
    }

    bool may_close = true;

    if (unsaved_changes_in) {
        stack_enter_frame();
        str message;
        if (unsaved_changes_in_count <= 1) {
            message = stack_printf("\"%.*s\" has unsaved changes.\nClosing now will discard these changes.",
                                   str_fmt(unsaved_changes_in->path_display_string));
        } else {
            message = stack_printf("\"%.*s\" and %lli more file%s have unsaved changes.\nChosing 'Ok' will discard these changes.",
                                   str_fmt(unsaved_changes_in->path_display_string),
                                   unsaved_changes_in_count - 1, unsaved_changes_in_count > 2? "s" : "");
        }
        may_close = prompt_with_dialog(lit_to_str("Unsaved changes"), message);
        stack_leave_frame();
    } else if (force_prompt) {
        may_close = prompt_with_dialog(lit_to_str("Are you sure"), lit_to_str("Are you sure you want to close the editor?"));
    }

    return(may_close);
}

void paste(Buffer *buffer, View *view, s32 where)
{
    stack_enter_frame();
    str clipboard_data;
    char *error = get_from_clipboard(&clipboard_data);
    if (!error) {
        history_insert_sentinel(buffer); // Mark on every copy/paste. Previous line just keeps track of the mode
        buffer_empty_all_selections(buffer, view);

        u64 hash = hash_good_64(clipboard_data);
        if (hash == app.platform_clipboard_hash && view->selections.length > 1 && app.clipboard.length > 1) {
            // NB (Morten, 2020-05-11) We assume that if the hashes are equal, the clipboard has not been modified

            s32 n = min(app.clipboard.length, view->selections.length);
            for (s32 i = 0; i < n; ++i) {
                str single = app.clipboard[i];
                buffer_insert_at_caret(buffer, view, i, single);
            }
            buffer_view_set_focus_to_focused_selection(buffer, view);
            buffer_view_show(buffer, view, BUFFER_SHOW_ANYWHERE);

        } else if (clipboard_data.length > 0) {
            if (where == 0) {
                buffer_insert_at_all_carets(buffer, view, clipboard_data);
            } else {
                buffer_insert_on_new_line_at_all_carets(buffer, view, where, clipboard_data);
            }
        }

        history_insert_sentinel(buffer);
    } else {
        debug_printf("Error while pasting (%s)\n", error);
    }
    stack_leave_frame();
}

void on_typed(s32 codepoint, bool control, bool shift)
{
    s32 buffer_index;
    Buffer *buffer;
    View *view;
    if (!app.prompt.showing) {
        buffer_index = app.splits[app.focused_split].buffer_index;
        buffer = &app.buffers[buffer_index];
        view = &buffer->views[app.splits[app.focused_split].view_index];
    } else {
        buffer_index = -1;
        buffer = &app.command_buffer;
        view = &buffer->views[0];
    }
    bool can_edit = !buffer->no_user_input;

    if ((control && codepoint == 'c') || codepoint == CHAR_ESCAPE)
    {
        if (app.edit_mode == EditMode::INSERT) {
            buffer_remove_trailing_spaces(buffer, view);
            history_insert_sentinel(buffer);
        }
        app.edit_mode = EditMode::MOVE;
        app.prompt.showing = false;
    }

    else if (codepoint == CHAR_F9)
    {
        bool dark = memcmp(&COLORS_DARK, &colors, sizeof(Colors)) == 0;
        memcpy(&colors, dark? &COLORS_BRIGHT : &COLORS_DARK, sizeof(Colors));
    }

    else if (app.prompt.showing && (codepoint == CHAR_UP || (codepoint == 'k' && control))) app.prompt.suggestion_active = min(app.prompt.suggestion_active + 1, app.prompt.suggestion_matching - 1);
    else if (app.prompt.showing && (codepoint == CHAR_DOWN || (codepoint == 'j' && control))) app.prompt.suggestion_active = max(app.prompt.suggestion_active - 1, 0);

    else if (control && codepoint == ' ')
    {
        if (app.split_view) {
            app.focused_split = (app.focused_split + 1) % array_length(app.splits);
            app.edit_mode = EditMode::MOVE;
            app.last_action = LAST_ACTION_NONE;
            app.prompt.showing = false;
        }
    }

    else if (codepoint == CHAR_LEFT)  buffer_step_all_carets_codepoints(buffer, view, -1, false, shift);
    else if (codepoint == CHAR_RIGHT) buffer_step_all_carets_codepoints(buffer, view, 1, false, shift);
    else if (codepoint == CHAR_UP)    buffer_step_all_carets_lines(buffer, view, -1, shift, control);
    else if (codepoint == CHAR_DOWN)  buffer_step_all_carets_lines(buffer, view, 1,  shift, control);
    else if (codepoint == CHAR_HOME)  buffer_step_all_carets_codepoints(buffer, view, S32_MIN, true, false);
    else if (codepoint == CHAR_END)   buffer_step_all_carets_codepoints(buffer, view, S32_MAX, true, false);

    else if (can_edit && codepoint == '\b')        buffer_empty_all_selections(buffer, view, EMPTY_ALL_SELECTIONS_OR_DELETE_ONE_LEFT);
    else if (can_edit && codepoint == CHAR_DELETE) buffer_empty_all_selections(buffer, view, EMPTY_ALL_SELECTIONS_OR_DELETE_ONE_RIGHT);

    else if (codepoint == CHAR_F11) toggle_fullscreen();

    else if (app.edit_mode == EditMode::MOVE_TO_NEXT_TYPED || app.edit_mode == EditMode::MOVE_TO_PREVIOUS_TYPED)
    {
        if (codepoint >= 0) {
            s32 direction = app.edit_mode == EditMode::MOVE_TO_NEXT_TYPED? 1 : -1;
            buffer_step_all_carets_to_boundary(buffer, view, direction, codepoint);
            app.repeat_mode = app.edit_mode;
            app.repeat_codepoint = codepoint;
        }
        app.edit_mode = EditMode::MOVE;
    }

    else if (app.edit_mode == EditMode::EXPAND_SELECTION)
    {
        s32 action = -1;
        bool repeatable = false;
        switch (codepoint) {
            case '(': case ')': action = BUFFER_EXPAND_PARENTHESIS; repeatable = true; break;
            case '[': case ']': action = BUFFER_EXPAND_BRACKET; repeatable = true; break;
            case '{': case '}': action = BUFFER_EXPAND_BRACE; repeatable = true; break;
            case '<': case '>': action = BUFFER_EXPAND_TAG; repeatable = true; break;
            case 'b':           action = BUFFER_EXPAND_BLOCK; repeatable = true; break;

            case '\"': action = BUFFER_EXPAND_DOUBLE_QUOTE; break;
            case '\'': action = BUFFER_EXPAND_SINGLE_QUOTE; break;
            case 'l':  action = BUFFER_EXPAND_LINE_WITHOUT_LEADING_SPACES; break;
            case 'L':  action = BUFFER_EXPAND_LINE; break;
            case 'a':  action = BUFFER_EXPAND_ALL; break;
            case 'w':  action = BUFFER_EXPAND_WORD; break;
        }

        if (action != -1) {
            if (repeatable && view->selections.length == 1) {
                app.repeat_mode = EditMode::EXPAND_SELECTION;
                app.repeat_action = action;
            } else {
                app.repeat_mode = EditMode::MOVE;
            }

            buffer_expand_all_carets(buffer, view, action);
        }
        app.edit_mode = EditMode::MOVE;
    }

    else if (app.edit_mode == EditMode::MOVE_TO_TYPED_LINE)
    {
        if (codepoint >= '0' && codepoint <= '9') {
            s32 New = app.jump_to_line*10 + (codepoint - '0');
            if (New > app.jump_to_line) app.jump_to_line = New;
        } else if (codepoint == '-') {
            app.jump_to_line_negative = true;
        } else {
            if (is_newline(codepoint)) {
                s32 line = app.jump_to_line;
                if (app.jump_to_line_negative) line = buffer->physical_line_count - line;
                buffer_jump_to_physical_line(buffer, view, line, true);
            }
            app.edit_mode = EditMode::MOVE;
        }
    }

    else if (app.edit_mode == EditMode::JUMP_FOCUS)
    {
        int action;
        switch (codepoint) {
            case 'j': action = BUFFER_SHOW_BOTTOM; break;
            case 'k': action = BUFFER_SHOW_TOP; break;
            default:  action = BUFFER_SHOW_CENTER; break;
        }
        buffer_view_set_focus_to_focused(buffer, view);
        buffer_view_show(buffer, view, action);
        app.edit_mode = EditMode::MOVE;
    }

    else if (app.edit_mode == EditMode::MOVE) {
        if (codepoint == 'c' && !app.prompt.showing) prompt_command();

        else if ((codepoint == 's' || codepoint == 'S') && !app.prompt.showing)
        {
            app.search_direction = codepoint == 'S'? -1 : 1;
            prompt_search();
            if (control) {
                buffer_insert_at_all_carets(&app.command_buffer, &app.command_buffer.views[0], app.last_search_term);
            }
        }
        else if (codepoint == '*')
        {
            Selection focused_selection = view->selections[view->focused_selection];

            bool do_identifier_match = focused_selection.start.offset == focused_selection.end.offset;
            if (do_identifier_match) buffer_expand(buffer, view, &focused_selection, BUFFER_EXPAND_WORD);

            if (focused_selection.start.offset != focused_selection.end.offset) {
                stack_enter_frame();
                str outtake = buffer_get_slice(buffer, focused_selection.start.offset, focused_selection.end.offset);
                outtake = stack_copy(outtake);

                app.search_direction = 1;
                buffer_search(buffer, view, outtake);
                if (do_identifier_match) {
                    buffer_search_filter(buffer, view, SEARCH_RESULT_CASE_MATCH | SEARCH_RESULT_IDENTIFIER_MATCH);
                }

                s32 base_offset = focused_selection.carets[focused_selection.focused_end].offset;
                for (s32 i = 0; i < view->search.active; ++i) {
                    if (view->search.list[i].min <= base_offset && view->search.list[i].max >= base_offset) {
                        view->search.focused = i;
                        break;
                    }
                }

                buffer_view_set_focus_to_focused(buffer, view);
                buffer_view_show(buffer, view, BUFFER_SHOW_ANYWHERE);
                stack_leave_frame();
            }
        }
        else if (codepoint == CHAR_F3)
        {
            prompt_change_search_mode();
        }

        else if (can_edit && control && codepoint == 'w' && !app.prompt.showing && buffer->path)
        {
            buffer_check_for_external_changes(buffer);
            if (buffer->external_status != Buffer::EXT_SAME) {
                prompt_overwrite_or_reload();
            } else {
                buffer_save(buffer);
            }
        }

        else if (codepoint == 'o' && !app.prompt.showing) prompt_browse();
        else if (codepoint == 'O' && !app.prompt.showing) prompt_open_buffers();
        else if (codepoint == '\t' && app.splits[app.focused_split].prev_buffer_index != -1) show_buffer(app.splits[app.focused_split].prev_buffer_index);

        else if (codepoint == ' ') buffer_next_selection(buffer, view, shift? -1 : 1);
        else if (codepoint == 'q') buffer_remove_all_search_results(buffer, view);
        else if (codepoint == 'Q') buffer_remove_focused_selection(buffer, view);
        else if (codepoint == 'm') buffer_collapse_selections(buffer, view);
        else if (codepoint == 'M') buffer_remove_nonfocused_selection(buffer, view);
        else if (codepoint == 'n') buffer_change_selection_focused_end(buffer, view);
        else if (codepoint == '3') buffer_add_caret_on_each_selected_line(buffer, view);
        else if (codepoint == '#') buffer_indent_all_carets_to_same_level(buffer, view);
        else if (codepoint == 't' || codepoint == 'T')
        {
            s32 direction = codepoint == 't'? 1 : -1;
            buffer_add_selection_at_active_search_result(buffer, view, direction);
            history_insert_sentinel(buffer);
        }
        else if (codepoint == 'b' || codepoint == 'B')
        {
            buffer_expand_all_carets_to_next_search_result(buffer, view, codepoint == 'b'? 1 : -1);
            history_insert_sentinel(buffer);
        }
        else if (codepoint == 'r' || codepoint == 'R')
        {
            buffer_next_search_result(buffer, view, codepoint == 'r'? 1 : -1, true);
        }

        else if (codepoint == 'z') app.edit_mode = EditMode::JUMP_FOCUS;
        else if (codepoint == '[' || codepoint == ']')
        {
            s32 delta = codepoint == '['? -10 : 10;
            buffer_view_scroll(buffer, view, delta);
        }

        else if (codepoint == 'H' || codepoint == 'h' || codepoint == 'L' || codepoint == 'l')
        {
            history_insert_sentinel(buffer);
            s32 direction = (codepoint == 'h' || codepoint == 'H')? -1 : 1;
            bool split = codepoint == 'H' || codepoint == 'L';
            buffer_step_all_carets_codepoints(buffer, view, direction, true, split);
        }
        else if (codepoint == 'K' || codepoint == 'k' || codepoint == 'J' || codepoint == 'j')
        {
            history_insert_sentinel(buffer);
            s32 direction = (codepoint == 'K' || codepoint == 'k')? -1 : 1;
            bool grow = codepoint == 'K' || codepoint == 'J';
            buffer_step_all_carets_lines(buffer, view, direction, grow, control);
        }
        else if (codepoint == 'W' || codepoint == 'w' || codepoint == 'E' || codepoint == 'e')
        {
            history_insert_sentinel(buffer);
            s32 direction = (codepoint == 'w' || codepoint == 'e')? 1 : -1;
            bool to_start = codepoint == 'w' || codepoint == 'W';
            buffer_step_all_carets_words(buffer, view, direction, to_start);
        }
        else if (codepoint == 'y' || codepoint == 'Y')
        {
            history_insert_sentinel(buffer);
            s32 direction = (codepoint == 'y')? 1 : -1;
            buffer_step_all_carets_both_ways(buffer, view, direction, false);
        }
        else if (codepoint == '{' || codepoint == '}')
        {
            history_insert_sentinel(buffer);
            buffer_step_all_carets_empty_lines(buffer, view, codepoint == '{'? -1 : 1);
        }

        else if (codepoint == '_' || codepoint == '.')
        {
            history_insert_sentinel(buffer);
            buffer_step_all_carets_codepoints(buffer, view, codepoint == '_'? S32_MIN : S32_MAX, true, true);
        }
        else if (codepoint == '-')
        {
            history_insert_sentinel(buffer);
            buffer_step_all_carets_codepoints(buffer, view, S32_MIN, true, true);
            buffer_step_all_carets_past_whitespace(buffer, view, 1);
        }

        else if (codepoint == 'g')
        {
            app.edit_mode = EditMode::MOVE_TO_TYPED_LINE;
            app.jump_to_line_negative = false;
            app.jump_to_line = 0;
        }
        else if (codepoint == 'f' || codepoint == 'F')
        {
            app.edit_mode = codepoint == 'f'? EditMode::MOVE_TO_NEXT_TYPED : EditMode::MOVE_TO_PREVIOUS_TYPED;
            buffer_view_set_focus_to_focused_selection(buffer, view);
            buffer_view_show(buffer, view, BUFFER_SHOW_ANYWHERE);
        }
        else if (codepoint == 'v')
        {
            app.edit_mode = EditMode::EXPAND_SELECTION;
            buffer_view_set_focus_to_focused_selection(buffer, view);
            buffer_view_show(buffer, view, BUFFER_SHOW_ANYWHERE);
        }
        else if (codepoint == ',' || codepoint == ';')
        {
            switch (app.repeat_mode) {
                case EditMode::MOVE_TO_NEXT_TYPED:
                case EditMode::MOVE_TO_PREVIOUS_TYPED:
                {
                    s32 direction = app.repeat_mode == EditMode::MOVE_TO_NEXT_TYPED? 1 : -1;
                    if (codepoint == ';') direction *= -1;
                    buffer_step_all_carets_to_boundary(buffer, view, direction, app.repeat_codepoint);
                } break;

                case EditMode::EXPAND_SELECTION: {
                    buffer_expand_all_carets(buffer, view, app.repeat_action);
                } break;

                default: break;
            }
        }

        else if (can_edit && (codepoint == 'x' || codepoint == 'X'))
        {
            if (app.last_action != LAST_ACTION_DELETE) {
                history_insert_sentinel(buffer);
                app.last_action = LAST_ACTION_DELETE;
            }
            buffer_empty_all_selections(buffer, view, codepoint == 'x'? EMPTY_ALL_SELECTIONS_OR_DELETE_ONE_RIGHT : EMPTY_ALL_SELECTIONS_OR_DELETE_ONE_LEFT);
        }

        else if (codepoint == 'd' || (can_edit && codepoint == 'D'))
        {
            history_insert_sentinel(buffer);
            app.last_action = LAST_ACTION_DELETE;

            arena_reset(&app.clipboard_arena);
            app.clipboard = arena_make_slice(&app.clipboard_arena, str, view->selections.length);

            stack_enter_frame(); // stack frame because 'buffer_get_slice' allocates if selection stradles gap
            // It probably would be cleaner to have buffer_get_slice copy into the arena for us :^)

            for (s32 i = 0; i < view->selections.length; ++i) {
                Selection focused_selection = view->selections[i];
                str outtake = buffer_get_slice(buffer, focused_selection.start.offset, focused_selection.end.offset);
                app.clipboard[i] = arena_copy(&app.clipboard_arena, outtake);
            }

            stack_leave_frame();
            stack_enter_frame(); // stack frame for concatenating to build 'full'

            str full;
            if (app.clipboard.length == 1) {
                full = app.clipboard[0];
            } else {
                full = {};
                for_each (part, app.clipboard) {
                    stack_printf_append(&full, "%.*s\n", str_fmt(*part));
                }
            }

            if (full.length > 0) {
                char *error = put_in_clipboard(full);
                if (!error) {
                    if (codepoint == 'D') buffer_empty_all_selections(buffer, view);
                    if (codepoint == 'd') buffer_collapse_selections(buffer, view);
                    app.platform_clipboard_hash = hash_good_64(full);
                } else if (error) {
                    debug_printf("Error while copying (%s)\n", error);
                }
            }

            stack_leave_frame();
        }

        else if (can_edit && (codepoint == 'u' || codepoint == 'U'))
        {
            app.last_action = LAST_ACTION_NONE;
            history_scroll(buffer, view, codepoint == 'u'? -1 : 1);
        }

        else if (can_edit && codepoint == 'i')
        {
            app.last_action = LAST_ACTION_NONE; // Note (Morten, 2020-08-24) Insert mode manages history sentinels separately
            history_insert_sentinel(buffer);
            buffer_empty_all_selections(buffer, view);
            app.edit_mode = EditMode::INSERT;
        }

        else if (can_edit && is_newline(codepoint))
        {
            app.last_action = LAST_ACTION_NONE; // Note (Morten, 2020-08-24) Insert mode manages history sentinels separately
            history_insert_sentinel(buffer);
            buffer_insert_on_new_line_at_all_carets(buffer, view, shift? -1 : 1, lit_to_str(""));
            buffer_collapse_selections(buffer, view);
            app.edit_mode = EditMode::INSERT;
        }

        else if (can_edit && (codepoint == '<' || codepoint == '>'))
        {
            if (app.last_action != LAST_ACTION_INSERT) {
                history_insert_sentinel(buffer);
                app.last_action = LAST_ACTION_INSERT;
            }
            buffer_indent_selected_lines(buffer, view, codepoint == '>');
        }

        else if (can_edit && (codepoint == 'p' || codepoint == 'P'))
        {
            app.last_action = LAST_ACTION_NONE;
            paste(buffer, view, codepoint == 'p'? 1 : -1);
            buffer_view_show(buffer, view, BUFFER_SHOW_ANYWHERE);
        }

        else if (codepoint == CHAR_F5)
        {
            if (shift) {
                stop_script();
            } else if (script_last_exit_code() != SCRIPT_STILL_RUNNING) {
                bool run = app.build_script && !control;
                if (!run) {
                    stack_enter_frame();
                    Path new_script = show_open_file_dialog(app.browse_directory, false);
                    if (new_script) {
                        if (app.build_script) heap_free(app.build_script);
                        app.build_script = heap_copy(new_script);
                        run = true;
                    }
                    stack_leave_frame();
                }

                if (run) {
                    buffer_check_for_external_changes(buffer);
                    if (buffer->path && buffer->external_status == Buffer::EXT_SAME) buffer_save(buffer);

                    app.show_build_script_info = true;
                    buffer_reset(&app.buffers[app.script_buffer_index]);
                    update_build_script_errors();
                    start_script(app.build_script);
                }
            }
        }
        else if (codepoint == CHAR_F6) app.show_build_script_info = false;
        else if (codepoint == 230 || codepoint == 198) // 'æ' and 'Æ'
        {
            app.show_build_script_info = true;
            if (app.build_errors.length <= 0) {
                app.build_error_active = -1;
            } else {
                if (app.build_error_active == -1) {
                    app.build_error_active = 0;
                } else {
                    app.build_error_active += codepoint == 230? 1 : -1;
                    s32 l = app.build_errors.length;
                    if (app.build_error_active <  0) app.build_error_active = 0;
                    if (app.build_error_active >= l) app.build_error_active = l - 1;
                }

                BuildError *error = &app.build_errors[app.build_error_active];
                error->path_valid = error->path && show_path(error->path);
                if (error->path_valid) {
                    Buffer *buffer = &app.buffers[app.splits[app.focused_split].buffer_index];
                    View *view = &buffer->views[app.splits[app.focused_split].view_index];
                    buffer_jump_to_physical_line(buffer, view, error->line, false);
                }

                for (s32 i = 0; i < (app.split_view? 2 : 1); ++i) {
                    if (app.splits[i].buffer_index == app.script_buffer_index) {
                        Buffer *buffer = &app.buffers[app.splits[i].buffer_index];
                        View *view = &buffer->views[app.splits[i].view_index];
                        buffer_jump_to_physical_line(buffer, view, error->script_output_line, false);
                    }
                }
            }
        }

        #if 1 && !defined(OPTIMIZE)
        else if (codepoint == CHAR_F1)
        {
            _reset_prompt();
            app.prompt.suggestions = arena_make_slice(&app.prompt.arena, PromptSuggestion, alen(COLOR_NAMES));
            for (s32 i = 0; i < alen(COLOR_NAMES); ++i) {
                PromptSuggestion suggestion = {};
                suggestion.display_string = COLOR_NAMES[i];
                suggestion.user_integer = i;
                app.prompt.suggestions[i] = suggestion;
            }
            app.prompt.text = lit_to_str("Choose color");
            app.prompt.execute_function = [](PromptSuggestion *selected, str typed) {
                if (selected) {
                    u32 *slot = ((u32 *) &colors) + selected->user_integer;
                    u32 color = *slot;
                    u32 alpha = color >> 24;
        
                    color = ((color&0xff0000)>>16) | (color&0x00ff00) | ((color&0x0000ff)<<16);
                    u32 idk[16] = {};
                    win32::choosecolorw cc = {};
                    cc.struct_size = sizeof(cc);
                    cc.result = color;
                    cc.cust_colors = idk;
                    cc.flags = win32::CC_RGBINIT | win32::CC_FULLOPEN;
                    cc.owner = win32::GetActiveWindow();
                    win32::ChooseColorW(&cc);
                    color = cc.result;
                    color = ((color&0xff0000)>>16) | (color&0x00ff00) | ((color&0x0000ff)<<16) | (alpha << 24);
                    *slot = color;
                    request_redraw();

                    debug_printf("\n");
                    for (s32 i = 0; i < sizeof(colors)/4; ++i) {
                        u32 value = *(((u32 *)&colors) + i);
                        debug_printf("%06x\n", value);
                    }
                }
            };
        }
        #endif

    } else if (app.edit_mode == EditMode::INSERT && codepoint >= 0) {
        assert(can_edit);

        str insert_line;
        if (is_newline(codepoint)) {
            if (app.prompt.showing) {
                execute_user_prompt(shift);
            } else {
                // NB (Morten, 2020-05-27) The function internally converts \n to the selected newline combination
                buffer_empty_all_selections(buffer, view);
                buffer_insert_at_all_carets(buffer, view, lit_to_str("\n"));
            }
        } else if (codepoint == '\t') {
            buffer_empty_all_selections(buffer, view);
            buffer_insert_tabs_at_all_carets(buffer, view);
        } else if (control && codepoint == 'v') {
            buffer_empty_all_selections(buffer, view);
            paste(buffer, view, 0);
        } else {
            char InsertBuffer[4];
            insert_line.data = InsertBuffer;
            insert_line.length = encode_utf8(codepoint, (u8 *) insert_line.data);
            assert(codepoint <= UNICODE_LARGEST_CODEPOINT);
            assert(insert_line.length > 0);

            buffer_empty_all_selections(buffer, view);
            buffer_insert_at_all_carets(buffer, view, insert_line);
        }
    }

    if (app.prompt.showing) update_prompt_suggestions();
}

void on_script_output(str text)
{
    if (app.script_buffer_index < app.buffers.length) {
        Buffer *script_buffer = &app.buffers[app.script_buffer_index];
        buffer_insert_at_end(script_buffer, text);
        update_build_script_errors();
    }
}

s32 _split_index_for_cursor_position(s32v2 mouse_position)
{
    s32 split_index = app.focused_split;
    s32 min_distance = S32_MAX;
    for (s32 i = 0; i < alen(app.splits); ++i) {
        s32 distance = rect_external_distance(app.splits[i].screen_rect, mouse_position);
        if (distance < min_distance) {
            min_distance = distance;
            split_index = i;
        }
    }
    return(split_index);
}

void on_file_dropped(s32 x, s32 y, Path path)
{
    s32 split_index = _split_index_for_cursor_position({ x, y });
    app.focused_split = split_index;
    show_path(path);
}

void on_scroll(s32 x, s32 y, s32 delta, bool control, bool shift)
{
    if (control) {
        s32 new_font_size = min(max(FONT_SIZE_MIN, app.font.metrics.size + delta), FONT_SIZE_MAX);
        if (new_font_size != app.font.metrics.size) {
            font_change_size(&app.font, new_font_size);
        }
    } else {
        s32 split_index = _split_index_for_cursor_position({ x, y });
        Buffer *buffer = &app.buffers[app.splits[split_index].buffer_index];
        View *view = &buffer->views[app.splits[split_index].view_index];
        buffer_view_scroll(buffer, view, -delta*4);
    }
}

bool on_click(s32 x, s32 y, bool drag, bool control, bool shift)
{
    Buffer *buffer = null;
    View *view = null;
    Rect screen_rect;

    if (drag) {
        buffer = app.drag_buffer;
        view = app.drag_view;
        screen_rect = app.drag_screen_rect;
    } else {
        for (s32 i = 0; i < (app.split_view? 2 : 1); ++i) {
            if (rect_contains(app.splits[i].screen_rect, { x, y })) {
                screen_rect = app.splits[i].screen_rect;
                buffer = &app.buffers[app.splits[i].buffer_index];
                view = &buffer->views[app.splits[i].view_index];
                app.focused_split = i;
                break;
            }
        }
    
        if (app.prompt.showing) {
            if (!buffer) {
                if (rect_contains(app.command_prompt_screen_rect, { x, y })) {
                    screen_rect = app.command_prompt_screen_rect;
                    buffer = &app.command_buffer;
                    view = &app.command_buffer.views[0];
                }
            } else {
                app.prompt.showing = false;
            }
        }

        app.drag_previous_offset = -1;
    }

    bool changed = false;
    if (buffer) {
        s32 offset = buffer_layout_offset_to_offset(buffer, view, { x - screen_rect.x0, y - screen_rect.y0 });

        Selection new_selection = {};
        new_selection.start.offset = drag? app.drag_start_offset : offset;
        new_selection.end.offset = offset;
        new_selection.focused_end = 1;

        int action = BUFFER_SET_SELECTION_SET;
        if (control) action = drag? BUFFER_SET_SELECTION_REPLACE : BUFFER_SET_SELECTION_ADD;
        buffer_set_selection(buffer, view, new_selection, action);
        buffer_view_set_focus_to_focused_selection(buffer, view); // Note (Morten, 2020-07-26) We used to not call this, and I seem to remember that there was a good reason for it, but I can't remember what it was. If we don't call this to change view->focus_offset though, things get messed up when placing the cursor while at the bottom of <script output> while it continously scrolls.
        app.edit_mode = EditMode::MOVE;

        if (!drag) {
            app.drag_buffer = buffer;
            app.drag_view = view;
            app.drag_start_offset = offset;
            app.drag_screen_rect = screen_rect;
        }

        changed = app.drag_previous_offset != offset;
        app.drag_previous_offset = offset;
    }

    return(changed);
}

bool on_three_second_timer()
{
    bool any_changes = false;
    for_each (buffer, app.buffers) {
        any_changes |= buffer_check_for_external_changes(buffer);
    }
    return(any_changes);
}



bool draw_buffer(DrawTargetSlice canvas, Buffer *buffer, View *view, bool focused)
{
    s64 visible_lines = (canvas.area.y1 - canvas.area.y0) / app.font.metrics.line_height;
    s32 margin_lines = 4;
    if (visible_lines < 2*margin_lines) margin_lines = 0;

    buffer_set_layout_parameters(buffer, &app.font, app.max_glyphs_per_line, visible_lines, margin_lines, app.show_special_characters);

    bool animating = false;

    s32 focus_delta = view->focus_line_offset_target*FOCUS_LINE_OFFSET_SUBSTEPS - view->focus_line_offset_current;
    if (focus_delta != 0) {
        animating = true;
        // Note (Morten, 2020-08-02) In practice (having used it both on 60Hz and 144Hz monitors) I find this looks fine in spite of it not being framerate independent. Sure, on a higher refresh-rate monitor it will scroll faster, but because of the higher framerate it will still look smooth
        s32 sign = focus_delta > 0? 1 : -1;
        s32 max_step = focus_delta*sign;
        s32 step = max(FOCUS_LINE_OFFSET_SUBSTEPS/4, max_step/4);
        step = min(step, max_step);
        view->focus_line_offset_current += step*sign;
    }

    s32 text_height = app.font.metrics.ascent - app.font.metrics.descent;
    s32 line_height = app.font.metrics.line_height;
    s32 focus_line = buffer_offset_to_virtual_line_index(buffer, view->focus_offset);
    s32 top_line = focus_line + (view->focus_line_offset_current / FOCUS_LINE_OFFSET_SUBSTEPS) - 1;
    s32 top_line_subline_offset = (view->focus_line_offset_current % FOCUS_LINE_OFFSET_SUBSTEPS) * line_height / FOCUS_LINE_OFFSET_SUBSTEPS;
    s32 line_index_start = clamp(top_line, 0, buffer->lines.length - 1);
    s32 y0 = (line_index_start - top_line - 1)*line_height - top_line_subline_offset;
    s32 line_count = clamp(((canvas.area.y1 - canvas.area.y0) - y0 + line_height) / line_height, 1, buffer->lines.length - line_index_start);
    view->layout_offset = { MARGIN, y0 - line_index_start*line_height };
    s32 line_index_end = line_index_start + line_count;

    bool in_edit_mode = focused && app.edit_mode == EditMode::INSERT;
    u32 ColorCaret = in_edit_mode? colors.caret_insert : colors.caret;
    u32 ColorCaretInactive = in_edit_mode? colors.caret_insert : colors.caret_inactive;

    Selection *selection_start = &view->selections[0];
    Selection *selection_focused = &view->selections[view->focused_selection];
    Selection *selection_end = selection_start + view->selections.length;

    SearchResult *search_start = view->search.list;
    SearchResult *search_end = search_start + view->search.active;
    SearchResult *search_focused = null;
    if (view->search.focused >= 0 && view->search.focused < view->search.active) {
        search_focused = &view->search.list[view->search.focused];
    }

    s32 offset_start = buffer->lines[line_index_start].start;
    s32 offset_end   = buffer->lines[line_index_end - 1].end;

    s32 highlight_index = 0;
    while (highlight_index + 16 < buffer->highlights.length && buffer->highlights[highlight_index + 16].end < offset_start) highlight_index += 16;

    // We could use a binary search here, but usually there are few selections, so I can't be bothered implementing one
    while (selection_start < selection_end && selection_start->end.offset < offset_start) ++selection_start;
    while (search_start < search_end && search_start->max < offset_start) ++search_start;


    // Selections
    for (Selection *selection = selection_start; selection < selection_end && selection->start.offset <= offset_end; ++selection) {
        s32v2 area_start = buffer_offset_to_layout_offset(buffer, view, selection->start.offset);
        s32v2 area_end   = buffer_offset_to_layout_offset(buffer, view, selection->end.offset);

        u32 fill_color = (selection == selection_focused)? colors.selection : colors.selection_unfocused;

        // This is the sort of code they warn you about at school. And I sort of get it :/ (Morten, 2020-05-12)
        if (area_start.y == area_end.y) {
            if (area_start.x != area_end.x) {
                draw_solid(&canvas, area_start.x, area_start.y + 1, area_end.x, area_start.y + text_height - 1, fill_color);
                draw_solid(&canvas, area_start.x + 1, area_start.y, area_end.x - 1, area_start.y + 1, colors.selection_border);
                draw_solid(&canvas, area_start.x + 1, area_start.y + text_height - 1, area_end.x - 1, area_start.y + text_height, colors.selection_border);
                draw_solid(&canvas, area_start.x - 1, area_start.y + 2, area_start.x, area_start.y + text_height - 2, colors.selection_border);
                draw_solid(&canvas, area_end.x, area_start.y + 2, area_end.x + 1, area_start.y + text_height - 2, colors.selection_border);
                draw_single_pixel(&canvas, area_start.x, area_start.y + 1, colors.selection_border);
                draw_single_pixel(&canvas, area_end.x - 1, area_start.y + 1, colors.selection_border);
                draw_single_pixel(&canvas, area_end.x - 1, area_start.y + text_height - 2, colors.selection_border);
                draw_single_pixel(&canvas, area_start.x, area_start.y + text_height - 2, colors.selection_border);
            }
        } else {
            bool parts_joined = true;

            s32 canvas_width = canvas.area.x1 - canvas.area.x0;

            draw_solid(&canvas, area_start.x, area_start.y + 1, canvas_width, area_start.y + text_height, fill_color);
            draw_solid(&canvas, 0, area_end.y, area_end.x, area_end.y + text_height - 1, fill_color);

            if (area_end.y - area_start.y > line_height) {
                draw_solid(&canvas, area_start.x - 1, area_start.y + text_height, canvas_width, area_start.y + line_height, fill_color);
                draw_solid(&canvas, 0, area_start.y + line_height, canvas_width, area_end.y + text_height - line_height, fill_color);
                draw_solid(&canvas, 0, area_end.y + text_height - line_height, area_end.x + 1, area_end.y, fill_color);
            } else if (area_end.x > area_start.x) {
                draw_solid(&canvas, area_start.x - 1, area_start.y + text_height, area_end.x + 1, area_start.y + line_height, fill_color);
            } else {
                parts_joined = false;
            }

            draw_solid(&canvas, area_start.x + 1, area_start.y, canvas_width, area_start.y + 1, colors.selection_border);
            draw_single_pixel(&canvas, area_start.x, area_start.y + 1, colors.selection_border);
            draw_solid(&canvas, area_start.x - 1, area_start.y + 2, area_start.x, area_start.y + (parts_joined? line_height : text_height) - 1, colors.selection_border);
            draw_solid(&canvas, 0, area_start.y + line_height - 1, (parts_joined? area_start.x : area_end.x) - 1, area_start.y + line_height, colors.selection_border);
            if (!parts_joined) draw_single_pixel(&canvas, area_start.x, area_start.y + text_height - 1, colors.selection_border);

            draw_solid(&canvas, 0, area_end.y + text_height - 1, area_end.x - 1, area_end.y + text_height, colors.selection_border);
            draw_single_pixel(&canvas, area_end.x - 1, area_end.y + text_height - 2, colors.selection_border);
            draw_solid(&canvas, area_end.x, area_end.y + (parts_joined? text_height - line_height : 0) + 1, area_end.x + 1, area_end.y + text_height - 2, colors.selection_border);
            draw_solid(&canvas, (parts_joined? area_end.x : area_start.x) + 1, area_end.y + text_height - line_height, canvas_width, area_end.y + text_height - line_height + 1, colors.selection_border);
            if (!parts_joined) draw_single_pixel(&canvas, area_end.x - 1, area_end.y, colors.selection_border);
        }
    }


    // Text
    draw_glyphs(&canvas, &app.font, valid_decoded_codepoint(BUFFER_END_MARKER), MARGIN, y0 - line_height, colors.support);
    for (s32 line_index = line_index_start; line_index < line_index_end; ++line_index) {
        VirtualLine *line = &buffer->lines[line_index];
        s32 y = y0 + (line_index - line_index_start)*line_height;
        s32 x0 = MARGIN + line->virtual_indent*app.font.metrics.advance;
        s32 x = x0;

        s32 tab_width = (buffer->tab_width? buffer->tab_width : TAB_WIDTH_DEFAULT) * app.font.metrics.advance;

        if (line->virtual_indent > 0) {
            draw_glyphs(&canvas, &app.font, valid_decoded_codepoint(SOFTWRAP_PRE_CODEPOINT), x - app.font.metrics.advance*(SOFTWRAP_PRE_SPACES + 1), y, colors.support);
        }

        s32 buffer_offset = line->start;
        s32 line_offset = 0;

        stack_enter_frame();
        str line_text = buffer_get_virtual_line_text(buffer, line_index);

        while (line_offset < line_text.length) {
            while (highlight_index < buffer->highlights.length && buffer->highlights[highlight_index].end <= buffer_offset) {
                ++highlight_index;
            }
            u32 color = colors.foreground;
            if (highlight_index < buffer->highlights.length && buffer->highlights[highlight_index].start <= buffer_offset) {
                switch (buffer->highlights[highlight_index].kind) {
                    case HighlightKind::Comment:           color = colors.comment; break;
                    case HighlightKind::Keyword:           color = colors.keyword; break;
                    case HighlightKind::Identifier:        color = colors.foreground; break;
                    case HighlightKind::OtherLiteral:      color = colors.literal; break;
                    case HighlightKind::NumberLiteral:     color = colors.literal; break;
                    case HighlightKind::StringLiteral:     color = colors.string; break;
                    case HighlightKind::CharacterLiteral:  color = colors.literal; break;
                    case HighlightKind::Preprocessor:      color = colors.preprocessor; break;
                    default: break;
                };
            }

            DecodedCodepoint decoded = decode_utf8((u8 *) line_text.data + line_offset, line_text.length - line_offset);
            buffer_offset += decoded.length;
            line_offset += decoded.length;

            if (decoded.codepoint == '\t') {
                draw_glyphs(&canvas, &app.font, { true, 1, ':' }, x, y, colors.support, 0);
                x = x0 + round_up(x - x0 + 1, tab_width);
            } else {
                x += draw_glyphs(&canvas, &app.font, decoded, x, y, color, colors.special);
            }
        }

        stack_leave_frame();

        if (line->flags & VirtualLine::MARK_CONTINUATION) {
            draw_glyphs(&canvas, &app.font, valid_decoded_codepoint(SOFTWRAP_POST_CODEPOINT), x, y, colors.support);
        }
    }
    draw_glyphs(&canvas, &app.font, valid_decoded_codepoint(BUFFER_END_MARKER), MARGIN, y0 + line_height*(line_index_end - line_index_start), colors.support);


    // Search highlights
    for (SearchResult *range = search_start; range < search_end && range->min <= offset_end; ++range) {
        if (range->flags & SEARCH_RESULT_HIDE) continue;

        s32v2 area_start = buffer_offset_to_layout_offset(buffer, view, range->min);
        s32v2 area_end   = buffer_offset_to_layout_offset(buffer, view, range->max);
        u32 factor = range == search_focused? colors.search_match_focused : colors.search_match;

        if (area_start.y == area_end.y) {
            if (area_start.x != area_end.x) {
                draw_blend(&canvas, area_start.x, area_start.y, area_end.x, area_start.y + text_height, factor);
            }
        } else {
            draw_blend(&canvas, area_start.x, area_start.y, canvas.area.x1 - canvas.area.x0, area_start.y + text_height, factor);
            bool mid_full = area_end.y - area_start.y > line_height;
            draw_blend(&canvas, mid_full? 0 : area_start.x, area_start.y + text_height, canvas.area.x1 - canvas.area.x0, area_end.y, factor);
            draw_blend(&canvas, 0, area_end.y, area_end.x, area_end.y + text_height, factor);
        }
    }

    // Carets
    for (Selection *selection = selection_start; selection < selection_end && selection->start.offset <= offset_end; ++selection) {
        s32 offset = selection->focused_end == 0? selection->start.offset : selection->end.offset;
        s32v2 p = buffer_offset_to_layout_offset(buffer, view, offset);
        draw_solid(&canvas, p.x - 1, p.y + 1, p.x + 1, p.y + text_height - 1, selection == selection_focused? ColorCaret : ColorCaretInactive);
    }

    return(animating);
}

bool draw_buffer_and_status(DrawTargetSlice canvas, s32 Split)
{
    stack_enter_frame();

    bool animating = false;

    s32 status_height = app.font.metrics.line_height + 2*MARGIN;
    s32 canvas_width = canvas.area.x1 - canvas.area.x0;
    s32 canvas_height = canvas.area.y1 - canvas.area.y0;
    draw_solid(&canvas, 0, canvas_height - status_height, canvas_width, canvas_height, colors.border);
    draw_solid(&canvas, 0, 0, canvas_width, canvas_height - status_height, colors.background);

    DrawTargetSlice text_canvas = draw_target_slice(&canvas, 0, 0, canvas_width, canvas_height - status_height);
    DrawTargetSlice status_canvas = draw_target_slice(&canvas, MARGIN, canvas_height - (app.font.metrics.line_height + 2*MARGIN), canvas_width - MARGIN, canvas_height);

    s32 buffer_index = app.splits[Split].buffer_index;
    Buffer *buffer = &app.buffers[buffer_index];
    View *view = &buffer->views[app.splits[Split].view_index];
    bool focused = Split == app.focused_split && !app.prompt.showing;
    app.splits[Split].screen_rect = text_canvas.area;

    animating |= draw_buffer(text_canvas, buffer, view, focused);

    if (Split == app.focused_split && app.prompt.showing) {
        DrawTargetSlice command_text_canvas = draw_target_slice(&canvas, 0, canvas_height - (app.font.metrics.line_height + MARGIN), canvas_width, canvas_height - MARGIN);
        app.command_prompt_screen_rect = command_text_canvas.area;

        if (buffer_length(&app.command_buffer) > 0) {
            draw_buffer(command_text_canvas, &app.command_buffer, &app.command_buffer.views[0], true);
        } else {
            u32 color = app.prompt.highlight_text? colors.status_prompt : colors.status_inactive;
            draw_text_ellipsis(&status_canvas, &app.font, app.prompt.text, 0, status_canvas.area.x1 - status_canvas.area.x0, MARGIN, color, false);
        }

        s32 y = text_canvas.area.y1 - text_canvas.area.y0;

        s32 i = 0;
        while (i < app.prompt.suggestion_matching && y > 0) {
            u32 color_text = colors.foreground;
            u32 color_background = colors.border;
            u32 color_extra = colors.status_inactive;
            if (i == app.prompt.suggestion_active) {
                swap(color_text, color_background);
                color_extra = color_text;
            }

            draw_solid(&text_canvas, 0, y - app.font.metrics.line_height - MARGIN, text_canvas.area.x1 - text_canvas.area.x0, y, color_background);
            s32 yl = y - app.font.metrics.line_height - MARGIN/2;

            str display = app.prompt.suggestions[i].display_string;
            str display_extra = app.prompt.suggestions[i].display_string_extra;

            s32 x = draw_text_ellipsis(&text_canvas, &app.font, display, MARGIN, text_canvas.area.x1 - text_canvas.area.x0 - MARGIN - 2*app.font.metrics.advance, yl, color_text, false);
            draw_text_ellipsis(&text_canvas, &app.font, display_extra, MARGIN + x, text_canvas.area.x1 - text_canvas.area.x0 - MARGIN - 2*app.font.metrics.advance, yl, color_extra, false);

            y -= app.font.metrics.line_height + MARGIN;
            ++i;
        }
        if (i == 0 && !app.prompt.no_suggestions_given) {
            draw_solid(&text_canvas, 0, y - app.font.metrics.line_height - MARGIN, text_canvas.area.x1 - text_canvas.area.x0, y, colors.border);
            draw_text_ellipsis(&text_canvas, &app.font, lit_to_str("(No matches)"), MARGIN, text_canvas.area.x1 - text_canvas.area.x0 - MARGIN, y - app.font.metrics.line_height - MARGIN/2, colors.support, false);
            y -= app.font.metrics.line_height + MARGIN;
        }
        if (!app.prompt.no_suggestions_given) {
            draw_solid(&text_canvas, 0, y - MARGIN, text_canvas.area.x1 - text_canvas.area.x0, y, colors.border);
        }
    } else {
        char *edit_mode_string = null;
        if (focused) {
            switch (app.edit_mode) {
                case EditMode::MOVE:                   edit_mode_string = "move"; break;
                case EditMode::MOVE_TO_NEXT_TYPED:
                case EditMode::MOVE_TO_PREVIOUS_TYPED: edit_mode_string = "jump to character"; break;
                case EditMode::INSERT:                 edit_mode_string = "insert"; break;
                case EditMode::JUMP_FOCUS:             edit_mode_string = "jump focus"; break;
                case EditMode::EXPAND_SELECTION:       edit_mode_string = "expand selection"; break;
                case EditMode::MOVE_TO_TYPED_LINE:
                {
                    edit_mode_string = stack_printf("jump to %s%i", app.jump_to_line_negative? "-" : "", app.jump_to_line).data;
                } break;
                default: assert(false);
            }
        }

        s32 focus_offset;
        if (0 <= view->search.focused && view->search.focused < view->search.active) {
            focus_offset = view->search.list[view->search.focused].min;
        } else {
            Selection selection = view->selections[view->focused_selection];
            focus_offset = selection.carets[selection.focused_end].offset;
        }
        s32 focus_line = buffer_offset_to_virtual_line_index(buffer, focus_offset);
        s32 physical_line = buffer->lines[focus_line].physical_line_index;
        s32 total_physical_lines = buffer->physical_line_count;
        int total_physical_lines_digits = log10(total_physical_lines);

        str status_message = {};
        if (view->selections.length > 1) {
            stack_printf_append(&status_message, "caret %i/%i - ", view->focused_selection + 1, (s32) view->selections.length);
        }
        if (view->search.active > 0) {
            if (view->search.focused >= 0) {
                stack_printf_append(&status_message, "match %i/%i - ", view->search.focused + 1, view->search.active);
            } else {
                stack_printf_append(&status_message, "%i match%s - ", (s32) view->search.active, view->search.active == 1? "" : "es");
            }
        }
        if (edit_mode_string) {
            stack_printf_append(&status_message, "%s - ", edit_mode_string);
        }
        stack_printf_append(&status_message, "%*i/%i", total_physical_lines_digits, physical_line, total_physical_lines);
        u32 color_status = focused? colors.status_active : colors.status_inactive;
        s32 status_canvas_width = status_canvas.area.x1 - status_canvas.area.x0;
        s32 status_width = draw_text_ellipsis(&status_canvas, &app.font, status_message, status_canvas_width/2, status_canvas_width, MARGIN, color_status, true);

        s32 path_width = status_canvas_width - status_width - 3*app.font.metrics.advance;
        str display = buffer->path_display_string;
        str display_extra = get_status_mark(buffer);
        s32 x = draw_text_ellipsis(&status_canvas, &app.font, display, 0, path_width, MARGIN, color_status, false);
        draw_text_ellipsis(&status_canvas, &app.font, display_extra, x, path_width, MARGIN, colors.status_inactive, false);
    }

    stack_leave_frame();
    return(animating);
}

bool redraw(DrawTargetSlice canvas)
{
    s32 top = 0;
    s32 width = canvas.area.x1 - canvas.area.x0;
    s32 height = canvas.area.y1 - canvas.area.y0;

    if (app.show_build_script_info) {
        stack_enter_frame();

        s32 script_info_height = app.font.metrics.line_height + 2*MARGIN;
        top += script_info_height;
        DrawTargetSlice script_canvas = draw_target_slice(&canvas, 0, 0, width, script_info_height);

        s64 exit_code = script_last_exit_code();

        str message;
        if (exit_code == SCRIPT_HASNT_RUN) {
            message = lit_to_str("Script has not been started");
        } else if (exit_code == SCRIPT_STILL_RUNNING) {
            char *DisplayString = path_to_str(app.build_script).data;
            message = stack_printf("Running %s...", DisplayString);
        } else if (app.build_error_active >= 0) {
            BuildError error = app.build_errors[app.build_error_active];
            message = stack_printf("(%i/%i) %s%.*s(%i): %.*s", app.build_error_active + 1, app.build_errors.length, error.path_valid? "" : "?? ", str_fmt(error.path_string), error.line, str_fmt(error.error_message));
        } else {
            message = stack_printf("Exited with %xh, %lli errors", (u32) exit_code, app.build_errors.length);
        }

        u32 color_background = colors.script_running;
        if (exit_code == 0) color_background = colors.script_good;
        if (exit_code > 0) color_background = colors.script_bad;
        draw_solid(&script_canvas, 0, 0, width, script_info_height, color_background);
        draw_text_ellipsis(&script_canvas, &app.font, message, 2*MARGIN, width - 2*MARGIN, MARGIN, colors.background, false);

        stack_leave_frame();
    }

    bool animating = false;
    if (app.split_view) {
        s32 mid = width/2 - SPLIT_BORDER/2;
        DrawTargetSlice canvas_left = draw_target_slice(&canvas, 0, top, mid, height);
        DrawTargetSlice canvas_right = draw_target_slice(&canvas, mid + SPLIT_BORDER, top, width, height);
        app.max_glyphs_per_line = (mid - 2*MARGIN) / app.font.metrics.advance;
        animating |= draw_buffer_and_status(canvas_left, 0);
        animating |= draw_buffer_and_status(canvas_right, 1);
        draw_solid(&canvas, mid, top, mid + SPLIT_BORDER, height, colors.border);
    } else {
        DrawTargetSlice canvas_full = draw_target_slice(&canvas, 0, top, width, height);
        app.max_glyphs_per_line = (width - 2*MARGIN) / app.font.metrics.advance;
        animating |= draw_buffer_and_status(canvas_full, 0);
    }

    return(animating);
}


BuildErrorCachedPath *build_error_get_cached_path(str raw)
{
    BuildErrorCachedPath *result = null;

    for (BuildErrorCachedPath *p = app.build_error_path_cache; p && !result; p = p->next) {
        if (p->raw == raw) result = p;
    }

    if (!result) {
        stack_enter_frame();

        Path relative = str_to_path(raw);
        Path build_script_directory = path_parent(app.build_script);
        Path full = path_make_absolute(relative, build_script_directory);

        if (full) {
            str relative_string = path_to_str_relative(full, app.browse_directory);

            result = arena_alloc(&app.build_error_arena, BuildErrorCachedPath, 1);
            result->raw = arena_copy(&app.build_error_arena, raw);
            result->path_string = arena_copy(&app.build_error_arena, relative_string);
            result->path = arena_copy(&app.build_error_arena, full);

            result->next = app.build_error_path_cache;
            app.build_error_path_cache = result;
        }

        stack_leave_frame();
    }

    return(result);
}

void update_build_script_errors()
{
    app.build_errors.clear();
    app.build_error_active = -1;

    Buffer *script_buffer = &app.buffers[app.script_buffer_index];
    str script_output = buffer_move_gap_to_end(script_buffer);

    s32 line_index = 0;
    str line = {};
    while (eat_line(&script_output, &line)) {
        ++line_index;
        trim_spaces(&line);
        if (line.length == 0) continue;

        str error_path = {};
        str error_message = {};
        s32 error_line_index = 0;

        s32 i = 0;
        while (i + 1 < line.length && !((line[i] == ':' && is_digit(line[i + 1])) || line[i] == '(')) ++i;
        error_path = slice(line, 0, i);
        line = slice(line, i);

        trim_spaces(&error_path);
        bool ok = error_path.length > 0 && line.length > 0;

        if (ok) {
            if (line[0] == ':') {
                line = slice(line, 1);
                ok &= eat_s32(&line, &error_line_index);
                eat_spaces(&line);
                ok &= eat_specific(&line, ":");
            } else if (line[0] == '(') {
                line = slice(line, 1);
                ok &= eat_s32(&line, &error_line_index);
                eat_spaces(&line);
                ok &= eat_up_to(&line, null, ')');
                ok &= eat_specific(&line, ")");
                eat_spaces(&line);
                eat_specific(&line, ":");
            } else {
                ok = false;
            }
            error_message = line;
        }

        trim_spaces(&error_path);
        trim_spaces(&error_message);

        if (ok) {
            BuildError error = {};
            error.script_output_line = line_index;
            error.line = error_line_index;
            error.path_string = error_path;
            error.error_message = error_message;

            BuildErrorCachedPath *cached_path = build_error_get_cached_path(error_path);
            if (cached_path) {
                error.path_string = cached_path->path_string;
                error.path = cached_path->path;
            }

            app.build_errors.append(error);
        }
    }
}