@echo off

set optimize=0
set debug=1
set release=0
set use_clang=0

set font_backend=gdi

:parse_flags
shift
set flag=%0
if defined flag (
    if "%flag%"=="optimize" (
        set optimize=1
        goto parse_flags
    )
    if "%flag%"=="nodebug" (
        set debug=0
        goto parse_flags
    )
    if "%flag%"=="release" (
        set release=1
        goto parse_flags
    )
    if "%flag%"=="clang" (
        set use_clang=1
        goto parse_flags
    )

    echo Unknown command line option: "%flag%"
    exit /b 1
)

if not exist out mkdir out
REM Msvc creates an invalid pdb if there already is a pdb in the folder. Nice
if exist out\am7.pdb del out\am7.pdb


if not exist out\icon.ico ( call icon\build.bat )

if /I %optimize%==1 ( set optimize_define="OPTIMIZE" ) else ( set optimize_define="NOOPTIMIZE" )
if /I %debug%==1 ( set debug_define="DEBUG" ) else ( set debug_define="NODEBUG" )

if /I %optimize%==0 ( set subsystem=console ) else ( set subsystem=windows )

set defines=-D%debug_define% -D%optimize_define% -D"WINDOWS" -Dsubsystem_%subsystem% -DFONT_BACKEND_%font_backend%

if /I %release%==0 (
    set executable=out\am7.exe
) else (
    set executable=C:\Dev\tools\am7.exe
)
echo Writing executable to %executable%

..\ctime\ctime -begin timings.ctm

if /I %use_clang%==1 ( goto build_clang ) else ( goto build_msvc )

:build_msvc
echo Building with msvc

if /I %optimize%==1 ( set optimize_options=/O2 ) else ( set optimize_options=/Od )
if /I %debug%==1 ( set compile_debug_options=/Zi ) else ( set compile_debug_options= )
if /I %debug%==1 ( set link_debug_options=/debug ) else ( set link_debug_options= )

cl /nologo /c /J /utf-8 /GS- /WX /EHa- /Zl /WL /D"_NO_CRT_STDIO_INLINE" ^
    %optimize_options% %compile_debug_options% %defines%  ^
    /Fd"out\am7_obj.pdb" /Fo"out\am7.obj" ^
    source\backend_win32.cpp
set err=%errorlevel%
if %err% neq 0 goto after_build
link /nologo %link_debug_options% /incremental:no ^
    /subsystem:%subsystem%^
    /out:"%executable%"^
    /entry:main^
    out\am7.obj out\icon.res misc\msvcrt.lib
set err=%errorlevel%
if %err% neq 0 goto after_build

goto after_build

:build_clang
echo Building with clang

if /I %optimize%==1 ( set optimize_options=-O2 ) else ( set optimize_options=-O0 )
if /I %debug%==1 ( set debug_options=-Wl,-debug -g -gcodeview ) else ( set debug_options= )

clang ^
    source\backend_win32.cpp ^
    --output "%executable%" ^
    %defines% %optimize_options% %debug_options% -Wl,-subsystem:%subsystem% ^
    -Wall -Werror -Wno-writable-strings -Wno-multichar -maes -mssse3 -Wl,-WX ^
    out\icon.res ^
    -D_NO_CRT_STDIO_INLINE -Wl,-entry:main -nostdlib misc\msvcrt.lib ^
    -fuse-ld=lld
set err=%errorlevel%
goto after_build

:after_build

..\ctime\ctime -end timings.ctm %err%

if %err% neq 0 (
    echo Build failed
    exit /b %err%
)

if /I %release%==0 ( out\am7 )
if %errorlevel% neq 0 exit /b %errorlevel%