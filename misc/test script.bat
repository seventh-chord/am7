@echo off
echo nice, dude!

ping 127.0.0.1 -n 2 >nul

echo The minstrell in the gallery

set dir=%~dp0
echo The script is in %dir% 

dir

ping 127.0.0.1 -n 1

echo R:\am7\todo.txt(2): lol
echo source\main.cpp(3): bro
echo source\main.cpp(4) nice
echo source\main.cpp( ): nice
echo source\main.cpp(4):  hehe
echo source\main.cpp(4):  bruder
echo R:\source\main.cpp(4):  heh

echo source\main.cpp(4) :  bruder
echo source\main.cpp(4, 7):  bruder

echo foo\bar.cpp:3:3: nice
echo foo\bar.cpp:3: dude

echo Goodbye!
exit /b 32