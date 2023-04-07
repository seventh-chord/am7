#pragma once

#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "pathcch.lib")
#pragma comment(lib, "dwrite.lib")

namespace win32
{

typedef s64 (*window_procedure)(void *Window, s32 Message, u64 W, s64 L);

struct rectangle
{
    s32 Left;
    s32 Top;
    s32 Right;
    s32 Bottom;
};

struct point
{
    s32 X;
    s32 Y;
};

struct window_class
{
    u32 Style;
    window_procedure WindowProcedure;
    s32 RequestedExtraBytesAfterWindowClass;
    s32 RequestedExtraBytesAfterWindowInstance;
    void *Instance;
    void *Icon;
    void *Cursor;
    void *BackgroundBrush;
    wchar_t *MenuName;
    wchar_t *ClassName;
};

struct window_message
{
    void *WindowHandle;
    u32 Message;
    u64 W;
    s64 L;
    u32 Time;
    point Point;
};

struct create_struct_w {
    void *CreateParameters;
    void *Instance;
    void *Menu;
    void *ParentWindowHandle;
    s32 cy, cx, y, x;
    s32 Style;
    wchar_t *Name;
    wchar_t *Class;
    u32 ExStyle;
};

struct window_placement
{
    u32 StructSize;
    u32 Flags;
    u32 ShowCommand;
    point Min;
    point Max;
    rectangle Normal;
    rectangle Device;
};

struct min_max_info
{
    point Reserved;
    point MaxSize;
    point MaxPosition;
    point MinTrackSize;
    point MaxTrackSize;
};

struct paint_struct
{
    void *DeviceContext;
    s32 Erase;
    rectangle Area;
    s32 Restore;
    s32 IncUpdate;
    u8 Reserved[32];
};

struct bitmap_info_header
{
    u32 Size;
    s32 Width;
    s32 Height;
    u16 Planes;
    u16 BitCount;
    u32 Compression;
    u32 SizeImage;
    s32 XPelsPerMeter;
    s32 YPelsPerMeter;
    u32 ClrUsed;
    u32 ClrImportant;
};

struct bitmap_info
{
    bitmap_info_header Header;
    u32 Colors[1];
};

struct monitor_info
{
  u32 StructSize;
  rectangle MonitorArea;
  rectangle WorkArea;
  u32 Flags;
};

struct startup_info_w
{
  u32 StructSize;
  wchar_t *lpReserved;
  wchar_t *lpDesktop;
  wchar_t *lpTitle;
  u32 X;
  u32 Y;
  u32 XSize;
  u32 YSize;
  u32 XCountChars;
  u32 YCountChars;
  u32 FillAttribute;
  u32 Flags;
  u16 ShowWindow;
  u16 Reserved2;
  u8 *Reserved3;
  void *StdIn;
  void *StdOut;
  void *StdErr;
};

typedef void *proc_thread_attribute_list_pointer;

struct startup_info_ex_w
{
    startup_info_w StartupInfo;
    proc_thread_attribute_list_pointer AttributeList;
};

struct process_info
{
  void *ProcessHandle;
  void *ThreadHandle;
  u32 ProcessId;
  u32 ThreadId;
};

struct security_attributes
{
  u32 Length;
  void *SecurityDescriptor;
  s32 InheritHandle;
};

struct filetime
{
  u32 Low;
  u32 High;
};

struct find_data_w
{
    u32 FileAttributes;
    filetime CreationTime;
    filetime LastAccessTime;
    filetime LastWriteTime;
    u32 FileSizeHigh;
    u32 FileSizeLow;
    u32 Reserved0;
    u32 Reserved1;
    wchar_t FileName[260];
    wchar_t AlternateFileName[14];
    u32 FileType;
    u32 CreatorType;
    u16 FinderFlags;
};

struct by_handle_file_information
{
    u32 FileAttributes;
    filetime CreationTime;
    filetime LastAccessTime;
    filetime LastWriteTime;
    u32 VolumeSerialNumber;
    u32 FileSizeHigh;
    u32 FileSizeLow;
    u32 NumberOfLinks;
    u32 FileIndexHigh;
    u32 FileIndexLow;
};


struct open_file_name_w 
{
    u32 StructSize;
    void *OwnerWindowHandle;
    void *Instance;
    wchar_t *Filter;
    wchar_t *CustomFilter;
    u32 MaxCustomFilter;
    u32 FilterIndex;
    wchar_t *File;
    u32 MaxFile;
    u16 *FileTitle;
    u32 MaxFileTitle;
    wchar_t *InitialDir;
    wchar_t *Title;
    u32 Flags;
    u16 FileOffset;
    u16 FileExtension;
    wchar_t *DefaultExtension;
    s32 UserDataForHook;
    proc_address *Hook;
    wchar_t *TemplateName;
};

struct guid
{
  u32 Data1;
  u16 Data2;
  u16 Data3;
  u8  Data4[8];
};


// Dude I really want to know what the people who wrote COM were smoking
struct IShellItem;
struct IFileOpenDialog;

struct IShellItemVtbl
{
    proc_address QueryInterface;
    proc_address AddRef;
    u32 (*Release)(IShellItem *This);
    proc_address BindToHandler;
    proc_address GetParent;
    s32 (*GetDisplayName)(IShellItem *This, s32 Index, wchar_t **Name);
    proc_address GetAttributes;
    proc_address Compare;
};

struct IShellItem
{
    IShellItemVtbl *vtbl;
};

struct IFileOpenDialogVtbl
{
    proc_address QueryInterface;
    proc_address AddRef;
    u32 (*Release)(IFileOpenDialog *This);
    s32 (*Show)(IFileOpenDialog * This, void *OwnerWindowHandle);
    proc_address SetFileTypes;
    proc_address SetFileTypeIndex;
    proc_address GetFileTypeIndex;
    proc_address Advise;
    proc_address Unadvise;
    s32 (*SetOptions)(IFileOpenDialog *This, u32 Options);
    proc_address GetOptions;
    proc_address SetDefaultFolder;
    proc_address SetFolder;
    proc_address GetFolder;
    proc_address GetCurrentSelection;
    proc_address SetFileName;
    proc_address GetFileName;
    proc_address SetTitle;
    proc_address SetOkButtonLabel;
    proc_address SetFileNameLabel;
    s32 (*GetResult)(IFileOpenDialog *This, IShellItem **ppsi);
    proc_address AddPlace;
    proc_address SetDefaultExtension;
    proc_address Close;
    proc_address SetClientGuid;
    proc_address ClearClientData;
    proc_address SetFilter;
    proc_address GetResults;
    proc_address GetSelectedItems;
};

struct IFileOpenDialog
{
    IFileOpenDialogVtbl *vtbl;
};

struct overlapped
{
    u64 Internal;
    u64 InternalHigh;
    union {
        struct {
            u32 Offset;
            u32 OffsetHigh;
        };
        void *Pointer;
    };
    void *Event;
};

struct large_integer
{
    union {
        struct {
            u32 LowPart;
            s32 HighPart;
        };
        s64 QuadPart;
    };
};

struct jobobject_basic_limit_information
{
    large_integer PerProcessUserTimeLimit;
    large_integer PerJobUserTimeLimit;
    u32 LimitFlags;
    u64 MinimumWorkingSetSize;
    u64 MaximumWorkingSetSize;
    u32 ActiveProcessLimit;
    s64 Affinity;
    u32 PriorityClass;
    u32 SchedulingClass;
};

struct jobobject_extended_limit_information
{
    jobobject_basic_limit_information BasicLimitInformation;
    struct
    {
        u64 ReadOperationCount;
        u64 WriteOperationCount;
        u64 OtherOperationCount;
        u64 ReadTransferCount;
        u64 WriteTransferCount;
        u64 OtherTransferCount;
    } IoInfo;
    u64 ProcessMemoryLimit;
    u64 JobMemoryLimit;
    u64 PeakProcessMemoryUsed;
    u64 PeakJobMemoryUsed;
};

struct jobobject_associate_completion_port
{
    void *Key;
    void *Port;
};

struct jobobject_basic_process_id_list
{
  u32 NumberOfAssignedProcesses;
  u32 NumberOfProcessIdsInList;
  u64 ProcessIdList[1];
};

struct systemtime
{
    u16 Year;
    u16 Month;
    u16 DayOfWeek;
    u16 Day;
    u16 Hour;
    u16 Minute;
    u16 Second;
    u16 Milliseconds;
};

typedef u32 (*thread_start_routine)(void *);

struct logfont_w
{
    s32 Height;
    s32 Width;
    s32 Escapement;
    s32 Orientation;
    s32 Weight;
    u8 Italic;
    u8 Underline;
    u8 StrikeOut;
    u8 CharSet;
    u8 OutPrecision;
    u8 ClipPrecision;
    u8 Quality;
    u8 PitchAndFamily;
    u16 FaceName[32];
};

struct textmetric_w
{
    s32 Height;
    s32 Ascent;
    s32 Descent;
    s32 InternalLeading;
    s32 ExternalLeading;
    s32 AveCharWidth;
    s32 MaxCharWidth;
    s32 Weight;
    s32 Overhang;
    s32 DigitizedAspectX;
    s32 DigitizedAspectY;
    u16 FirstChar;
    u16 LastChar;
    u16 DefaultChar;
    u16 BreakChar;
    u8 Italic;
    u8 Underlined;
    u8 StruckOut;
    u8 PitchAndFamily;
    u8 CharSet;
};

struct abc
{
    s32 A;
    u32 B;
    s32 C;
};

struct size
{
    s32 X;
    s32 Y;
};

struct bitmapinfoheader
{
    u32 Size;
    s32 Width;
    s32 Height;
    u16 Planes;
    u16 BitCount;
    u32 Compression;
    u32 SizeImage;
    s32 XPelsPerMeter;
    s32 YPelsPerMeter;
    u32 ClrUsed;
    u32 ClrImportant;
};

struct bitmapinfo
{
    bitmapinfoheader Header;
    u16 IdkWindowsIsWierd[16];
};

struct choosecolorw
{
  u32 struct_size;
  void *owner;
  void *instance;
  u32 result;
  u32 *cust_colors;
  u32 flags;
  u64 cust_data;
  void *hook;
  wchar_t *template_name;
};


extern "C"
{
    __declspec(dllimport)
    void ExitProcess(u32 ExitCode);
    __declspec(dllimport)
    u32 GetLastError();
    __declspec(dllimport)
    u32 WaitForSingleObject(void *Handle, u32 Milliseconds);
    __declspec(dllimport)
    void *VirtualAlloc(void *Address, u64 Size, u32 AllocationType, u32 ProtectionMode);
    __declspec(dllimport)
    s32 VirtualFree(void *Address, u64 Size, u32 FreeType);
    __declspec(dllimport)
    void *LocalFree(void *);
    __declspec(dllimport)
    s32 WriteConsoleW(void *Console, wchar_t *Buffer, u32 BufferLength, u32 *CharsWritten, void *Reserved);
    __declspec(dllimport)
    void *GetStdHandle(u32 Key);
    __declspec(dllimport)
    s32 GetConsoleMode(void *Console, u32 *Mode);
    __declspec(dllimport)
    s32 WriteFile(void *File, void *Buffer, u32 BufferLength, u32 *BytesWritten, void *Overlapped);
    __declspec(dllimport)
    s32 QueryPerformanceFrequency(s64 *Result);
    __declspec(dllimport)
    s32 QueryPerformanceCounter(s64 *Result);
    __declspec(dllimport)
    void OutputDebugStringW(wchar_t *String);
    __declspec(dllimport)
    void DebugBreak();
    __declspec(dllimport)
    s32 IsDebuggerPresent();
    __declspec(dllimport)
    s32 MessageBoxW(void *WindowHandle, wchar_t *Text, wchar_t *Caption, u32 Flags);
    __declspec(dllimport)
    wchar_t **CommandLineToArgvW(wchar_t *CommandLine, s32 *ArgumentCountOut);
    __declspec(dllimport)
    wchar_t *GetCommandLineW();
    __declspec(dllimport)
    void *GetModuleHandleW(wchar_t *module_name);
    __declspec(dllimport)
    void *CreateWindowExW(u32 ExtendedStyle, wchar_t *ClassName, wchar_t *WindowName, u32 Style, s32 X, s32 Y, s32 Width, s32 Height, void *Parent, void *Menu, void *Instance, void *Parameter);
    __declspec(dllimport)
    s64 DefWindowProcW(void *Window, s32 Message, u64 W, s64 L);
    __declspec(dllimport)
    s32 ShowWindow(void *Window, s32 Action);
    __declspec(dllimport)
    s32 SetForegroundWindow(void *WindowHandle);
    __declspec(dllimport)
    s32 PostMessageW(void *Window, u32 Message, u64 W, s64 L);
    __declspec(dllimport)
    s32 PostThreadMessageW(u32 ThreadId, u32 Message, u64 W, s64 L);
    __declspec(dllimport)
    s32 PeekMessageW(window_message *Message, void *Window, u32 FilterMin, u32 FilterMax, u32 RemoveMessage);
    __declspec(dllimport)
    s32 GetMessageW(window_message *Message, void *Window, u32 FilterMin, u32 FilterMax);
    __declspec(dllimport)
    u32 MsgWaitForMultipleObjects(u32 Count, void **Handles, s32 WaitAll, u32 Milliseconds, u32 WakeMask);
    __declspec(dllimport)
    s32 TranslateMessage(window_message *Message);
    __declspec(dllimport)
    s64 DispatchMessageW(window_message *Message);
    __declspec(dllimport)
    s16 GetKeyState(s32 VirtualKeyCode);
    __declspec(dllimport)
    s32 GetWindowRect(void *Window, rectangle *Rectangle);
    __declspec(dllimport)
    s32 GetClientRect(void *Window, rectangle *Rectangle);
    __declspec(dllimport)
    s32 ScreenToClient(void *Window, point *Point);
    __declspec(dllimport)
    s32 SetWindowPos(void *Window, void *InsertAfterWindow, s32 X, s32 Y, s32 Width, s32 Height, u32 Flags);
    __declspec(dllimport)
    s32 GetWindowPlacement(void *Window, window_placement *Placement);
    __declspec(dllimport)
    s32 SetWindowPlacement(void *Window, window_placement *Placement);
    __declspec(dllimport)
    void *MonitorFromWindow(void *Window, u32 Flags);
    __declspec(dllimport)
    s32 GetMonitorInfoW(void *Monitor, monitor_info *Info);
    __declspec(dllimport)
    s64 GetWindowLongPtrW(void *Window, s32 Index);
    __declspec(dllimport)
    s64 SetWindowLongPtrW(void *Window, s32 Index, s64 NewValue);
    __declspec(dllimport)
    s32 GetWindowLongW(void *Window, s32 Index);
    __declspec(dllimport)
    s32 SetWindowLongW(void *Window, s32 Index, s32 NewValue);
    __declspec(dllimport)
    void *LoadIconW(void *Module, wchar_t *Name);
    __declspec(dllimport)
    u64 RegisterClassW(window_class *WindowClass);
    __declspec(dllimport)
    void *BeginPaint(void *WindowHandle, paint_struct *Paint);
    __declspec(dllimport)
    s32 EndPaint(void *WindowHandle, paint_struct *Paint);
    __declspec(dllimport)
    s32 SetDIBitsToDevice(void *DeviceContext, s32 DestX, s32 DestY, u32 Width, u32 Height, s32 SourceX, s32 SourceY, u32 StartScan, u32 LineCount, void *Data, bitmap_info *Info, u32 ColorMode);
    __declspec(dllimport)
    s32 Rectangle(void *DeviceContext, s32 left, s32 top, s32 right, s32 bottom);
    __declspec(dllimport)
    s32 InvalidateRect(void *WindowHandle, rectangle *Rectangle, s32 Erase);
    __declspec(dllimport)
    void Sleep(u32 Milliseconds);
    __declspec(dllimport)
    s32 DwmFlush();
    __declspec(dllimport)
    void *SetCursor(void *Cursor);
    __declspec(dllimport)
    void *LoadCursorW(void *InstanceHandle, wchar_t *Name);
    __declspec(dllimport)
    s32 OpenClipboard(void *WindowHandleToNewOwner);
    __declspec(dllimport)
    s32 EmptyClipboard();
    __declspec(dllimport)
    void *GetClipboardOwner();
    __declspec(dllimport)
    void *SetClipboardData(u32 Format, void *Memory);
    __declspec(dllimport)
    u32 EnumClipboardFormats(u32 Format);
    __declspec(dllimport)
    s32 GetPriorityClipboardFormat(u32 *PriorityList, s32 PriorityListCapacity);
    __declspec(dllimport)
    s32 IsClipboardFormatAvailable(u32 Format);
    __declspec(dllimport)
    void *GetClipboardData(u32 Format);
    __declspec(dllimport)
    s32 CloseClipboard();
    __declspec(dllimport)
    void *GlobalAlloc(u32 Flags, u64 Bytes);
    __declspec(dllimport)
    void *GlobalFree(void *Pointer);
    __declspec(dllimport)
    void *GlobalLock(void *Pointer);
    __declspec(dllimport)
    s32 GlobalUnlock(void *Pointer);
    __declspec(dllimport)
    void DragAcceptFiles(void *WindowHandle, s32 Accept);
    __declspec(dllimport)
    s32 DragQueryPoint(void *DropHandle, point *Point);
    __declspec(dllimport)
    u32 DragQueryFileW(void *DropHandle, u32 Index, wchar_t *PathBuffer, u32 BufferLength);
    __declspec(dllimport)
    void DragFinish(void *DropHandle);
    __declspec(dllimport)
    s32 CreateProcessW(wchar_t *ApplicationName, wchar_t *CommandLine, void *ProcessAttributes, void *ThreadAttributes, s32 InheritHandles, u32 CreationFlags, void *Environment, wchar_t *CurrentDirectory, startup_info_w *StartupInfo, process_info *ProcessInfo);
    __declspec(dllimport)
    s32 InitializeProcThreadAttributeList(proc_thread_attribute_list_pointer AttributeList, u32 AttributeCount, u32 Flags, u64 *Size);
    __declspec(dllimport)
    s32 GetExitCodeProcess(void *Process, u32 *ExitCode);
    __declspec(dllimport)
    s32 GetExitCodeThread(void *Thread, u32 *ExitCode);
    __declspec(dllimport)
    u32 GetCurrentThreadId();
    __declspec(dllimport)
    s32 CreatePipe(void **ReadPipe, void **WritePipe, security_attributes *Attributes, u32 Size);
    __declspec(dllimport)
    u32 GetCurrentDirectoryW(u32 BufferLength, wchar_t *Buffer);
    __declspec(dllimport)
    s32 SetCurrentDirectoryW(wchar_t *Path);
    __declspec(dllimport)
    u32 GetLastError();
    __declspec(dllimport)
    void *CreateFileW(wchar_t *FileName, u32 Access, u32 ShareMode, void *SecurityAttributes, u32 CreationDisposition, u32 FlagsAndAttributes, void *TemplateFile);
    __declspec(dllimport)
    s32 GetFileSizeEx(void *File, s64 *FileSize);
    __declspec(dllimport)
    u32 GetFullPathNameW(wchar_t *Name, u32 BufferLength, wchar_t *Buffer, wchar_t **FilePart);
    __declspec(dllimport)
    s32 WriteFile(void *File, void *Buffer, u32 BufferLength, u32 *BytesWritten, void *Overlapped);
    __declspec(dllimport)
    s32 ReadFile(void *File, void *Buffer, u32 BytesToRead, u32 *BytesRead, void *Overlapped);
    __declspec(dllimport)
    s32 CloseHandle(void *Object);
    __declspec(dllimport)
    s32 GetFileInformationByHandle(void *File, by_handle_file_information *Information);
    __declspec(dllimport)
    void *FindFirstFileW(wchar_t *Directory, find_data_w *FindData);
    __declspec(dllimport)
    s32 FindNextFileW(void *Handle, find_data_w *FindData);
    __declspec(dllimport)
    s32 FindClose(void *Handle);
    __declspec(dllimport)
    void *FindFirstChangeNotificationW(wchar_t *Path, s32 WatchSubtree, u32 NotifyFilter);
    __declspec(dllimport)
    s32 FindNextChangeNotification(void *Handle);
    __declspec(dllimport)
    s32 FindCloseChangeNotification(void *Handle);
    __declspec(dllimport)
    void *GetActiveWindow();
    __declspec(dllimport)
    s32 GetOpenFileNameW(open_file_name_w *);
    __declspec(dllimport)
    s32 GetSaveFileNameW(open_file_name_w *);
    __declspec(dllimport)
    u32 CommDlgExtendedError();
    __declspec(dllimport)
    s32 SHGetKnownFolderPath(guid *Id, u32 Flags, void *Token, wchar_t **Path);
    __declspec(dllimport)
    s32 CoInitialize(void *Reserved);
    __declspec(dllimport)
    void CoTaskMemFree(void *Pointer);
    __declspec(dllimport)
    s32 CoCreateInstance(void *Clsid, void *Aggregate, u32 Context, void *Id, void *Result);
    __declspec(dllimport)
    s32 TerminateProcess(void *Process, u32 ExitCode);
    __declspec(dllimport)
    void *CreateEventW(security_attributes *Attributes, s32 ManualReset, s32 InitialState, wchar_t *Name);
    __declspec(dllimport)
    s32 ResetEvent(void *Event);
    __declspec(dllimport)
    s32 SetEvent(void *Event);
    __declspec(dllimport)
    void *CreateNamedPipeW(wchar_t *Name, u32 OpenMode, u32 PipeMode, u32 MaxInstances, u32 OutBufferSize, u32 InBufferSize, u32 DefaultTimeout, security_attributes *SecurityAttributes);
    __declspec(dllimport)
    u32 GetCurrentProcessId();
    __declspec(dllimport)
    s32 CancelIo(void *Handle);
    __declspec(dllimport)
    void *CreateJobObjectW(security_attributes *SecurityAttributes, wchar_t *Name);
    __declspec(dllimport)
    s32 AssignProcessToJobObject(void *Job, void *Process);
    __declspec(dllimport)
    s32 SetInformationJobObject(void *Job, s32 InfoClass, void *Info, u32 InfoLength);
    __declspec(dllimport)
    s32 QueryInformationJobObject(void *Job, s32 InfoClass, void *Info, u32 InfoLength, u32 *ReturnLength);
    __declspec(dllimport)
    u32 ResumeThread(void *Thread);
    __declspec(dllimport)
    void *CreateIoCompletionPort(void *FileHandle, void *ExistingCompletionPort, u64 CompletionKey, u32 ConcurrentThreads);
    __declspec(dllimport)
    s32 GetQueuedCompletionStatus(void *CompletionPort, u32 *NumberOfBytes, u64 *CompletionKey, overlapped **Overlapped, u32 Milliseconds);
    __declspec(dllimport)
    void *CreateThread(security_attributes *SecurityAttributes, u64 StackSize, thread_start_routine StartRoutine, void *Parameter, u32 Flags, u32 *ThreadId);
    __declspec(dllimport)
    s32 TerminateJobObject(void *Job, u32 ExitCode);
    __declspec(dllimport)
    s32 GetOverlappedResult(void *File, overlapped *Overlapped, u32 *BytesTransfered, s32 Wait);
    __declspec(dllimport)
    void *OpenProcess(u32 Access, s32 Inherit, u32 ProcessId);
    __declspec(dllimport)
    u32 GetModuleFileNameW(void *Module, wchar_t *Buffer, u32 Size);
    __declspec(dllimport)
    s32 PathIsDirectoryW(wchar_t *Path);
    __declspec(dllimport)
    s32 PathIsRelativeW(wchar_t *Path);
    __declspec(dllimport)
    wchar_t *CharUpperW(wchar_t *);
    __declspec(dllimport)
    wchar_t *CharLowerW(wchar_t *);
    __declspec(dllimport)
    s32 PathCchRemoveFileSpec(wchar_t *Path, u64 Size);
    __declspec(dllimport)
    s32 PathCchCombineEx(wchar_t *PathOut, u64 PathOutSize, wchar_t *Left, wchar_t *Right, u32 Flags);
    __declspec(dllimport)
    s32 PathCchRemoveBackslashEx(wchar_t *Path, u64 PathSize, wchar_t **End, u64 *NewSize);
    __declspec(dllimport)
    s32 PathIsUNCW(wchar_t *Path);
    __declspec(dllimport)
    u64 SetTimer(void *WindowHandle, u64 Id, u32 Milliseconds, void *TimerFunc);
    __declspec(dllimport)
    s32 KillTimer(void *WindowHandle, u64 Id);
    __declspec(dllimport)
    void GetLocalTime(systemtime *);
    __declspec(dllimport)
    s32 CreateDirectoryW(wchar_t *Path, void *SecurityAttributes);
    __declspec(dllimport)
    s32 SystemTimeToTzSpecificLocalTime(void *TimeZoneInfo, systemtime *Universal, systemtime *Local);
    __declspec(dllimport)
    s32 FileTimeToSystemTime(const filetime *In, systemtime *Out);
    __declspec(dllimport)
    void *CreateFontIndirectW(const logfont_w *);
    __declspec(dllimport)
    s32 GetTextMetricsW(void *Hdc, textmetric_w *);
    __declspec(dllimport)
    void *CreateCompatibleDC(void *Hdc);
    __declspec(dllimport)
    void *SelectObject(void *Hdc, void *Object);
    __declspec(dllimport)
    u32 SetTextColor(void *Hdc, u32 Color);
    __declspec(dllimport)
    u32 SetBkColor(void *Hdc, u32 Color);
    __declspec(dllimport)
    s32 GetCharABCWidthsW(void *Hdc, u32 FirstCodepoint, u32 LastCodepoint, abc *);
    __declspec(dllimport)
    s32 TextOutW(void *Hdc, s32 X0, s32 Y0, wchar_t *String, s32 StringLength);
    __declspec(dllimport)
    s32 GetTextExtentPoint32W(void *Hdc, wchar_t *String, s32 StringLength, size *);
    __declspec(dllimport)
    s32 DeleteObject(void *);
    __declspec(dllimport)
    void *CreateDIBSection(void *Hdc, bitmapinfo *Info, u32 Usage, void **Data, void *Section, u32 Offset);
    __declspec(dllimport)
    u32 GetGlyphIndicesW(void *Hdc, wchar_t *Text, s32 Count, u16 *GlyphIndices, u32 Flags);
    __declspec(dllimport)
    s32 ExtTextOutW(void *Hdc, s32 x, s32 y, u32 Options, rectangle *Rectangle, wchar_t *Text, s32 TextLength, s32 *Dx);
    __declspec(dllimport)
    s32 ChooseColorW(choosecolorw *cc);
    __declspec(dllimport)
    void *SetCapture(void *window_handle);
    __declspec(dllimport)
    s32 ReleaseCapture();
    __declspec(dllimport)
    s32 MultiByteToWideChar(u32 CodePage, u32 Flags, char *multibyte, int multibyte_length, wchar_t *wide, int wide_length);
    __declspec(dllimport)
    void *HeapAlloc(void *Heap, u32 Flags, u64 Size);
    __declspec(dllimport)
    s32 HeapFree(void *Heap, u32 Flags, void *Memory);
    __declspec(dllimport)
    void *HeapReAlloc(void *Heap, u32 Flags, void *Memory, u64 Size);
    __declspec(dllimport)
    void *GetProcessHeap();

    unsigned long _exception_code();
};

enum {
    WS_OVERLAPPED   = 0x00000000,
    WS_POPUP        = 0x80000000,
    WS_CHILD        = 0x40000000,
    WS_MINIMIZE     = 0x20000000,
    WS_VISIBLE      = 0x10000000,
    WS_DISABLED     = 0x08000000,
    WS_CLIPSIBLINGS = 0x04000000,
    WS_CLIPCHILDREN = 0x02000000,
    WS_MAXIMIZE     = 0x01000000,
    WS_CAPTION      = 0x00c00000,
    WS_BORDER       = 0x00800000,
    WS_DLGFRAME     = 0x00400000,
    WS_VSCROLL      = 0x00200000,
    WS_HSCROLL      = 0x00100000,
    WS_SYSMENU      = 0x00080000,
    WS_THICKFRAME   = 0x00040000,
    WS_GROUP        = 0x00020000,
    WS_TABSTOP      = 0x00010000,
    WS_MINIMIZEBOX  = 0x00020000,
    WS_MAXIMIZEBOX  = 0x00010000,
    WS_TILED        = WS_OVERLAPPED,
    WS_ICONIC       = WS_MINIMIZE,
    WS_SIZEBOX      = WS_THICKFRAME,
    WS_OVERLAPPEDWINDOW = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX,
    CW_USEDEFAULT = 2147483648,
    CS_VREDRAW = 0x0001,
    CS_HREDRAW = 0x0002,
    SW_SHOW = 5,
    SW_HIDE = 0,
    GWLP_USERDATA = -21,
    GWL_STYLE = -16,
    WM_CREATE = 0x01,
    WM_SIZE = 0x05,
    WM_ACTIVATEAPP = 0x1c,
    WM_CLOSE = 0x010,
    WM_SHOWWINDOW = 0x18,
    WM_KEYDOWN = 0x100,
    WM_SYSKEYDOWN = 0x104,
    WM_KEYUP = 0x101,
    WM_CHAR = 0x102,
    WM_MOUSEWHEEL = 0x020a,
    WM_MOUSEMOVE = 0x0200,
    WM_LBUTTONUP = 0x0202,
    WM_LBUTTONDOWN = 0x0201,
    WM_MBUTTONDOWN = 0x0207,
    WM_MBUTTONUP = 0x0208,
    WM_RBUTTONDOWN = 0x0204,
    WM_RBUTTONUP = 0x0205,
    WM_SETFOCUS = 0x0007,
    WM_KILLFOCUS = 0x0008,
    WM_SETCURSOR = 0x0020,
    WM_GETMINMAXINFO = 0x0024,
    WM_PAINT = 0xf,
    WM_DROPFILES = 0x233,
    WM_TIMER = 0x113,
    WM_APP = 0x8000,
    BI_RGB = 0,
    DIB_RGB_COLORS = 0,
    IDC_ARROW = 32512,
    HTCLIENT = 1,
    MONITOR_DEFAULTTONEAREST = 2,
    HWND_TOP = 0,
    SWP_NOOWNERZORDER = 0x0200,
    SWP_NOMOVE = 0x0002,
    SWP_NOSIZE = 0x0001,
    SWP_NOZORDER = 0x0004,
    SWP_FRAMECHANGED = 0x0020,
    VK_SHIFT = 0x10,
    VK_CONTROL = 0x11,
    VK_MENU = 0x12,
    VK_PRIOR = 0x21,
    VK_NEXT = 0x22,
    VK_END = 0x23,
    VK_HOME = 0x24,
    VK_LEFT = 0x25,
    VK_UP = 0x26,
    VK_RIGHT = 0x27,
    VK_DOWN = 0x28,
    VK_INSERT = 0x2d,
    VK_DELETE = 0x2e,
    VK_RETURN = 0x0d,
    VK_TAB = 0x09,
    VK_BACK = 0x08,
    VK_F1 = 0x70,
    VK_F2 = 0x71,
    VK_F3 = 0x72,
    VK_F4 = 0x73,
    VK_F5 = 0x74,
    VK_F6 = 0x75,
    VK_F7 = 0x76,
    VK_F8 = 0x77,
    VK_F9 = 0x78,
    VK_F10 = 0x79,
    VK_F11 = 0x7A,
    VK_F12 = 0x7B,
    MK_CONTROL = 0x0008,
    MK_LBUTTON = 0x0001,
    MK_MBUTTON = 0x0010,
    MK_RBUTTON = 0x0002,
    MK_SHIFT = 0x0004,
    MK_XBUTTON1 = 0x0020,
    MK_XBUTTON2 = 0x0040,
    CF_BITMAP = 2,
    CF_DIB = 8,
    CF_DIBV5 = 17,
    CF_DIF = 5,
    CF_DSPBITMAP = 0x0082,
    CF_DSPENHMETAFILE = 0x008e,
    CF_DSPMETAFILEPICT = 0x0083,
    CF_DSPTEXT = 0x0081,
    CF_ENHMETAFILE = 14,
    CF_GDIOBJFIRST = 0x0300,
    CF_GDIOBJLAST = 0x03ff,
    CF_HDROP = 15,
    CF_LOCALE = 16,
    CF_METAFILEPICT = 3,
    CF_OEMTEXT = 7,
    CF_OWNERDISPLAY = 0x0080,
    CF_PALETTE = 9,
    CF_PENDATA = 10,
    CF_PRIVATELAST = 0x02ff,
    CF_PRIVATEFIRST = 0x0200,
    CF_RIFF = 11,
    CF_SYLK = 4,
    CF_TEXT = 1,
    CF_TIFF = 6,
    CF_UNICODETEXT = 13,
    CF_WAVE = 12,
    GMEM_FIXED = 0x0000,
    GMEM_MOVEABLE = 0x0002,
    GMEM_ZEROINIT = 0x0040,
    CREATE_BREAKAWAY_FROM_JOB = 0x01000000,
    CREATE_DEFAULT_ERROR_MODE = 0x04000000,
    CREATE_NEW_CONSOLE = 0x00000010,
    CREATE_NEW_PROCESS_GROUP = 0x00000200,
    CREATE_NO_WINDOW = 0x08000000,
    CREATE_PROTECTED_PROCESS = 0x00040000,
    CREATE_PRESERVE_CODE_AUTHZ_LEVEL = 0x02000000,
    CREATE_SEPARATE_WOW_VDM = 0x00000800,
    CREATE_SHARED_WOW_VDM = 0x00001000,
    CREATE_SUSPENDED = 0x00000004,
    CREATE_UNICODE_ENVIRONMENT = 0x00000400,
    DETACHED_PROCESS = 0x00000008,
    EXTENDED_STARTUPINFO_PRESENT = 0x00080000,
    INHERIT_PARENT_AFFINITY = 0x00010000,
    STARTF_FORCEONFEEDBACK = 0x00000040,
    STARTF_FORCEOFFFEEDBACK = 0x00000080,
    STARTF_PREVENTPINNING = 0x00002000,
    STARTF_RUNFULLSCREEN = 0x00000020,
    STARTF_TITLEISAPPID = 0x00001000,
    STARTF_TITLEISLINKNAME = 0x00000800,
    STARTF_USECOUNTCHARS = 0x00000008,
    STARTF_USEFILLATTRIBUTE = 0x00000010,
    STARTF_USEHOTKEY = 0x00000200,
    STARTF_USEPOSITION = 0x00000004,
    STARTF_USESHOWWINDOW = 0x00000001,
    STARTF_USESIZE = 0x00000002,
    STARTF_USESTDHANDLES = 0x00000100,
    STILL_ACTIVE = 259,
    MEM_COMMIT      = 0x00001000,
    MEM_RESERVE     = 0x00002000,
    MEM_RESET       = 0x00080000,
    MEM_RESET_UNDO  = 0x1000000,
    MEM_LARGE_PAGES = 0x20000000,
    MEM_PHYSICAL    = 0x00400000,
    MEM_TOP_DOWN    = 0x00100000,
    MEM_WRITE_WATCH = 0x00200000,
    MEM_COALESCE_PLACEHOLDERS = 0x00000001,
    MEM_PRESERVE_PLACEHOLDER = 0x00000002,
    MEM_DECOMMIT = 0x00004000,
    MEM_RELEASE = 0x00008000,
    PAGE_EXECUTE           = 0x10,
    PAGE_EXECUTE_READ      = 0x20,
    PAGE_EXECUTE_READWRITE = 0x40,
    PAGE_EXECUTE_WRITECOPY = 0x80,
    PAGE_NOACCESS          = 0x01,
    PAGE_READONLY          = 0x02,
    PAGE_READWRITE         = 0x04,
    PAGE_WRITECOPY         = 0x08,
    PAGE_TARGETS_INVALID   = 0x40000000,
    PAGE_TARGETS_NO_UPDATE = 0x40000000,
    PAGE_GUARD             = 0x100,
    PAGE_NOCACHE           = 0x200,
    PAGE_WRITECOMBINE      = 0x400,
    MB_ABORTRETRYIGNORE  = 0x0002,
    MB_CANCELTRYCONTINUE = 0x0006,
    MB_HELP              = 0x4000,
    MB_OK                = 0x0000,
    MB_OKCANCEL          = 0x0001,
    MB_RETRYCANCEL       = 0x0005,
    MB_YESNO             = 0x0004,
    MB_YESNOCANCEL       = 0x0003,
    MB_ICONERROR         = 0x0010,
    MB_ICONQUESTION      = 0x0020,
    MB_ICONWARNING       = 0x0030,
    MB_ICONINFORMATION   = 0x0040,
    WAIT_ABANDONED  = 0x080,
    WAIT_OBJECT_0   = 0x000,
    WAIT_TIMEOUT    = 0x102,
    WAIT_FAILED     = -1,
    IDABORT = 3,
    IDCANCEL = 2,
    IDCONTINUE = 11,
    IDIGNORE = 5,
    IDNO = 7,
    IDOK = 1,
    IDRETRY = 4,
    IDTRYAGAIN = 10,
    IDYES = 6,
    MB_DEFBUTTON1 = 0x000,
    MB_DEFBUTTON2 = 0x100,
    MB_DEFBUTTON3 = 0x200,
    MB_DEFBUTTON4 = 0x300,
    EXCEPTION_EXECUTE_HANDLER = 1,
    EXCEPTION_CONTINUE_SEARCH = 0,
    QS_ALLEVENTS = 0x4bf,
    QS_ALLINPUT = 0x04ff,
    QS_ALLPOSTMESSAGE = 0x0100,
    GENERIC_READ    = 0x80000000,
    GENERIC_WRITE   = 0x40000000,
    GENERIC_EXECUTE = 0x20000000,
    GENERIC_ALL     = 0x10000000,
    FILE_SHARE_READ   = 0x1,
    FILE_SHARE_WRITE  = 0x2,
    FILE_SHARE_DELETE = 0x4,
    CREATE_NEW = 1,
    CREATE_ALWAYS = 2,
    OPEN_EXISTING = 3,
    OPEN_ALWAYS = 4,
    TRUNCATE_EXISTING = 5,
    FILE_ATTRIBUTE_DIRECTORY = 0x10,
    FILE_ATTRIBUTE_NORMAL = 0x80,
    ERROR_INVALID_FUNCTION    = 0x1,
    ERROR_FILE_NOT_FOUND      = 0x2,
    ERROR_PATH_NOT_FOUND      = 0x3,
    ERROR_TOO_MANY_OPEN_FILES = 0x4,
    ERROR_ACCESS_DENIED       = 0x5,
    ERROR_INVALID_HANDLE      = 0x6,
    ERROR_ARENA_TRASHED       = 0x7,
    ERROR_NOT_ENOUGH_MEMORY   = 0x8,
    ERROR_INVALID_BLOCK       = 0x9,
    ERROR_BAD_ENVIRONMENT     = 0xa,
    ERROR_BAD_FORMAT          = 0xb,
    ERROR_INVALID_ACCESS      = 0xc,
    ERROR_INVALID_DATA        = 0xd,
    ERROR_OUTOFMEMORY         = 0xe,
    ERROR_INVALID_DRIVE       = 0xf,
    ERROR_CURRENT_DIRECTORY   = 0x10,
    ERROR_NOT_SAME_DEVICE     = 0x11,
    ERROR_NO_MORE_FILES       = 0x12,
    ERROR_WRITE_PROTECT       = 0x13,
    ERROR_BAD_UNIT            = 0x14,
    ERROR_NOT_READY           = 0x15,
    ERROR_BAD_COMMAND         = 0x16,
    ERROR_CRC                 = 0x17,
    ERROR_BAD_LENGTH          = 0x18,
    ERROR_SEEK                = 0x19,
    ERROR_NOT_DOS_DISK        = 0x1a,
    ERROR_SECTOR_NOT_FOUND    = 0x1b,
    ERROR_OUT_OF_PAPER        = 0x1c,
    ERROR_WRITE_FAULT         = 0x1d,
    ERROR_READ_FAULT          = 0x1e,
    ERROR_GEN_FAILURE         = 0x1f,
    ERROR_SHARING_VIOLATION   = 0x20,
    ERROR_LOCK_VIOLATION      = 0x21,
    ERROR_WRONG_DISK          = 0x22,
    ERROR_FILE_EXISTS         = 0x50,
    ERROR_INSUFFICIENT_BUFFER = 0x7a,
    ERROR_ALREADY_EXISTS      = 0xb7,
    ERROR_BROKEN_PIPE = 109,
    ERROR_IO_PENDING = 997,
    ERROR_CANCELED = 0x800704c7,
    FILE_NOTIFY_CHANGE_FILE_NAME    = 0x001,
    FILE_NOTIFY_CHANGE_DIR_NAME     = 0x002,
    FILE_NOTIFY_CHANGE_ATTRIBUTES   = 0x004,
    FILE_NOTIFY_CHANGE_SIZE         = 0x008,
    FILE_NOTIFY_CHANGE_LAST_WRITE   = 0x010,
    FILE_NOTIFY_CHANGE_SECURITY     = 0x100,
    OFN_ALLOWMULTISELECT = 0x00000200,
    OFN_CREATEPROMPT = 0x00002000,
    OFN_DONTADDTORECENT = 0x02000000,
    OFN_ENABLEHOOK = 0x00000020,
    OFN_ENABLEINCLUDENOTIFY = 0x00400000,
    OFN_ENABLESIZING = 0x00800000,
    OFN_ENABLETEMPLATE = 0x00000040,
    OFN_ENABLETEMPLATEHANDLE = 0x00000080,
    OFN_EXPLORER = 0x00080000,
    OFN_EXTENSIONDIFFERENT = 0x00000400,
    OFN_FILEMUSTEXIST = 0x00001000,
    OFN_FORCESHOWHIDDEN = 0x10000000,
    OFN_HIDEREADONLY = 0x00000004,
    OFN_LONGNAMES = 0x00200000,
    OFN_NOCHANGEDIR = 0x00000008,
    OFN_NODEREFERENCELINKS = 0x00100000,
    OFN_NOLONGNAMES = 0x00040000,
    OFN_NONETWORKBUTTON = 0x00020000,
    OFN_NOREADONLYRETURN = 0x00008000,
    OFN_NOTESTFILECREATE = 0x00010000,
    OFN_NOVALIDATE = 0x00000100,
    OFN_OVERWRITEPROMPT = 0x00000002,
    OFN_PATHMUSTEXIST = 0x00000800,
    OFN_READONLY = 0x00000001,
    OFN_SHAREAWARE = 0x00004000,
    OFN_SHOWHELP = 0x00000010,
    FOS_NOCHANGEDIR              = 0x00000008,
    FOS_PICKFOLDERS              = 0x00000020,
    FOS_FORCEFILESYSTEM          = 0x00000040,
    FOS_ALLNONSTORAGEITEMS       = 0x00000080,
    FOS_NOVALIDATE               = 0x00000100,
    FOS_ALLOWMULTISELECT         = 0x00000200,
    FOS_PATHMUSTEXIST            = 0x00000800,
    FOS_FILEMUSTEXIST            = 0x00001000,
    FOS_CREATEPROMPT             = 0x00002000,
    FOS_SHAREAWARE               = 0x00004000,
    FOS_NOREADONLYRETURN         = 0x00008000,
    FOS_NOTESTFILECREATE         = 0x00010000,
    PM_NOREMOVE = 0,
    PM_REMOVE = 1,
    PIPE_ACCESS_DUPLEX = 3,
    PIPE_ACCESS_INBOUND = 1,
    PIPE_ACCESS_OUTBOUND = 2,
    FILE_FLAG_FIRST_PIPE_INSTANCE = 0x00080000,
    FILE_FLAG_WRITE_THROUGH = 0x80000000,
    FILE_FLAG_OVERLAPPED = 0x40000000,
    JobObjectBasicLimitInformation = 2,
    JobObjectBasicProcessIdList = 3,
    JobObjectExtendedLimitInformation = 9,
    JobObjectAssociateCompletionPortInformation = 7,
    JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE = 0x00002000,
    JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION = 0x00000400,
    JOB_OBJECT_LIMIT_BREAKAWAY_OK = 0x00000800,
    JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK = 0x00001000,
    JOB_OBJECT_MSG_END_OF_JOB_TIME          = 1,
    JOB_OBJECT_MSG_END_OF_PROCESS_TIME      = 2,
    JOB_OBJECT_MSG_ACTIVE_PROCESS_LIMIT     = 3,
    JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO      = 4,
    JOB_OBJECT_MSG_NEW_PROCESS              = 6,
    JOB_OBJECT_MSG_EXIT_PROCESS             = 7,
    JOB_OBJECT_MSG_ABNORMAL_EXIT_PROCESS    = 8,
    JOB_OBJECT_MSG_PROCESS_MEMORY_LIMIT     = 9,
    JOB_OBJECT_MSG_JOB_MEMORY_LIMIT         = 10,
    JOB_OBJECT_MSG_NOTIFICATION_LIMIT       = 11,
    JOB_OBJECT_MSG_JOB_CYCLE_TIME_LIMIT     = 12,
    JOB_OBJECT_MSG_SILO_TERMINATED          = 13,
    PROCESS_CREATE_PROCESS = 0x0080,
    PROCESS_CREATE_THREAD = 0x0002,
    PROCESS_DUP_HANDLE = 0x0040,
    PROCESS_QUERY_INFORMATION = 0x0400,
    PROCESS_QUERY_LIMITED_INFORMATION = 0x1000,
    PROCESS_SET_INFORMATION = 0x0200,
    PROCESS_SET_QUOTA = 0x0100,
    PROCESS_SUSPEND_RESUME = 0x0800,
    PROCESS_TERMINATE = 0x0001,
    PROCESS_VM_OPERATION = 0x0008,
    PROCESS_VM_READ = 0x0010,
    PROCESS_VM_WRITE = 0x0020,
    PATHCCH_ALLOW_LONG_PATHS = 1,
    DEFAULT_QUALITY           = 0,
    DRAFT_QUALITY             = 1,
    PROOF_QUALITY             = 2,
    NONANTIALIASED_QUALITY    = 3,
    ANTIALIASED_QUALITY       = 4,
    CLEARTYPE_QUALITY         = 5,
    CLEARTYPE_NATURAL_QUALITY = 6,
    ANSI_CHARSET = 0,
    DEFAULT_CHARSET = 1,
    TRANSPARENT = 1,
    OPAQUE = 2,
    SRCCOPY     = 0x00CC0020,
    SRCPAINT    = 0x00EE0086,
    SRCAND      = 0x008800C6,
    SRCINVERT   = 0x00660046,
    SRCERASE    = 0x00440328,
    NOTSRCCOPY  = 0x00330008,
    NOTSRCERASE = 0x001100A6,
    MERGECOPY   = 0x00C000CA,
    MERGEPAINT  = 0x00BB0226,
    PATCOPY     = 0x00F00021,
    PATPAINT    = 0x00FB0A09,
    PATINVERT   = 0x005A0049,
    DSTINVERT   = 0x00550009,
    BLACKNESS   = 0x00000042,
    WHITENESS   = 0x00FF0062,
    FW_DONTCARE = 0,
    FW_THIN = 100,
    FW_EXTRALIGHT = 200,
    FW_ULTRALIGHT = 200,
    FW_LIGHT = 300,
    FW_NORMAL = 400,
    FW_REGULAR = 400,
    FW_MEDIUM = 500,
    FW_SEMIBOLD = 600,
    FW_DEMIBOLD = 600,
    FW_BOLD = 700,
    FW_EXTRABOLD = 800,
    FW_ULTRABOLD = 800,
    FW_HEAVY = 900,
    FW_BLACK = 900,
    DIB_PAL_COLORS = 1,
    BI_RLE8       = 1,
    BI_RLE4       = 2,
    BI_BITFIELDS  = 3,
    BI_JPEG       = 4,
    BI_PNG        = 5,
    GGI_MARK_NONEXISTING_GLYPHS = 1,
    ETO_GLYPH_INDEX = 16,
    CC_ANYCOLOR = 0x00000100,
    CC_ENABLEHOOK = 0x00000010,
    CC_ENABLETEMPLATE = 0x00000020,
    CC_ENABLETEMPLATEHANDLE = 0x00000040,
    CC_FULLOPEN = 0x00000002,
    CC_PREVENTFULLOPEN = 0x00000004,
    CC_RGBINIT = 0x00000001,
    CC_SHOWHELP = 0x00000008,
    CC_SOLIDCOLOR = 0x00000080,
    CP_UTF8 = 65001,
};

enum: u32 {
    STATUS_WAIT_0                       = 0x00000000,
    STATUS_ABANDONED_WAIT_0             = 0x00000080,
    STATUS_USER_APC                     = 0x000000C0,
    STATUS_TIMEOUT                      = 0x00000102,
    STATUS_PENDING                      = 0x00000103,
    DBG_EXCEPTION_HANDLED               = 0x00010001,
    DBG_CONTINUE                        = 0x00010002,
    STATUS_SEGMENT_NOTIFICATION         = 0x40000005,
    STATUS_FATAL_APP_EXIT               = 0x40000015,
    DBG_REPLY_LATER                     = 0x40010001,
    DBG_TERMINATE_THREAD                = 0x40010003,
    DBG_TERMINATE_PROCESS               = 0x40010004,
    DBG_CONTROL_C                       = 0x40010005,
    DBG_PRINTEXCEPTION_C                = 0x40010006,
    DBG_RIPEXCEPTION                    = 0x40010007,
    DBG_CONTROL_BREAK                   = 0x40010008,
    DBG_COMMAND_EXCEPTION               = 0x40010009,
    DBG_PRINTEXCEPTION_WIDE_C           = 0x4001000A,
    STATUS_GUARD_PAGE_VIOLATION         = 0x80000001,
    STATUS_DATATYPE_MISALIGNMENT        = 0x80000002,
    STATUS_BREAKPOINT                   = 0x80000003,
    STATUS_SINGLE_STEP                  = 0x80000004,
    STATUS_LONGJUMP                     = 0x80000026,
    STATUS_UNWIND_CONSOLIDATE           = 0x80000029,
    DBG_EXCEPTION_NOT_HANDLED           = 0x80010001,
    STATUS_ACCESS_VIOLATION             = 0xC0000005,
    STATUS_IN_PAGE_ERROR                = 0xC0000006,
    STATUS_INVALID_HANDLE               = 0xC0000008,
    STATUS_INVALID_PARAMETER            = 0xC000000D,
    STATUS_NO_MEMORY                    = 0xC0000017,
    STATUS_ILLEGAL_INSTRUCTION          = 0xC000001D,
    STATUS_NONCONTINUABLE_EXCEPTION     = 0xC0000025,
    STATUS_INVALID_DISPOSITION          = 0xC0000026,
    STATUS_ARRAY_BOUNDS_EXCEEDED        = 0xC000008C,
    STATUS_FLOAT_DENORMAL_OPERAND       = 0xC000008D,
    STATUS_FLOAT_DIVIDE_BY_ZERO         = 0xC000008E,
    STATUS_FLOAT_INEXACT_RESULT         = 0xC000008F,
    STATUS_FLOAT_INVALID_OPERATION      = 0xC0000090,
    STATUS_FLOAT_OVERFLOW               = 0xC0000091,
    STATUS_FLOAT_STACK_CHECK            = 0xC0000092,
    STATUS_FLOAT_UNDERFLOW              = 0xC0000093,
    STATUS_INTEGER_DIVIDE_BY_ZERO       = 0xC0000094,
    STATUS_INTEGER_OVERFLOW             = 0xC0000095,
    STATUS_PRIVILEGED_INSTRUCTION       = 0xC0000096,
    STATUS_STACK_OVERFLOW               = 0xC00000FD,
    STATUS_DLL_NOT_FOUND                = 0xC0000135,
    STATUS_ORDINAL_NOT_FOUND            = 0xC0000138,
    STATUS_ENTRYPOINT_NOT_FOUND         = 0xC0000139,
    STATUS_CONTROL_C_EXIT               = 0xC000013A,
    STATUS_DLL_INIT_FAILED              = 0xC0000142,
    STATUS_PIPE_BROKEN                  = 0xC000014B,
    STATUS_FLOAT_MULTIPLE_FAULTS        = 0xC00002B4,
    STATUS_FLOAT_MULTIPLE_TRAPS         = 0xC00002B5,
    STATUS_REG_NAT_CONSUMPTION          = 0xC00002C9,
    STATUS_HEAP_CORRUPTION              = 0xC0000374,
    STATUS_STACK_BUFFER_OVERRUN         = 0xC0000409,
    STATUS_INVALID_CRUNTIME_PARAMETER   = 0xC0000417,
    STATUS_ASSERTION_FAILURE            = 0xC0000420,
    STATUS_ENCLAVE_VIOLATION            = 0xC00004A2,
    STATUS_FATAL_USER_CALLBACK_EXCEPTION = 0xC000041D,
};

char *_exception_message(u32 Code)
{
    char *Message;
    switch (Code) {
        case win32::STATUS_ACCESS_VIOLATION:             Message = "Access violation"; break;
        case win32::STATUS_IN_PAGE_ERROR:                Message = "In-page error"; break;
        case win32::STATUS_INVALID_HANDLE:               Message = "Invalid handle"; break;
        case win32::STATUS_INVALID_PARAMETER:            Message = "Invalid parameter"; break;
        case win32::STATUS_NO_MEMORY:                    Message = "No memory"; break;
        case win32::STATUS_ILLEGAL_INSTRUCTION:          Message = "Illegal instruction"; break;
        case win32::STATUS_NONCONTINUABLE_EXCEPTION:     Message = "Noncontinuable exception"; break;
        case win32::STATUS_INVALID_DISPOSITION:          Message = "Invalid disposition"; break;
        case win32::STATUS_ARRAY_BOUNDS_EXCEEDED:        Message = "Array bounds exceeded"; break;
        case win32::STATUS_FLOAT_DENORMAL_OPERAND:       Message = "Float denormal operand"; break;
        case win32::STATUS_FLOAT_DIVIDE_BY_ZERO:         Message = "Float divide by zero"; break;
        case win32::STATUS_FLOAT_INEXACT_RESULT:         Message = "Float inexact result"; break;
        case win32::STATUS_FLOAT_INVALID_OPERATION:      Message = "Float invalid operation"; break;
        case win32::STATUS_FLOAT_OVERFLOW:               Message = "Float overflow"; break;
        case win32::STATUS_FLOAT_STACK_CHECK:            Message = "Float stack check"; break;
        case win32::STATUS_FLOAT_UNDERFLOW:              Message = "Float underflow"; break;
        case win32::STATUS_INTEGER_DIVIDE_BY_ZERO:       Message = "Integer divide by zero"; break;
        case win32::STATUS_INTEGER_OVERFLOW:             Message = "Integer overflow"; break;
        case win32::STATUS_PRIVILEGED_INSTRUCTION:       Message = "Privileged instruction"; break;
        case win32::STATUS_STACK_OVERFLOW:               Message = "Stack overflow"; break;
        case win32::STATUS_DLL_NOT_FOUND:                Message = "Dll not found"; break;
        case win32::STATUS_ORDINAL_NOT_FOUND:            Message = "Ordinal not found"; break;
        case win32::STATUS_ENTRYPOINT_NOT_FOUND:         Message = "Entrypoint not found"; break;
        case win32::STATUS_CONTROL_C_EXIT:               Message = "Control c exit"; break;
        case win32::STATUS_DLL_INIT_FAILED:              Message = "Dll init failed"; break;
        case win32::STATUS_FLOAT_MULTIPLE_FAULTS:        Message = "Float multiple faults"; break;
        case win32::STATUS_FLOAT_MULTIPLE_TRAPS:         Message = "Float multiple traps"; break;
        case win32::STATUS_REG_NAT_CONSUMPTION:          Message = "Reg nat consumption"; break;
        case win32::STATUS_HEAP_CORRUPTION:              Message = "Heap corruption"; break;
        case win32::STATUS_STACK_BUFFER_OVERRUN:         Message = "Stack buffer overrun"; break;
        case win32::STATUS_INVALID_CRUNTIME_PARAMETER:   Message = "Invalid c runtime parameter"; break;
        case win32::STATUS_ASSERTION_FAILURE:            Message = "Assertion failure"; break;
        case win32::STATUS_ENCLAVE_VIOLATION:            Message = "Enclave violation"; break;
        default:                                         Message = "Unknown error"; break;
    }
    return(Message);
}

str filetime_to_localtime_string(u64 fake_filetime)
{
    filetime real_filetime;
    real_filetime.Low = (u32) (fake_filetime & 0xffffffffull);
    real_filetime.High = (u32) ((fake_filetime >> 32ull) & 0xffffffffull);
    systemtime global, local;
    FileTimeToSystemTime(&real_filetime, &global);
    SystemTimeToTzSpecificLocalTime(null, &global, &local);
    return(stack_printf("%04i-%02i-%02i %02i:%02i %02i.%04i", local.Year, local.Month, local.Day, local.Hour, local.Minute, local.Second, local.Milliseconds));
}

};