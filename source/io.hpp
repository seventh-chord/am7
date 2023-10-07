#pragma once

#include "util.hpp"
#include "unicode.h"

struct _Path;
typedef _Path *Path;

Path stack_copy(Path source);
Path heap_copy(Path source);
Path arena_copy(Arena *arena, Path source);

bool path_compare(Path left, Path right);

Path path_make_absolute(Path subpath, Path relative_to);
bool path_is_directory(Path maybe_directory);
bool path_is_normal_file(Path maybe_file);
Path path_parent(Path source);

Path str_to_path(char *string);
Path str_to_path(str string);
Path str_to_path(wchar_t *string);
Path str_to_path(wstr string);

str path_to_str(Path Path);
str path_to_str_relative(Path path, Path relative_to);
str path_extension(Path Path);

Path get_appdata_path();
Path get_working_directory();
void set_working_directory(Path Path);

Path show_open_file_dialog(Path initial_directory, bool try_create_file);
Path show_save_file_dialog(Path initial_directory, bool try_create_file);
Path show_open_directory_dialog();


enum struct IoError
{
    OK = 0,
    FILE_NOT_FOUND,
    DIRECTORY_NOT_FOUND,
    UNKNOWN_ERROR,
    ALREADY_OPEN,
    INVALID_DATA,
    ALREADY_EXISTS,
};
char *io_error_to_str(IoError error);

IoError create_file(Path path);
IoError create_directory(Path path);
IoError read_entire_file(Path path, str *data);
IoError write_entire_file(Path path, str data);

struct DirItem
{
    str display_string;
    Path full_path;
    DirItem *next;
};


enum { DirItemList_MAX = 2*1024 };

struct DirItemList
{
    DirItem *first;
    s32 count;
};

DirItemList path_list_directory(Path directory);



char *io_error_to_str(IoError error)
{
    char *result = null;
    switch (error) {
        case IoError::OK: result = "Ok"; break;
        case IoError::FILE_NOT_FOUND: result = "File not found"; break;
        case IoError::DIRECTORY_NOT_FOUND: result = "Directory not found"; break;
        case IoError::UNKNOWN_ERROR: result = "Unkown error"; break;
        case IoError::ALREADY_OPEN: result = "File already open"; break;
        case IoError::ALREADY_EXISTS: result = "Already exists"; break;
        case IoError::INVALID_DATA: result = "Found invalid data"; break;
        default: assert(false);
    }
    return(result);
}

Path str_to_path(char *string)
{
    return(str_to_path(cstring_to_str(string)));
}

Path str_to_path(str string)
{
    wstr wide = utf8_to_utf16(string);
    Path path = str_to_path(wide);
    return(path);
}

Path str_to_path(wchar_t *string)
{
    return(str_to_path(cstring_to_wstr(string)));
}



#include "win32.hpp"

struct _Path
{
    s16 length;
    u32 identifier[3];
    wchar_t data[0]; // Guaranteed to be null-terminated
};

Path stack_copy(Path source)
{
    Path copy = null;
    if (source) {
        s32 size = (source->length + 1)*sizeof(wchar_t) + sizeof(_Path);
        copy = (Path) stack_alloc_aligned(size, alignof(_Path));
        copy->length = source->length;
        memcpy(copy, source, size);
    }
    return(copy);
}

Path heap_copy(Path source)
{
    Path copy = null;
    if (source) {
        s32 size = (source->length + 1)*sizeof(wchar_t) + sizeof(_Path);
        copy = (Path) heap_alloc(size);
        copy->length = source->length;
        memcpy(copy, source, size);
    }
    return(copy);
}

Path arena_copy(Arena *arena, Path source)
{
    Path copy = null;
    if (source) {
        s32 size = (source->length + 1)*sizeof(wchar_t) + sizeof(_Path);
        copy = (Path) arena_alloc_aligned(arena, size, alignof(_Path));
        copy->length = source->length;
        memcpy(copy, source, size);
    }
    return(copy);
}

Path get_working_directory()
{
    u32 required = win32::GetCurrentDirectoryW(0, null);
    if (required == 0) {
        u32 error = win32::GetLastError();
        fail("Couldn't get current working directory (%u)\n", error);
    }
    
    Path result = (Path) stack_alloc_aligned(sizeof(_Path) + required*sizeof(wchar_t), alignof(_Path));
    u32 actual = win32::GetCurrentDirectoryW(required, &result->data[0]);
    assert(actual != 0 && (actual + 1) == required);
    result->length = actual;
    assert(result->data[result->length] == 0);
    return(result);
}

void set_working_directory(Path path)
{
    win32::SetCurrentDirectoryW(path->data);
}

wchar_t *_path_to_extended_format(Path path)
{
    wchar_t *result = path->data;
    if (!win32::PathIsUNCW(result)) {
        result = stack_alloc(wchar_t, path->length + 5);
        memcpy(result, L"\\\\?\\", 4*sizeof(wchar_t));
        memcpy(&result[4], path->data, path->length*sizeof(wchar_t));
        result[path->length + 4] = 0;
    }
    return(result);
}

bool _path_load_identifier(Path path)
{
    // Note (Morten, 2020-03-12)
    // According to MSDN:
    //  - On NTFS file indices are guaranteed to stay constant for a given file until it is deleted
    //  - On FAT32 indices stay consistent except for during defragmentation with some defragmenters (the windows defragmenter keeps indices consistent)
    // I am not sure whether or not we might get issues with files being renamed, etc. This might be something we want to catch at a different level anyways though.
    // One issue this presents is that it won't detect when two new-and-not-yet-saved files are equal...
    
    bool has_identifier = path->identifier[0] || path->identifier[1] || path->identifier[2];
    if (!has_identifier) {
        stack_enter_frame();
        wchar_t *open_path  = _path_to_extended_format(path);
        void *handle = win32::CreateFileW(open_path, 0, win32::FILE_SHARE_READ | win32::FILE_SHARE_WRITE | win32::FILE_SHARE_DELETE, null, win32::OPEN_EXISTING, win32::FILE_ATTRIBUTE_NORMAL, null);
        stack_leave_frame();

        if (handle == ((void *) -1) || handle == 0) {
            has_identifier = false; // Can't have an identifier if the file doesn't exist
        } else {
            win32::by_handle_file_information info;
            s32 result = win32::GetFileInformationByHandle(handle, &info);
            if (!result) {
                has_identifier = false; // Maybe we actually need to look into the situation here...
            } else {
                path->identifier[0] = info.VolumeSerialNumber;
                path->identifier[1] = info.FileIndexLow;
                path->identifier[2] = info.FileIndexHigh;
                has_identifier = true;
            }
            win32::CloseHandle(handle);
        }
    }
    
    return(has_identifier);
}

bool path_compare(Path left, Path right)
{
    if (!left || !right) return(!left && !right);
    if (left->length == right->length && memcmp(left->data, right->data, left->length*sizeof(wchar_t)) == 0) return(true);
    bool left_ok = _path_load_identifier(left);
    bool right_ok = _path_load_identifier(right);
    return(left_ok && right_ok && (memcmp(left->identifier, right->identifier, sizeof(left->identifier)) == 0));
}

Path _path_empty(s16 size)
{
    Path result = (Path) stack_alloc_aligned((size + 1)*sizeof(wchar_t) + sizeof(_Path), alignof(_Path));
    memset(result, 0, sizeof(_Path));
    result->length = size;
    result->data[size] = 0;
    return(result);
}

Path str_to_path(wstr source)
{
    Path result = _path_empty(source.length);
    memcpy(result->data, source.data, source.length * sizeof(wchar_t));
    for (s32 i = 0; i < result->length; ++i) {
        if (result->data[i] == '/') {
            result->data[i] = '\\';
        }
    }
    return(result);
}

str path_extension(Path path)
{
    wstr wide = {};
    if (path) {
        for (s16 i = path->length - 1; i >= 0; --i) {
            if (path->data[i] == '.') {
                wide = { &path->data[i + 1], (s64) (path->length - i - 1) };
                break;
            } else if (path->data[i] == '/' || path->data[i] == '\\') {
                break;
            }
        }
    }
    
    str result = {};
    if (wide.length > 0) result = utf16_to_utf8(wide);
    return(result);
}


str path_to_str(Path path)
{
    wstr wide = { path->data, (s64) path->length };
    str result = utf16_to_utf8(wide);
    return(result);
}

str path_to_str_relative(Path path, Path relative_to)
{
    s16 rel_length = relative_to->length;
    if (rel_length > 0 && relative_to->data[rel_length - 1] == '\\') --rel_length;
    bool can_remove = rel_length + 2 <= path->length && path->data[rel_length] == '\\';
    for (s32 i = 0; i < rel_length && can_remove; ++i) can_remove = path->data[i] == relative_to->data[i];

    wchar_t *wide = (wchar_t *) path->data;
    s16 wide_length = path->length;
    if (can_remove) {
        wide += rel_length + 1;
        wide_length -= rel_length + 1;
    }

    str result = utf16_to_utf8({ wide, (s64) wide_length });
    return(result);
}

Path path_parent(Path source)
{
    if (source) {
        Path parent = stack_copy(source);
        win32::PathCchRemoveBackslashEx(parent->data, parent->length + 1, 0, 0);
        bool ok = win32::PathCchRemoveFileSpec(parent->data, parent->length) == 0;
        if (ok) {
            parent->length = cstring_length(parent->data);
            return(parent);
        }
    }
    return(null);
}


IoError read_entire_file(Path path, str *result)
{
    IoError error = IoError::OK;
    *result = {};
    
    stack_enter_frame();
    wchar_t *open_path = _path_to_extended_format(path);
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
        } else if (size > U32_MAX) {
            error = IoError::UNKNOWN_ERROR;
        } else {
            result->length = size;
            s64 allocation_size = round_up(size + 1, 16);
            result->data = (char *) stack_alloc_aligned(allocation_size, 16);

            for (s64 i = size; i < allocation_size; ++i) result->data[i] = 0;

            u32 read;
            s32 read_result = win32::ReadFile(handle, result->data, (u32) size, &read, null);
            if (read_result == 0 || read != size) {
                result->data = null;
                error = IoError::UNKNOWN_ERROR;
            } else {
                error = IoError::OK;
            }
        }
        win32::CloseHandle(handle);
    }

    return(error);
}

IoError write_entire_file(Path path, str data)
{
    IoError result = IoError::OK;

    stack_enter_frame();
    wchar_t *open_path = _path_to_extended_format(path);
    void *handle = win32::CreateFileW(open_path, win32::GENERIC_WRITE, 0, null, win32::CREATE_ALWAYS,
                                          win32::FILE_ATTRIBUTE_NORMAL, null);
    stack_leave_frame();

    if (handle == ((void*) -1)) {
        u32 error_code = win32::GetLastError();
        switch (error_code) {
            case win32::ERROR_FILE_NOT_FOUND: result = IoError::FILE_NOT_FOUND; break;
            case win32::ERROR_PATH_NOT_FOUND: result = IoError::DIRECTORY_NOT_FOUND; break;  // Or, apparently, the file name was too long
            case win32::ERROR_SHARING_VIOLATION: result = IoError::ALREADY_OPEN; break;
            default: result = IoError::UNKNOWN_ERROR; break;
        }
    } else {
        u32 written = 0;
        s32 success = win32::WriteFile(handle, data.data, data.length, &written, null);

        if (!success || written != data.length) {
            u32 error = win32::GetLastError();
            unused(error);
            result = IoError::UNKNOWN_ERROR;
        }

        win32::CloseHandle(handle);
    }

    return(result);
}

IoError create_directory(Path path)
{
    IoError result = IoError::OK;

    stack_enter_frame();
    wchar_t *open_path = _path_to_extended_format(path);
    s32 success = win32::CreateDirectoryW(open_path, null);
    if (!success) {
        u32 error_code = win32::GetLastError();
        switch (error_code) {
            case win32::ERROR_ALREADY_EXISTS: result = IoError::ALREADY_EXISTS; break;
            case win32::ERROR_PATH_NOT_FOUND: result = IoError::DIRECTORY_NOT_FOUND; break;
            default: result = IoError::UNKNOWN_ERROR; break;
        }
    }
    stack_leave_frame();

    return(result);
}

IoError create_file(Path path)
{
    IoError result = IoError::OK;

    stack_enter_frame();
    wchar_t *open_path = _path_to_extended_format(path);
    void *handle = win32::CreateFileW(open_path, win32::GENERIC_WRITE, 0, null, win32::CREATE_NEW,
                                          win32::FILE_ATTRIBUTE_NORMAL, null);
    stack_leave_frame();

    if (handle == ((void*) -1)) {
        u32 error_code = win32::GetLastError();
        switch (error_code) {
            case win32::ERROR_FILE_NOT_FOUND: result = IoError::FILE_NOT_FOUND; break;
            case win32::ERROR_PATH_NOT_FOUND: result = IoError::DIRECTORY_NOT_FOUND; break;  // Or, apparently, the file name was too long
            case win32::ERROR_SHARING_VIOLATION: result = IoError::ALREADY_OPEN; break;
            case win32::ERROR_FILE_EXISTS: result = IoError::ALREADY_EXISTS; break;
            default: result = IoError::UNKNOWN_ERROR; break;
        }
    } else {
        win32::CloseHandle(handle);
    }

    return(result);
}

Path show_open_file_dialog(Path initial_directory, bool try_create_file)
{
    wchar name_buffer[1024] = { 0 };

    win32::open_file_name_w info = {};
    info.StructSize = sizeof(info);
    info.OwnerWindowHandle = win32::GetActiveWindow();
    info.File = name_buffer;
    info.MaxFile = array_length(name_buffer);
    info.Flags = win32::OFN_EXPLORER | win32::OFN_FORCESHOWHIDDEN | win32::OFN_NOCHANGEDIR;
    info.InitialDir = initial_directory->data;
    s32 success = win32::GetOpenFileNameW(&info);

    Path result = null;

    if (!success) {
        u32 error_code = win32::CommDlgExtendedError();
        if (error_code != 0) {
            debug_printf("Failed to show open file dialog (%u)\n", error_code);
        }
    } else {
        s64 name_length = cstring_length(name_buffer, array_length(name_buffer));
        result = _path_empty(name_length);
        memcpy(result->data, name_buffer, name_length * sizeof(wchar_t));

        if (try_create_file) {
            IoError error = create_file(result);
            if (!(error == IoError::OK || error == IoError::ALREADY_EXISTS)) {
                stack_enter_frame();
                char *path_utf8 = path_to_str(result).data;
                str message = stack_printf("Couldn't create file \"%s\" (%s)", path_utf8, io_error_to_str(error));
                wchar_t *message_wide = utf8_to_utf16(message).data;
                win32::MessageBoxW(info.OwnerWindowHandle, message_wide, L"Couldn't create file", win32::MB_OK | win32::MB_ICONWARNING);
                stack_leave_frame();

                result = null;
            }
        }
    }

    return(result);
}

Path show_save_file_dialog(Path initial_directory)
{
    wchar name_buffer[1024] = { 0 };

    win32::open_file_name_w info = {};
    info.StructSize = sizeof(info);
    info.OwnerWindowHandle = win32::GetActiveWindow();
    info.File = name_buffer;
    info.MaxFile = array_length(name_buffer);
    info.Flags = win32::OFN_EXPLORER | win32::OFN_FORCESHOWHIDDEN | win32::OFN_NOCHANGEDIR | win32::OFN_OVERWRITEPROMPT;
    info.InitialDir = initial_directory->data;
    s32 success = win32::GetSaveFileNameW(&info);

    Path result = null;

    if (!success) {
        u32 error_code = win32::CommDlgExtendedError();
        if (error_code != 0) {
            debug_printf("Failed to show open file dialog (%u)\n", error_code);
        }
    } else {
        s64 name_length = cstring_length(name_buffer, array_length(name_buffer));
        result = _path_empty(name_length);
        memcpy(result->data, name_buffer, name_length * sizeof(wchar_t));
    }

    return(result);
}

Path show_open_directory_dialog()
{
    s32 result = 0;
    wchar_t *returned_path = null;
    win32::IFileOpenDialog *dialog = null;
    win32::IShellItem *item = null;
    win32::guid clsid = { 0xdc1c5a9c, 0xe88a, 0x4dde, { 0xa5, 0xa1, 0x60, 0xf8, 0x2a, 0x20, 0xae, 0xf7 } };
    win32::guid id = { 0xd57c7288, 0xd4ad, 0x4768, { 0xbe, 0x02, 0x9d, 0x96, 0x95, 0x32, 0xd9, 0x60 } };
    Path path = null;

    win32::CoInitialize(null);

    result = win32::CoCreateInstance(&clsid, null, 23, &id, (void *) &dialog);
    if (result != 0) goto error;
    result = dialog->vtbl->SetOptions(dialog, win32::FOS_PICKFOLDERS | win32::FOS_FORCEFILESYSTEM | win32::FOS_PATHMUSTEXIST | win32::FOS_FILEMUSTEXIST | win32::FOS_NOCHANGEDIR);
    if (result != 0) goto error;
    result = dialog->vtbl->Show(dialog, win32::GetActiveWindow());
    if (result != 0) goto error;
    result = dialog->vtbl->GetResult(dialog, &item);
    if (result != 0) goto error;
    result = item->vtbl->GetDisplayName(item, 0x80058000, &returned_path);
    if (result != 0) goto error;

    if (returned_path) {
        s64 length = cstring_length(returned_path);
        assert(length < S16_MAX);
        path = _path_empty(length);
        memcpy(path->data, returned_path, length*sizeof(wchar_t));
        win32::CoTaskMemFree((void *) returned_path);
    }

error:
    if (item) item->vtbl->Release(item);
    if (dialog) dialog->vtbl->Release(dialog);
    if (result != 0 && result != win32::ERROR_CANCELED) {
        debug_printf("Couldn't show directory dialog (%i)\n", result);
    }

    return(path);
}

Path get_appdata_path()
{
    win32::guid guid = { 0x3eb685db, 0x65f9, 0x4cf6, { 0xa0, 0x3a, 0xe3, 0xef, 0x65, 0x72, 0x9f, 0x3d } };
    wchar_t *buffer = null;
    win32::SHGetKnownFolderPath(&guid, 0, 0, &buffer);
    u64 buffer_length = cstring_length(buffer);
    Path result = _path_empty(buffer_length);
    memcpy(result->data, buffer, buffer_length * sizeof(wchar_t));
    win32::CoTaskMemFree((void *) buffer);
    return(result);
}

Path _path_make_absolute(wchar_t *subpath, wchar_t *relative_to)
{
    s32 subpath_length = cstring_length(subpath);
    s32 relative_to_length = cstring_length(relative_to);
    s32 max_length = min(subpath_length + relative_to_length + 2, S16_MAX);

    Path absolute = _path_empty(max_length);
    s32 result = win32::PathCchCombineEx(absolute->data, max_length, relative_to, subpath, win32::PATHCCH_ALLOW_LONG_PATHS);
    if (result == 0) {
        win32::PathCchRemoveBackslashEx(absolute->data, max_length + 1, 0, 0);

        s64 actual_length = cstring_length(absolute->data);
        assert(actual_length <= max_length);
        absolute->length = (s16) actual_length;

        s64 old_allocation_size = stack_get_allocation_size(absolute);
        s64 new_allocation_size = old_allocation_size - max_length + actual_length;
        stack_trim_allocation(absolute, new_allocation_size);
    } else {
        absolute = null;
    }
    return(absolute);
}

Path path_make_absolute(Path subpath, Path relative_to)
{
    return(_path_make_absolute(subpath->data, relative_to->data));
}

DirItemList path_list_directory(Path directory)
{
    DirItemList result = {};
    DirItem *list_tail = 0;

    struct DirectoryFifo
    {
        wstr subdirectory;
        DirectoryFifo *next;
    };

    DirectoryFifo *fifo_head = stack_alloc(DirectoryFifo, 1);
    fifo_head->subdirectory = {};
    fifo_head->next = 0;
    DirectoryFifo *fifo_tail = fifo_head;

    while (fifo_head && result.count < DirItemList_MAX) {
        wstr subdirectory = fifo_head->subdirectory;
        fifo_head = fifo_head->next;
        if (!fifo_head) {
            fifo_tail = 0;
        }

        wchar_t *search_path = stack_alloc(wchar_t, directory->length+1 + subdirectory.length+1 + 2);
        s32 search_path_length = 0;

        memcpy(search_path + search_path_length, directory->data, directory->length*sizeof(wchar_t));
        search_path_length += directory->length;
        if (search_path[search_path_length - 1] != '\\') search_path[search_path_length++] = '\\';

        memcpy(search_path + search_path_length, subdirectory.data, subdirectory.length*sizeof(wchar_t));
        search_path_length += subdirectory.length;
        if (search_path[search_path_length - 1] != '\\') search_path[search_path_length++] = '\\';

        search_path[search_path_length++] = '*';
        search_path[search_path_length] = 0;

        win32::find_data_w find_data;
        void *handle = win32::FindFirstFileW(search_path, &find_data);
        bool done = handle == ((void *) -1);
        while (!done && result.count < DirItemList_MAX) {
            wstr file_name = { find_data.FileName, cstring_length(find_data.FileName, alen(find_data.FileName)) };

            if (file_name == L"." || file_name == L"..") {
                // Skip these entries.

            } else if (file_name == L".git") {
                // I don't want the mess in .git to show up in the file browser.
                // TODO maybe we should have some more inteligent logic here?

            } else {
                wstr file_relative_path;
                file_relative_path.data = stack_alloc(wchar_t, subdirectory.length + 1 + file_name.length + 1);
                file_relative_path.length = 0;

                if (subdirectory.length > 0) {
                    memcpy(file_relative_path.data + file_relative_path.length, subdirectory.data, subdirectory.length * sizeof(wchar_t));
                    file_relative_path.length += subdirectory.length;
                    if (file_relative_path.data[file_relative_path.length - 1] != '\\') {
                        file_relative_path.data[file_relative_path.length++] = '\\';
                    }
                }

                memcpy(file_relative_path.data + file_relative_path.length, file_name.data, file_name.length * sizeof(wchar_t));
                file_relative_path.length += file_name.length;

                file_relative_path.data[file_relative_path.length] = 0;

                // Directories.
                if (find_data.FileAttributes & win32::FILE_ATTRIBUTE_DIRECTORY) {
                    DirectoryFifo *fifo_entry = stack_alloc(DirectoryFifo, 1);
                    fifo_entry->subdirectory = file_relative_path;
                    if (fifo_tail) {
                        fifo_tail->next = fifo_entry;
                    } else {
                        fifo_head = fifo_entry;
                    }
                    fifo_tail = fifo_entry;

                // Normal files.
                } else {
                    DirItem *item = stack_alloc(DirItem, 1);
                    item->display_string = utf16_to_utf8(file_relative_path);
                    item->full_path = _path_make_absolute(file_relative_path.data, directory->data);
                    item->next = null;

                    result.count += 1;
                    if (list_tail) {
                        list_tail->next = item;
                    } else {
                        result.first = item;
                    }
                    list_tail = item;
                }
            }

            s32 find_result = win32::FindNextFileW(handle, &find_data);
            done = find_result == 0;
        }
        if (handle != ((void *) -1)) {
            win32::FindClose(handle);
        }
    }

    return result;
}

bool path_is_directory(Path maybe_directory)
{
    return(win32::PathIsDirectoryW(maybe_directory->data));
}

bool path_is_normal_file(Path maybe_file)
{
    u32 attributes = win32::GetFileAttributesW(maybe_file->data);
    return(attributes == win32::FILE_ATTRIBUTE_NORMAL);
}
