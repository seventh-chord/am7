#include "util.hpp"
#include "io.hpp"
#include "unicode.h"
#include "win32.hpp"
#include "graphics.hpp"
#include "timing.hpp"

char *put_in_clipboard(str data);
char *get_from_clipboard(str *result);
bool prompt_with_dialog(str title, str message);

void start_script(Path script_path);
void stop_script();

enum { SCRIPT_STILL_RUNNING = -1, SCRIPT_HASNT_RUN = -2 };
s64 script_last_exit_code();

void request_redraw();
void request_close();
void toggle_fullscreen();

enum
{
    CHAR_ESCAPE = 27,
    CHAR_LEFT = -1,
    CHAR_RIGHT = -2,
    CHAR_UP = -3,
    CHAR_DOWN = -4,
    CHAR_HOME = -5,
    CHAR_END = -6,
    CHAR_PAGE_UP = -7,
    CHAR_PAGE_DOWN = -8,
    CHAR_INSERT = -9,
    CHAR_DELETE = -10,
    CHAR_F1 = -11,
    CHAR_F2 = -12,
    CHAR_F3 = -13,
    CHAR_F4 = -14,
    CHAR_F5 = -15,
    CHAR_F6 = -16,
    CHAR_F7 = -17,
    CHAR_F8 = -18,
    CHAR_F9 = -19,
    CHAR_F10 = -20,
    CHAR_F11 = -21,
    CHAR_F12 = -22,
};

// callbacks to app
void on_typed(s32 Codepoint, bool Control, bool Shift);
void on_scroll(s32 x, s32 y, s32 Delta, bool Control, bool Shift);
bool on_click(s32 x, s32 y, bool drag, bool control, bool shift);
void on_file_dropped(s32 x, s32 y, Path path);
void on_script_output(str Data);
bool on_three_second_timer();
bool on_close_requested(bool force_prompt);
bool redraw(DrawTargetSlice canvas);

#include "app.hpp"


struct ScriptWaitMessage
{
    enum {
        OUTPUT,
        DONE,
        FAILED,
    } kind;

    union {
        struct {
            u8 *data;
            s32 length;
        } output;
        u32 exit_code;
        u32 error_code;
    };
};

void _script_handle_message(ScriptWaitMessage *message);

s64 my_window_proc(void *window_handle, s32 message, u64 w, s64 l);
s64 my_window_proc_seh(void *window_handle, s32 message, u64 w, s64 l)
{
    u32 my_exception_code;
    s64 result = 0;
    __try {
        result = my_window_proc(window_handle, message, w, l);
    } __except (my_exception_code = win32::_exception_code(), 1) {
        _assert(true, "A critical, unrecoverable error occured during message handling (0x%x, %s)\n", my_exception_code, win32::_exception_message(my_exception_code));
    }
    return(result);
}

struct BackendWin32
{
    void *win32_cursor_normal;
    void *window_handle;
    win32::window_placement window_placement_before_fullscreen;
    bool close_requested;

    DrawTarget backbuffer;
    bool resubmit_everything_next_frame;

    bool script_has_run_ever;
    void *script_wait_thread;
    void *script_job;
    s64 script_last_exit_code;
};
global_variable BackendWin32 backend;

void backend_main()
{
    {
        // Note (Morten, 2020-11-08) Force creation of a message queue in this thread, so if we start a script in app init, the messages sent by the script-wait thread will be properly received
        win32::window_message msg = {};
        win32::PeekMessageW(&msg, 0, win32::WM_APP, win32::WM_APP, win32::PM_NOREMOVE);
    }

    app_init();

    wchar_t *window_title = L"A-moll septim";

    backend.win32_cursor_normal = win32::LoadCursorW(0, (wchar_t *) win32::IDC_ARROW);

    win32::window_class WindowClass = {};
    WindowClass.WindowProcedure = win32::IsDebuggerPresent()? &my_window_proc : &my_window_proc_seh;
    WindowClass.Style = win32::CS_HREDRAW | win32::CS_VREDRAW;
    WindowClass.Instance = win32::GetModuleHandleW(null);
    WindowClass.ClassName = window_title;
    WindowClass.Cursor = backend.win32_cursor_normal;

    WindowClass.Icon = win32::LoadIconW(WindowClass.Instance, L"WINDOW_ICON");
    if (WindowClass.Icon == null) {
        debug_printf("Couldn't load icon resource\n");
    }

    u16 WindowClassAtom = win32::RegisterClassW(&WindowClass);
    assert(WindowClassAtom);

    s32 x = win32::CW_USEDEFAULT;
    s32 y = win32::CW_USEDEFAULT;
    s32 width = 640;
    s32 height = 480;

    void *window_handle = win32::CreateWindowExW(0, window_title, window_title, win32::WS_OVERLAPPEDWINDOW, x, y, width, height, null, null, WindowClass.Instance, 0);
    assert(window_handle);
    backend.window_handle = window_handle;

    win32::DragAcceptFiles(window_handle, 1);
    win32::SetCursor(backend.win32_cursor_normal);
    win32::ShowWindow(window_handle, win32::SW_SHOW);

    while (!backend.close_requested) {
        debug_assert(!global_stack_allocator.length); // Uh oh, somebody forgot a stack frame

        win32::window_message msg = {};
        s32 result = win32::GetMessageW(&msg, 0, 0, 0);
        assert(result != -1);

        if (msg.Message == win32::WM_APP) {
            ScriptWaitMessage *message = (ScriptWaitMessage *) msg.W;
            _script_handle_message(message);
            heap_free(message);
        } else {
            win32::TranslateMessage(&msg);
            win32::DispatchMessageW(&msg);
        }
    }

    debug_printf("You can call me; on another line\n");
    win32::ExitProcess(0);
}

int main()
{
    if (win32::IsDebuggerPresent()) {
        backend_main();
    } else {
        u32 my_exception_code;
        __try {
            backend_main();
        } __except (my_exception_code = win32::_exception_code(), 1) {
            _assert(true, "A critical, unrecoverable error occured (0x%x, %s)\n", my_exception_code, win32::_exception_message(my_exception_code));
        }
    }

    return(0);
}

void request_redraw()
{
    win32::InvalidateRect(backend.window_handle, null, false);
}

void request_close()
{
    win32::PostMessageW(backend.window_handle, win32::WM_CLOSE, 0, 0);
}

void toggle_fullscreen()
{
    char *Error = null;

    u32 WindowStyle = win32::GetWindowLongW(backend.window_handle, win32::GWL_STYLE);
    if (WindowStyle & win32::WS_OVERLAPPEDWINDOW) {
        win32::monitor_info MonitorInfo = { sizeof(win32::monitor_info) };
        if (win32::GetMonitorInfoW(win32::MonitorFromWindow(backend.window_handle, win32::MONITOR_DEFAULTTONEAREST), &MonitorInfo)) {
            s32 MonitorX = MonitorInfo.MonitorArea.Left;
            s32 MonitorY = MonitorInfo.MonitorArea.Top;
            s32 MonitorWidth = MonitorInfo.MonitorArea.Right - MonitorX;
            s32 MonitorHeight = MonitorInfo.MonitorArea.Bottom - MonitorY;

            if (win32::GetWindowPlacement(backend.window_handle, &backend.window_placement_before_fullscreen)) {
                WindowStyle &= ~win32::WS_OVERLAPPEDWINDOW;
                WindowStyle |= win32::WS_POPUP;
                win32::SetWindowLongW(backend.window_handle, win32::GWL_STYLE, WindowStyle);
                win32::SetWindowPos(backend.window_handle, (void *) win32::HWND_TOP,
                                    MonitorX, MonitorY, MonitorWidth, MonitorHeight,
                                    win32::SWP_NOOWNERZORDER | win32::SWP_FRAMECHANGED);
            } else {
                Error = "Couldn't get window placement";
            }
        } else {
            Error = "Couldn't determine monitor size";
        }
    } else {
        WindowStyle |= win32::WS_OVERLAPPEDWINDOW;
        WindowStyle &= ~win32::WS_POPUP;
        win32::SetWindowLongW(backend.window_handle, win32::GWL_STYLE, WindowStyle);
        win32::SetWindowPlacement(backend.window_handle, &backend.window_placement_before_fullscreen);
        win32::SetWindowPos(backend.window_handle, null, 0, 0, 0, 0,
                            win32::SWP_NOMOVE | win32::SWP_NOSIZE | win32::SWP_NOZORDER |
                            win32::SWP_NOOWNERZORDER | win32::SWP_FRAMECHANGED);
    }

    if (Error) {
        stack_enter_frame();
        wstr WideError = utf8_to_utf16(cstring_to_str(Error));
        win32::MessageBoxW(backend.window_handle, WideError.data, L"Couldn't toggle fullscreen", win32::MB_OK | win32::MB_ICONWARNING);
        stack_leave_frame();
    }
}

s64 my_window_proc(void *window_handle, s32 message, u64 w, s64 l)
{
    s64 result = S64_MIN;

    switch (message) {
        case win32::WM_CLOSE: {
            if (on_close_requested(false)) {
                backend.close_requested = true;
                win32::ShowWindow(window_handle, win32::SW_HIDE);
            }
            result = 0;
        } break;
        
        case win32::WM_SYSKEYDOWN: {
            // Always prompt for confirmation on Alt-F4, because I usually hit it by accident
            if (w == win32::VK_F4) {
                if (on_close_requested(true)) {
                    backend.close_requested = true;
                    win32::ShowWindow(window_handle, win32::SW_HIDE);
                }
                result = 0;
            }
        } break;

        case win32::WM_GETMINMAXINFO: {
            win32::min_max_info *info = (win32::min_max_info *) l;
            info->MinTrackSize.X = 480;
            info->MinTrackSize.Y = 320;
            result = 0;
        } break;

        case win32::WM_ACTIVATEAPP: {
            if (w) backend.resubmit_everything_next_frame = true;
        } break;

        case win32::WM_PAINT: {
            win32::rectangle area = {};
            win32::GetClientRect(window_handle, &area);
            s32 width = area.Right - area.Left;
            s32 height = area.Bottom - area.Top;

            result = 0;
            win32::paint_struct Paint = {};
            win32::BeginPaint(window_handle, &Paint);

            timing_start("resize");
            draw_target_set_size(&backend.backbuffer, width, height);
            DrawTargetSlice canvas = draw_target_as_slice(&backend.backbuffer);
            timing_start("redraw");
            bool redraw_again = redraw(canvas);
            timing_draw(&canvas);

            timing_start("draw_target_finish");
            draw_target_finish(&backend.backbuffer);

            timing_start("blit");
            win32::bitmap_info bmp = {};
            bmp.Header.Size = sizeof(bmp.Header);
            bmp.Header.Width = width;
            bmp.Header.Height = -height;
            bmp.Header.Planes = 1;
            bmp.Header.BitCount = 32;
            bmp.Header.Compression = win32::BI_RGB;

            s32 x0 = width;
            s32 x1 = 0;
            s32 y0 = height;
            s32 y1  = 0;
            for (s32 i = 0; i < alen(backend.backbuffer.grid); ++i) {
                DrawTarget::GridCell *cell = &backend.backbuffer.grid[i];
                if (cell->changed_this_frame) {
                    x0 = min(x0, cell->area.x0);
                    x1 = max(x1, cell->area.x1);
                    y0 = min(y0, cell->area.y0);
                    y1 = max(y1, cell->area.y1);
                }
            }

            if (backend.resubmit_everything_next_frame) {
                x0 = 0;
                x1 = width;
                y0 = 0;
                y1 = height;
                backend.resubmit_everything_next_frame = false;
            }

            // NB (Morten, 2020-07-19) We could also do individual calls to SetDIBitsToDevice for each changed cell, however that a) leads to tearing (because GDI exposes no swap-buffer control) and b) actually is slower even when we end up submitting less pixels
            if (x0 < x1 && y0 < y1) {
                win32::SetDIBitsToDevice(
                    Paint.DeviceContext,
                    x0, y0,
                    x1 - x0, y1 - y0,
                    x0, height - y1,
                    0, height,
                    backend.backbuffer.buffer, &bmp, win32::DIB_RGB_COLORS);
            }

            win32::EndPaint(window_handle, &Paint);

            if (redraw_again) request_redraw();
        
            timing_start("dwm_flush");
            win32::DwmFlush(); // TODO On some versions of windows, this might not block. We should try detecting that, and at least issue a sleep, so we don't repaint as fast as possible

            timing_finish_frame();
        } break;

        case win32::WM_KEYDOWN: {
            s32 was_down = (l >> 30) & 1;
            if (!was_down) win32::SetCursor(null);

            bool control = win32::GetKeyState(win32::VK_CONTROL) & 0x8000;
            bool shift = win32::GetKeyState(win32::VK_SHIFT) & 0x8000;
            bool alt = win32::GetKeyState(win32::VK_MENU) & 0x8000;

            s32 Key = 0;
            switch (w) {
                case win32::VK_PRIOR:    Key = CHAR_PAGE_UP; break;
                case win32::VK_NEXT:     Key = CHAR_PAGE_DOWN; break;
                case win32::VK_END:      Key = CHAR_END; break;
                case win32::VK_HOME:     Key = CHAR_HOME; break;
                case win32::VK_LEFT:     Key = CHAR_LEFT; break;
                case win32::VK_UP:       Key = CHAR_UP; break;
                case win32::VK_RIGHT:    Key = CHAR_RIGHT; break;
                case win32::VK_DOWN:     Key = CHAR_DOWN; break;
                case win32::VK_DELETE:   Key = CHAR_DELETE; break;
                case win32::VK_INSERT:   Key = CHAR_INSERT; break;
                case win32::VK_RETURN:   Key = '\r'; break;
                case win32::VK_TAB:      Key = '\t'; break;
                case win32::VK_BACK:     Key = '\b'; break;
                case win32::VK_F1:       Key = CHAR_F1; break;
                case win32::VK_F2:       Key = CHAR_F2; break;
                case win32::VK_F3:       Key = CHAR_F3; break;
                case win32::VK_F4:       Key = CHAR_F4; break;
                case win32::VK_F5:       Key = CHAR_F5; break;
                case win32::VK_F6:       Key = CHAR_F6; break;
                case win32::VK_F7:       Key = CHAR_F7; break;
                case win32::VK_F8:       Key = CHAR_F8; break;
                case win32::VK_F9:       Key = CHAR_F9; break;
                case win32::VK_F10:      Key = CHAR_F10; break;
                case win32::VK_F11:      Key = CHAR_F11; break;
                case win32::VK_F12:      Key = CHAR_F12; break;
            }

            if (Key != 0) {
                result = 0;
                on_typed(Key, control, shift);
                request_redraw();
            } else if (w >= 'A' && w <= 'Z' && control && !alt) {
                s32 codepoint = w;
                if (!shift) codepoint = codepoint + 'a' - 'A';
                shift = false;
                on_typed(codepoint, control, shift);
                request_redraw();
            }
        } break;

        case win32::WM_CHAR: {
            // NB (Morten, 2020-05-09) According to Mārtiņš Možeiko we will get two WM_CHAR messages if somebody types a character encoded with a surrogate pair in utf16. Maybe we want to handle that?
            result = 0;

            s32 codepoint = (s32) w;
            //s32 Repeat = L & 0xffff;
            //s32 Scancode = (L >> 16) & 0xff;
            //bool IsExtended = (Scancode >> 24) & 1;
            //bool AltDown = (Scancode >> 29) & 1;
            
            if (codepoint >= 1 && codepoint <= 26) {
                // These messages are generated when we press ctrl+letter, and we don't want them
            } else {
                bool control = win32::GetKeyState(win32::VK_CONTROL) & 0x8000;
                bool shift = win32::GetKeyState(win32::VK_SHIFT) & 0x8000;
                on_typed(codepoint, control, shift);
                request_redraw();
            }
        } break;

        case win32::WM_MOUSEWHEEL: {
            s32 delta = (s32) ((s16) ((u16) ((w >> 16) & 0xffff)));
            delta = (delta + sign(delta)*119) / 120;

            bool control = w & win32::MK_CONTROL;
            bool shift = w & win32::MK_SHIFT;

            s32 sx = l & 0xffff;
            s32 sy = (l >> 16) & 0xffff;
            win32::point Point = { sx, sy };
            win32::ScreenToClient(window_handle, &Point);
            s32 x = Point.X;
            s32 y = Point.Y;

            on_scroll(x, y, delta, control, shift);
            request_redraw();
        } break;

        case win32::WM_DROPFILES: {
            void *drop_handle = (void *) w;
            win32::point pos = {};
            win32::DragQueryPoint(drop_handle, &pos);
            u32 file_count = win32::DragQueryFileW(drop_handle, 0xffffffff, 0, 0);
            for (u32 i = 0; i < file_count; ++i) {
                u32 length = win32::DragQueryFileW(drop_handle, i, 0, 0);
                stack_enter_frame();
                Path path = _path_empty(length);
                win32::DragQueryFileW(drop_handle, i, path->data, length + 1);
                on_file_dropped(pos.X, pos.Y, path);
                stack_leave_frame();
            }
            win32::DragFinish(drop_handle);
            win32::SetForegroundWindow(window_handle);
            request_redraw();
        } break;

        case win32::WM_LBUTTONUP:
        case win32::WM_LBUTTONDOWN:
        {
            bool down = message == win32::WM_LBUTTONDOWN;
            if (down) win32::SetCapture(window_handle); else win32::ReleaseCapture();

            if (down) {
                s32 x = (s16) (l & 0xffff);
                s32 y = (s16) ((l >> 16) & 0xffff);
                bool control = w & win32::MK_CONTROL;
                bool shift = w & win32::MK_SHIFT;
                bool changed = on_click(x, y, false, control, shift);
                if (changed) request_redraw();
            }
        } break;

        case win32::WM_MOUSEMOVE: {
            win32::SetCursor(backend.win32_cursor_normal);

            if (w & win32::MK_LBUTTON) {
                s32 x = (s16) (l & 0xffff);
                s32 y = (s16) ((l >> 16) & 0xffff);

                bool control = w & win32::MK_CONTROL;
                bool shift = w & win32::MK_SHIFT;
                bool changed = on_click(x, y, true, control, shift);
                if (changed) request_redraw();
            }
        } break;

        enum { BACKEND_THREE_SECOND_TIMER_IDENTIFIER = 90125 };
        case win32::WM_SETFOCUS: {
            win32::SetTimer(window_handle, BACKEND_THREE_SECOND_TIMER_IDENTIFIER, 1, null);
        } break;
        case win32::WM_KILLFOCUS: {
            win32::KillTimer(window_handle, BACKEND_THREE_SECOND_TIMER_IDENTIFIER);
        } break;
        case win32::WM_TIMER: {
            if (w == BACKEND_THREE_SECOND_TIMER_IDENTIFIER) {
                bool changed = on_three_second_timer();
                if (changed) request_redraw();
                win32::SetTimer(window_handle, BACKEND_THREE_SECOND_TIMER_IDENTIFIER, 3000, null);
            }
        } break;
    }

    if (result == S64_MIN) {
        result = win32::DefWindowProcW(window_handle, message, w, l);
    }
    return(result);
}

// TODO We currently just do the bare minimum to support ansi text, and hope we only really have to deal with unicode text most of the time. In theory, when reading from the clipboard we still have to read CF_LOCALE and use it to properly decode from ansi. When pasting we would have to determine the optimal locale and translate unicode into that before passing it into CF_TEXT. Implementing this without having a program which only outputs CF_TEXT for testing seems pointless though.
char *put_in_clipboard(str data)
{
    char *error = null;

    stack_enter_frame();
    wstr encoded_string = utf8_to_utf16(data);

    void *global_mem = (u16*) win32::GlobalAlloc(win32::GMEM_MOVEABLE, (encoded_string.length + 1) * sizeof(wchar_t));
    if (!global_mem) {
        error = "Couldn't allocate space for unicode text";
    } else {
        u16 *locked_global_mem = (u16 *) win32::GlobalLock(global_mem);
        if (!locked_global_mem) {
            error = "Couldn't lock memory for unicode text";
            win32::GlobalFree(global_mem);
        } else {
            memcpy(locked_global_mem, encoded_string.data, encoded_string.length * sizeof(wchar_t));
            locked_global_mem[encoded_string.length] = 0;
            s32 lock_count = win32::GlobalUnlock(global_mem);
            assert(lock_count == 0);

            if (!win32::OpenClipboard(backend.window_handle)) {
                error = "Clipboard is in use by another process";
                win32::GlobalFree(global_mem);
            } else {
                win32::EmptyClipboard();
                win32::SetClipboardData(win32::CF_UNICODETEXT, global_mem);
                win32::CloseClipboard();
            }
        }
    }

    stack_leave_frame();
    return(error);
}

char *get_from_clipboard(str *result)
{
    char *error = null;
    if (win32::OpenClipboard(backend.window_handle)) {
        bool has_ansi = false;
        bool has_unicode = false;
        for (u32 format = 0; (format = win32::EnumClipboardFormats(format));) {
            has_ansi |= format == win32::CF_TEXT;
            has_unicode |= format == win32::CF_UNICODETEXT;
        }

        if (has_unicode) {
            void *data = win32::GetClipboardData(win32::CF_UNICODETEXT);
            wchar_t *unlocked_data = (wchar_t *) win32::GlobalLock(data);
            if (data) {
                s64 data_length = cstring_length(unlocked_data);
                *result = utf16_to_utf8({ unlocked_data, data_length });
                win32::GlobalUnlock(data);
            } else {
                error = "Couldn't retrieve unicode text from clipboard data";
            }
        } else if (has_ansi) {
            debug_printf("Pasting ANSI text from clipboard. This might give messed up output!\n");
            void *data = win32::GetClipboardData(win32::CF_TEXT);
            char *unlocked_data = (char *) win32::GlobalLock(data);
            if (data) {
                u64 data_length = cstring_length(unlocked_data);
                *result = windows_ansi_to_utf8((u8 *) unlocked_data, data_length);
                win32::GlobalUnlock(data);
            } else {
                error = "Couldn't retrieve ansi text from clipboard data";
            }
        } else {
            error = "No ansi or unicode text in clipboard";
        }

        win32::CloseClipboard();
    } else {
        error = "Couldn't access clipboard";
    }
    return(error);
}

bool prompt_with_dialog(str title, str message)
{
    stack_enter_frame();
    wstr wide_title = utf8_to_utf16(title);
    wstr wide_message = utf8_to_utf16(message);
    s32 result = win32::MessageBoxW(backend.window_handle, wide_message.data, wide_title.data, win32::MB_OKCANCEL | win32::MB_ICONWARNING | win32::MB_DEFBUTTON2);
    bool ok = result == win32::IDOK;
    stack_leave_frame();
    return(ok);
}


struct ScriptWaitThreadParameters
{
    void *io_port;
    void *job_object;
    void *pipe;
    u32 receive_thread_id;
    void *process;
};

u32 _script_wait_thread_routine(void *parameter)
{
    // NB Make sure we don't call any of our library functions, most of which are incredibly thread-unsafe

    ScriptWaitThreadParameters parameters = *((ScriptWaitThreadParameters *) parameter);
    heap_free(parameter);

    while (1) {
        u8 read_buffer[4096];
        u32 bytes_read;
        s32 read_result = win32::ReadFile(parameters.pipe, read_buffer, sizeof(read_buffer), &bytes_read, 0);
        if (read_result) {
            ScriptWaitMessage *message = (ScriptWaitMessage *) heap_alloc(sizeof(ScriptWaitMessage) + bytes_read);
            message->kind = ScriptWaitMessage::OUTPUT;
            message->output.length = bytes_read;
            message->output.data = (u8 *) (message + 1);
            memcpy(message->output.data, read_buffer, bytes_read);
            if (!win32::PostThreadMessageW(parameters.receive_thread_id, win32::WM_APP, (u64) message, 0)) goto post_failed;

            // Note (Morten, 2020-11-08) We don't want to spam a bunch of messages when the script process does a lot of small writes, so we wait a brief moment to let the buffer fill up
            if (bytes_read != sizeof(read_buffer)) {
                win32::Sleep(10);
            }
        } else {
            u32 error = win32::GetLastError();
            if (error == win32::ERROR_BROKEN_PIPE) {
                // The initial process has died, now wait for the job to complete
                break;
            } else {
                ScriptWaitMessage *message = (ScriptWaitMessage *) heap_alloc(sizeof(ScriptWaitMessage));
                message->kind = ScriptWaitMessage::FAILED;
                message->error_code = error;
                if (!win32::PostThreadMessageW(parameters.receive_thread_id, win32::WM_APP, (u64) message, 0)) goto post_failed;
                goto done;
            }
        }
    }

    while (1) {
        u32 code = 0;
        u64 key = 0;
        win32::overlapped *overlapped = 0;
        if (!win32::GetQueuedCompletionStatus(parameters.io_port, &code, &key, &overlapped, U32_MAX)) {
            u32 error = win32::GetLastError();
            if (error == 0) error = U32_MAX;

            ScriptWaitMessage *message = (ScriptWaitMessage *) heap_alloc(sizeof(ScriptWaitMessage));
            message->kind = ScriptWaitMessage::FAILED;
            message->error_code = error;
            if (!win32::PostThreadMessageW(parameters.receive_thread_id, win32::WM_APP, (u64) message, 0)) goto post_failed;

            goto done;
        } else {
            if (key == (u64) parameters.job_object && code == win32::JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO) {
                u32 exit_code = 0;
                win32::GetExitCodeProcess(parameters.process, &exit_code);

                ScriptWaitMessage *message = (ScriptWaitMessage *) heap_alloc(sizeof(ScriptWaitMessage));
                message->kind = ScriptWaitMessage::DONE;
                message->exit_code = exit_code;
                if (!win32::PostThreadMessageW(parameters.receive_thread_id, win32::WM_APP, (u64) message, 0)) goto post_failed;

                break;
            }
        }
    }

    if (false) {
    post_failed:
        u32 error = win32::GetLastError();
        fail("Couldn't send post script message (%xh)\n", error);
    }

done:
    win32::TerminateJobObject(parameters.job_object, 0);
    win32::CloseHandle(parameters.job_object);

    win32::CloseHandle(parameters.io_port);

    win32::CancelIo(parameters.pipe);
    win32::CloseHandle(parameters.pipe);

    win32::CloseHandle(parameters.process);

    return(0);
}

void _script_handle_message(ScriptWaitMessage *message)
{
    assert(backend.script_job && backend.script_wait_thread);

    switch (message->kind) {
        case ScriptWaitMessage::OUTPUT: {
            str text = {};
            text.data = (char *) message->output.data;
            text.length = message->output.length;
            on_script_output(text);
        } break;

        case ScriptWaitMessage::DONE:
        case ScriptWaitMessage::FAILED:
        {
            win32::WaitForSingleObject(backend.script_wait_thread, U32_MAX);
            win32::CloseHandle(backend.script_wait_thread);

            backend.script_job = null;
            backend.script_wait_thread = null;

            stack_enter_frame();
            str text;
            if (message->kind == ScriptWaitMessage::DONE) {
                text = stack_printf("Process exited with code %xh\n", message->exit_code);
                backend.script_last_exit_code = message->exit_code;
            } else {
                text = stack_printf("Critical error while waiting on process (%xh)\n", message->error_code);
                backend.script_last_exit_code = U32_MAX;
            }
            on_script_output(text);
            stack_leave_frame();
        } break;

        default: assert(false);
    }

    request_redraw();
}

void start_script(Path script_path)
{
    if (!backend.script_wait_thread) {
        assert(!backend.script_job);
        stack_enter_frame();

        on_script_output(stack_printf("Launching %s\n", path_to_str(script_path).data));

        void *io_port = win32::CreateIoCompletionPort((void *) -1, null, 0, 1);
        if (!io_port) {
            u32 error = win32::GetLastError();
            fail("Couldn't create io port for script (%u)\n", error);
        }

        void *job = win32::CreateJobObjectW(null, null);
        if (!job) {
            u32 error = win32::GetLastError();
            fail("Couldn't create job for script execution (%u)\n", error);
        }
        win32::jobobject_extended_limit_information job_settings = {};
        job_settings.BasicLimitInformation.LimitFlags = win32::JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | win32::JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION;
        if (!win32::SetInformationJobObject(job, win32::JobObjectExtendedLimitInformation, (void *) &job_settings, sizeof(job_settings))) {
            u32 error = win32::GetLastError();
            fail("Couldn't configure job for script execution (%u)\n", error);
        }
        win32::jobobject_associate_completion_port job_port_link = {};
        job_port_link.Key = job;
        job_port_link.Port = io_port;
        if (!win32::SetInformationJobObject(job, win32::JobObjectAssociateCompletionPortInformation, (void *) &job_port_link, sizeof(job_port_link))) {
            u32 error = win32::GetLastError();
            fail("Couldn't configure job for script execution (%u)\n", error);
        }

        str pipe_name = stack_printf("\\\\.\\pipe\\am7_script_pipe_%u", win32::GetCurrentProcessId());
        wchar_t *pipe_name_w = utf8_to_utf16(pipe_name).data;

        void *receive_pipe = win32::CreateNamedPipeW(pipe_name_w, win32::PIPE_ACCESS_INBOUND | win32::FILE_FLAG_FIRST_PIPE_INSTANCE, 0, 2, 0, 4096, 0, null);
        if (receive_pipe == ((void *) -1)) {
            u32 error = win32::GetLastError();
            fail("Couldn't create server pipe for script execution (%u)\n", error);
        }

        win32::security_attributes sec = {};
        sec.Length = sizeof(sec);
        sec.InheritHandle = true;
        void *send_pipe = win32::CreateFileW(pipe_name_w, win32::GENERIC_WRITE, 0, &sec, win32::OPEN_EXISTING, 0, null);
        if (send_pipe == ((void *) -1)) {
            u32 error = win32::GetLastError();
            fail("Couldn't create client pipe for script execution (%u)\n", error);
        }

        win32::startup_info_w startup_info = {};
        startup_info.StructSize = sizeof(startup_info);

        #if 1
        startup_info.Flags = win32::STARTF_USESTDHANDLES;
        startup_info.StdIn = null;
        startup_info.StdOut = send_pipe;
        startup_info.StdErr = send_pipe;
        #else
        #pragma pack(push, 1)
        struct
        {
            u32 Length;
            u8 Flags[2];
            void *Handles[2];
        } Secret;
        #pragma pack(pop)
        Secret.Length = 2;
        Secret.Flags[0] = 0;
        Secret.Flags[1] = 0x41;
        Secret.Handles[0] = (void *) -1;
        Secret.Handles[1] = SendPipe;
        startup_info.Reserved2 = sizeof(Secret);
        startup_info.Reserved3 = (u8 *) &Secret;
        #endif

        Path script_dir = path_parent(script_path);

        wchar_t *prefix = L"/c \"";
        wchar_t *postfix = L"\"";
        s32 prefix_length = cstring_length(prefix);
        s32 postfix_length = cstring_length(postfix);
        wchar_t *command = stack_alloc(wchar_t, script_path->length + prefix_length + postfix_length + 1);
        memcpy(&command[0], prefix, prefix_length*sizeof(wchar_t));
        memcpy(&command[prefix_length], script_path->data, script_path->length*sizeof(wchar_t));
        memcpy(&command[prefix_length + script_path->length], postfix, postfix_length*sizeof(wchar_t));
        command[prefix_length + script_path->length + postfix_length] = 0;
        // NB 'CreateProcessW' may modify 'command' as per the windows documentation

        wchar_t *app_name = L"C:\\Windows\\System32\\cmd.exe";
        win32::process_info process_info = {};
        bool created_process = win32::CreateProcessW(app_name, command, null, null, true, win32::CREATE_NO_WINDOW | win32::CREATE_SUSPENDED, null, script_dir->data, &startup_info, &process_info);

        if (!created_process) {
            u32 error = win32::GetLastError();
            str message = stack_printf("Couldn't launch child process (%u)\n", error);
            debug_printf(message.data);
            on_script_output(message);

            win32::CloseHandle(job);
            win32::CloseHandle(io_port);
            win32::CloseHandle(receive_pipe);
        }

        win32::CloseHandle(send_pipe);

        if (created_process) {
            win32::AssignProcessToJobObject(job, process_info.ProcessHandle);

            ScriptWaitThreadParameters *parameters = (ScriptWaitThreadParameters *) heap_alloc(sizeof(ScriptWaitThreadParameters));
            parameters->io_port = io_port;
            parameters->job_object = job;
            parameters->pipe = receive_pipe;
            parameters->receive_thread_id = win32::GetCurrentThreadId();
            parameters->process = process_info.ProcessHandle;
            backend.script_wait_thread = win32::CreateThread(null, 0, &_script_wait_thread_routine, (void *) parameters, 0, null);
            if (!backend.script_wait_thread) {
                u32 Error = win32::GetLastError();
                fail("Couldn't start thread for script execution (%u)\n", Error);
            }

            win32::ResumeThread(process_info.ThreadHandle);
            win32::CloseHandle(process_info.ThreadHandle);

            backend.script_job = job;
            backend.script_has_run_ever = true;
        }

        stack_leave_frame();
    }
}

void stop_script()
{
    if (backend.script_job) {
        win32::TerminateJobObject(backend.script_job, 4);
    }
}

s64 script_last_exit_code()
{
    if (!backend.script_has_run_ever) {
        return(SCRIPT_HASNT_RUN);
    } else if (backend.script_job) {
        return(SCRIPT_STILL_RUNNING);
    } else {
        return(backend.script_last_exit_code);
    }
}