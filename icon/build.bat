@echo off

set dir=%~dp0..\

cl /nologo /WX /EHa- /Fd"%dir%out\\" /Fo"%dir%out\\" /Fe"%dir%out\\" %dir%icon\icon_tool.c
if %errorlevel% neq 0 exit /b %errorlevel%

%dir%out\icon_tool.exe %dir%out\icon.ico %dir%out\icon.res %dir%icon\icon.svg