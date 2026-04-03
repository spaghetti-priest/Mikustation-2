
# On my MAC gcc just redirects to clang++ so maybe just support clang++ for now until I get a
# linux machine or someone else does it

#CXX = clang++

# ==============================================================================
# Headers and sources
# ==============================================================================
TARGET_EXE 	:= Mikustation_2
BUILD_DIR 	:= ./build
SRC_DIR 	:= ./src 
EXTERN_DIR 	:= ./extern 
SRCS 		:= $(SRC_DIR)/ps2.cpp $(EXTERN_DIR)/glad.c
SHELL_NAME 	:= $(shell uname)

COMPILE_FLAGS = -std=c++20 -g -Wall -Wformat
# LIBS =

ifeq ($(SHELL_NAME), Darwin)  
	@echo Building for MACOS
	LIBS += -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo `sdl2-config --libs`
endif

ifeq ($(OS), Windows_NT)
	@echo Building for Windows MinGW
	@echo Though it is recommended that you use the build.bat or CMAKE 
	LIBS += -Igdi
endif