#!/bin/bash

#---------------------------------------------------------------------------
# Build notes
# How to use:
#
# [Using MSVC]: First find vcvars64 or vcvars.bat in your Visual Studio path
# and run the program then call this script with the arguments listed below
#
# build.sh msvc debug
# build.sh msvc release
# build.sh clang debug
# build.sh clang release
#---------------------------------------------------------------------------

#---------------------------------------------------------------------------
# Parse and Unpack Arguments
#---------------------------------------------------------------------------
msvc=0
clang=0
debug=0
release=0

echo "------------------------------------------------------"
echo "Building arguments:"

case "$1" in
  msvc)   msvc=1;  echo "[MSVC compiler selected]" ;;
  clang)  clang=1; echo "[Clang compiler selected]" ;;
  release) msvc=1; echo "No compiler specified: Using MSVC with release mode" ;;
  "")     msvc=1;  echo "No compiler specified: Using MSVC" ;;
esac

case "$2" in
  debug)   debug=1;   echo "-Debug mode" ;;
  release) release=1; echo "-Release mode" ;;
  "")      debug=1;   echo "No build configuration specified: Using debug mode" ;;
esac

if [ "$msvc" -eq 1 ];    then clang=0; fi
if [ "$debug" -eq 1 ];   then release=0; fi

echo "------------------------------------------------------"
#
OS=$(uname -s)

#---------------------------------------------------------------------------
# External Dependencies
#---------------------------------------------------------------------------
SDL2_DIR="extern/SDL2"
EXTERN_DIR="extern/"
IMGUI_DIR="extern/imgui"

LIBS="-L\"${SDL2_DIR}/lib/x64\" -lSDL2 -lSDL2main -lopengl32 -lshell32"

#---------------------------------------------------------------------------
# Compiler and Linker Flags
#---------------------------------------------------------------------------
VC_COMPILER_FLAGS="/nologo /EHsc /Zi /MD /utf-8 /std:c++20"
VC_LINKER_FLAGS="/link ${LIBS} /incremental:no /opt:ref /subsystem:console"

CLANG_COMPILER_FLAGS="-fexceptions -g --std=c++20 -finput-charset=UTF-8 -DSDL_MAIN_HANDLED"
# CLANG_LINKER_FLAGS="${CLANG_LIBS} -Wl,/opt:ref -Wl,/subsystem:console"

if [ "$OS" = "Darwin" ]; then
  echo Darwin Architecture
  SDL2_CFLAGS=$(sdl2-config --cflags)
  SDL2_LIBS=$(sdl2-config --libs)
  CLANG_LIBS="-lSDL2 -lSDL2main -framework OpenGL"
  CLANG_INCLUDES="-I${EXTERN_DIR} -I${IMGUI_DIR} -I${IMGUI_DIR}/backends ${SDL2_CFLAGS}"
  CLANG_LINKER_FLAGS="${SDL2_LIBS}"
else
  CLANG_LIBS="-L\"${SDL2_DIR}/lib/x64\" -lSDL2 -lSDL2main -lopengl32 -lshell32"
  CLANG_INCLUDES="-I${EXTERN_DIR} -I${IMGUI_DIR} -I${IMGUI_DIR}/backends -I${SDL2_DIR}/include"
  CLANG_LINKER_FLAGS="${CLANG_LIBS} -Wl,/opt:ref -Wl,/subsystem:console"
fi

#---------------------------------------------------------------------------
# Sources and Includes
#---------------------------------------------------------------------------
OUT_DIR="Debug"
OUT_EXE="Mikustation-2"

VC_INCLUDES="/I${EXTERN_DIR} /I${IMGUI_DIR} /I${IMGUI_DIR}/backends /I${SDL2_DIR}/include"
# CLANG_INCLUDES="-I${EXTERN_DIR} -I${IMGUI_DIR} -I${IMGUI_DIR}/backends -I${SDL2_DIR}/include"

SOURCES="${IMGUI_DIR}/imgui.cpp src/ps2.cpp ${EXTERN_DIR}/glad.c"

mkdir -p "$OUT_DIR"

#---------------------------------------------------------------------------
# Compiler selection
#---------------------------------------------------------------------------
CLANG_DEBUG="clang++ -g -O0 ${CLANG_COMPILER_FLAGS}"
VC_DEBUG="cl /Od /Ob1 ${VC_COMPILER_FLAGS}"

if [ "$msvc" -eq 1 ]; then
  COMPILE="$VC_DEBUG"
  COMPILE_LINK="$VC_LINKER_FLAGS"
  COMPILE_INCLUDES="$VC_INCLUDES"
  OUT="/Fe${OUT_DIR}/${OUT_EXE}.exe /Fo${OUT_DIR}/"
fi

if [ "$clang" -eq 1 ]; then
  COMPILE="$CLANG_DEBUG"
  COMPILE_LINK="$CLANG_LINKER_FLAGS"
  COMPILE_INCLUDES="$CLANG_INCLUDES"
  OUT="-o ${OUT_DIR}/${OUT_EXE}.exe"
fi

#---------------------------------------------------------------------------
# Compile
#---------------------------------------------------------------------------
clang++ $CLANG_COMPILER_FLAGS $CLANG_INCLUDES $SOURCES -o "${OUT_DIR}/${OUT_EXE}.exe" $CLANG_LINKER_FLAGS
./Debug/Mikustation-2.exe