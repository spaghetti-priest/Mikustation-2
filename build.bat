
@echo off
setlocal enabledelayedexpansion


::---------------------------------------------------------------------------
:: Build notes
:: How to use:
::
:: [Using MSVC]: First find vcvars64 or vcvars.bat in your Visual studio path
:: and run the program then call this batch file with the arguments listed
:: below
::
:: build msvc debug
:: build msvc release
:: build clang debug
:: build clang release
::---------------------------------------------------------------------------

::---------------------------------------------------------------------------
:: Parse and Unpack Arguments
::---------------------------------------------------------------------------
rem Ask if clang or msvc
echo ------------------------------------------------------
echo Building arguments:
if "%1"=="msvc"  	   set msvc=1 		&& echo [MSVC compiler selected]
if "%1"=="clang" 	   set clang=1 	&& echo [Clang compiler selected]
if "%2"=="debug" 	   set debug=1 	&& echo -Debug mode
if "%2"=="release"   set release=1 	&& echo -Release mode
if "%2"=="" 		   set release=1 	&& echo No build Configuration specified: Using debug mode

rem if "%1"=="" 		   set msvc=1 		&& echo No compiler specified: Using Clang
if "%1"=="" 		   set clang=1 		&& echo No compiler specified: Using Clang
if "%1"=="release" 	set msvc=1 		&& echo No compiler specified: Using MSVC with release mode

if "%msvc%"=="1" 	   set clang=0 
if "%debug%"=="1" 	set release=0

echo ------------------------------------------------------

::---------------------------------------------------------------------------
:: External Dependencies
::---------------------------------------------------------------------------
set SDL2_DIR=extern\SDL2
set EXTERN_DIR=extern\
set IMGUI_DIR=extern\\imgui

set LIBS=/LIBPATH:"%SDL2_DIR%\\lib\\x64" SDL2.lib SDL2main.lib opengl32.lib shell32.lib
set CLANG_LIBS=-L"%SDL2_DIR%\\lib\\x64" -lSDL2 -lSDL2main -lopengl32 -lshell32
::---------------------------------------------------------------------------
:: Compiler and Linker Flags
::---------------------------------------------------------------------------
set VC_COMPILER_FLAGS=/nologo /EHsc /Zi /MD /utf-8 /std:c++20
set VC_LINKER_FLAGS=/link %LIBS% /incremental:no /opt:ref /subsystem:console

set CLANG_COMPILER_FLAGS=-fexceptions -g --std=c++20 -finput-charset=UTF-8 -Wno-deprecated
set CLANG_LINKER_FLAGS=%CLANG_LIBS% -Wl,/opt:ref -Wl,/subsystem:console

::---------------------------------------------------------------------------
:: Sources and Includes
::---------------------------------------------------------------------------
rem set INCLUDES= /I%EXTERN_DIR% /I%SDL2_DIR%\\include /I%FMT_DIR%\\include\\fmt
set OUT_DIR=Debug
set OUT_EXE=Mikustation-2

set VC_INCLUDES= /I%EXTERN_DIR% /I%IMGUI_DIR% /I%IMGUI_DIR%\\backends /I%SDL2_DIR%\\include
set CLANG_INCLUDES=-I%EXTERN_DIR% -I%IMGUI_DIR% -I%IMGUI_DIR%\\backends -I%SDL2_DIR%\\include

set SOURCES= %IMGUI_DIR%\\imgui.cpp src\\ps2.cpp %EXTERN_DIR%\\glad.c

if not exist %OUT_DIR% mkdir %OUT_DIR%

::---------------------------------------------------------------------------
:: Compiler selection
::---------------------------------------------------------------------------
set clang_debug=call clang++ -g -O0 %CLANG_COMPILER_FLAGS%
set vc_debug=call cl /Od /Ob1 %VC_COMPILER_FLAGS%
if "%msvc%" =="1"  set COMPILE=%vc_debug% 
if "%msvc%" =="1"  set compile_link=%VC_LINKER_FLAGS%
if "%msvc%" =="1"  set COMPILE_INCLUDES=%VC_INCLUDES%
if "%msvc%" == "1" set out=/Fe%OUT_DIR%/%OUT_EXE%.exe /Fo%OUT_DIR%/

if "%clang%" =="1" set COMPILE=%clang_debug%
if "%clang%" =="1" set compile_link=%CLANG_LINKER_FLAGS%
if "%clang%" =="1" set COMPILE_INCLUDES=%CLANG_INCLUDES%
if "%clang%" == "1" set out=-o %OUT_DIR%/%OUT_EXE%.exe 
::---------------------------------------------------------------------------
:: Compile
::---------------------------------------------------------------------------
:: FUck this if else statement man it just defaults to msvc for now
rem cl %VC_COMPILER_FLAGS% %VC_INCLUDES% %SOURCES% /Fe%OUT_DIR%/%OUT_EXE%.exe /Fo%OUT_DIR%/ %VC_LINKER_FLAGS%
clang++ %CLANG_COMPILER_FLAGS% %CLANG_INCLUDES% %SOURCES% -o %OUT_DIR%/%OUT_EXE%.exe %CLANG_LINKER_FLAGS%
