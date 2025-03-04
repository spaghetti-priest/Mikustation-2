
@echo off
setlocal

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
if "%1"=="msvc"  	set msvc=1 		&& echo -MSVC
if "%1"=="clang" 	set clang=1 	&& echo -Clang
if "%2"=="debug" 	set debug=1 	&& echo -Debug mode
if "%2"=="release" 	set release=1 	&& echo -Release mode
if "%2"=="" 		set release=1 	&& echo No build Configuration specified: Using debug mode

if "%1"=="" 		set msvc=1 		&& echo No compiler specified: Using MSVC
if "%1"=="release" 	set msvc=1 		&& echo No compiler specified: Using MSVC with release mode

if "%msvc%"=="1" 	set clang=0
if "%debug%"=="1" 	set release=0 

echo ------------------------------------------------------

::---------------------------------------------------------------------------
:: External Dependencies
::---------------------------------------------------------------------------
set SDL2_DIR=extern\SDL2
set EXTERN_DIR=extern
set LIBS=/LIBPATH:"%SDL2_DIR%\\lib\\x64" SDL2.lib SDL2main.lib opengl32.lib shell32.lib

::---------------------------------------------------------------------------
:: Compiler and Linker Flags
::---------------------------------------------------------------------------
set VC_COMPILER_FLAGS=  	/nologo /EHsc /Zi /MD /utf-8 /std:c++20
set VC_LINKER_FLAGS=    	%LIBS% /incremental:no /opt:ref /subsystem:console

set CLANG_COMPILER_FLAGS=  /nologo /EHsc /Zi /MD /utf-8 /std:c++20
set CLANG_LINKER_FLAGS=    %LIBS% /incremental:no /opt:ref /subsystem:console

::---------------------------------------------------------------------------
:: Sources and Includes
::---------------------------------------------------------------------------
rem set INCLUDES= /I%EXTERN_DIR% /I%SDL2_DIR%\\include /I%FMT_DIR%\\include\\fmt 
set OUT_DIR=Debug
set OUT_EXE=Mikustation-2

set INCLUDES= /I%EXTERN_DIR% 
set SOURCES=  src\\ps2.cpp %EXTERN_DIR%\\glad.c

if not exist %OUT_DIR% mkdir %OUT_DIR%

::---------------------------------------------------------------------------
:: Compile
::---------------------------------------------------------------------------
call cl %VC_COMPILER_FLAGS% %INCLUDES% %SOURCES% /Fe%OUT_DIR%/%OUT_EXE%.exe /Fo%OUT_DIR%/ /link %VC_LINKER_FLAGS%
rem call clang++ %SOURCES% -o %OUT_DIR%/%OUT_EXE%.exe