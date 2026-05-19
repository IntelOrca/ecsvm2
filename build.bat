@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=amd64
if not defined ECSVM_ENABLE_SDL3 (
    if exist dependencies\include\SDL3\SDL.h if exist dependencies\lib\x64\SDL3.lib set ECSVM_ENABLE_SDL3=1
)
if not defined ECSVM_ENABLE_SDL3 set ECSVM_ENABLE_SDL3=0
nmake /f Makefile.win clean
nmake /f Makefile.win ECSVM_ENABLE_SDL3=%ECSVM_ENABLE_SDL3%
