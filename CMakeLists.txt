cmake_minimum_required(VERSION 3.10)
project(Renderer LANGUAGES CXX)

# ==============================================================================
# Headers and sources
# ==============================================================================

set(HEADERS
    src/ee/cop0.h
    src/ee/cop01.h
    src/ee/r5900Interpreter.h
    src/ee/timer.h
    src/ee/ee_inc.h
    src/iop/iop.h
    src/iop/iop_dmac.h
    src/iop/iop_timer.h
    src/ee/iop_inc.h
    src/gs/gl.h
    src/gs/gs.h
    src/ee/gs_inc.h
    src/common.h
    src/dmac.h
    src/gif.h
    src/intc.h
    src/ipu.h
    src/kernel.h
    src/loader.h
    src/ps2.h
    src/ps2types.h
    src/sif.h
    src/vif.h
    src/vu.h
)

set(SOURCES
    src/ee/cop0.cpp
    src/ee/cop1.cpp
    src/ee/r5900Interpreter.cpp
    src/ee/timer.cpp
    src/ee/ee_inc.cpp
    src/iop/iop.cpp
    src/iop/iop_dmac.cpp
    src/iop/iop_timer.cpp
    src/iop/iop_inc.cpp
    src/gs/gl.cpp
    src/gs/gs.cpp
    src/gs/gs_inc.cpp
    src/common.cpp
    src/dmac.cpp
    src/gif.cpp
    src/intc.cpp
    src/ipu.cpp
    src/kernel.cpp
    src/loader.cpp
    src/sif.cpp
    src/vif.cpp
    src/vu.cpp
)

if(WIN32 OR APPLE OR UNIX)
    set(SOURCES ${SOURCES} src/ps2.cpp)
#endif()

# ==============================================================================
# Target definition
# ==============================================================================

set(TARGET Mikustation-2)
add_executable(${TARGET} ${HEADERS} ${SOURCES})

# ==============================================================================
# Target properties
# ==============================================================================

set_target_properties(${TARGET} PROPERTIES INTERPROCEDURAL_OPTIMIZATION ON)

# ==============================================================================
# Compile options
# ==============================================================================

if(MSVC)
    target_compile_options(${TARGET} PRIVATE /W4 /D_CRT_SECURE_NO_WARNINGS)
    target_compile_options(${TARGET} PRIVATE /fp:fast)
else()
    target_compile_options(${TARGET} PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(${TARGET} PRIVATE -ffast-math)
endif()

if(UNIX AND NOT APPLE)
    target_compile_options(${TARGET} PRIVATE -D_POSIX_C_SOURCE=200809L)
endif()

# ==============================================================================
# Link libraries
# ==============================================================================
find_library(SDL_LIB foo)
find_library(FMT_LIB foo)
# opengl lib?
find_library(FMT_LIB foo)

target_link_libraries(${TARGET} PUBLIC )

if(WIN32)
    # nothing to do for now
elseif(APPLE)
    target_link_libraries(${TARGET} PRIVATE "-framework Cocoa")
else()
    target_link_libraries(${TARGET} PRIVATE m X11)
endif()

# ==============================================================================
# IDE support
# ==============================================================================

set_directory_properties(PROPERTIES VS_STARTUP_PROJECT ${TARGET})
source_group(TREE "${CMAKE_SOURCE_DIR}/renderer" FILES ${HEADERS} ${SOURCES})
