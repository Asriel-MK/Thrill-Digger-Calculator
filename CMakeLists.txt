# =================================================================================================
# FILE: CMakeLists.txt
#
# DESCRIPTION:
# This file is the configuration entry point for the CMake build system.
# CMake is a tool that generates standard build files (like Visual Studio solutions, Makefiles,
# or Ninja build files) from this configuration.
#
# IMPORTANCE:
# It abstracts the build process, making it easier to compile the project across different
# environments without worrying about specific compiler commands. It handles dependencies,
# compilation flags, and linking libraries automatically.
#
# INTERACTION:
# - Reads source files (src/main.cpp).
# - Defines the output executable (ThrillDiggerCalculator).
# - Links against necessary system libraries (comctl32, user32, gdi32, kernel32).
# - Sets compiler standards (C++17).
#
# TO USE:
# 1. Install CMake.
# 2. Run `cmake .` (or `cmake -B build`) to generate build files.
# 3. Run `cmake --build .` (or `cmake --build build`) to compile.
# =================================================================================================

# Specifies the minimum version of CMake required to run this script.
# Version 3.15 is a safe modern baseline.
cmake_minimum_required(VERSION 3.15)

# Defines the project name and the languages used (CXX stands for C++).
project(ThrillDiggerCalculator LANGUAGES CXX)

# -------------------------------------------------------------------------------------------------
# COMPILER SETTINGS
# -------------------------------------------------------------------------------------------------

# Use C++17 standard. This is required for modern features used in the code
# (like std::clamp, constexpr, structured bindings, etc.).
set(CMAKE_CXX_STANDARD 17)

# Force the build to fail if C++17 is not supported by the compiler.
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -------------------------------------------------------------------------------------------------
# BUILD FLAGS (OPTIMIZATION)
# -------------------------------------------------------------------------------------------------

# Set flags specifically for the "Release" build type.
# /O2      : Maximize speed.
# /DNDEBUG : Disable debug assertions (removes overhead).
# /GL      : Enable Whole Program Optimization (allows cross-module inlining).
set(CMAKE_CXX_FLAGS_RELEASE "/O2 /DNDEBUG /GL")

# Set linker flags for Release mode.
# /LTCG    : Link Time Code Generation (companion to /GL).
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "/LTCG")

# -------------------------------------------------------------------------------------------------
# EXECUTABLE DEFINITION
# -------------------------------------------------------------------------------------------------

# Define the final executable output named "ThrillDiggerCalculator".
# The "WIN32" keyword tells CMake this is a Windows GUI application (not a console app),
# which affects the entry point (WinMain vs main).
add_executable(ThrillDiggerCalculator WIN32
    src/main.cpp
)

# Add 'src' to the include path.
# This allows code in main.cpp to do `#include "solver.h"` finding it in the src folder.
target_include_directories(ThrillDiggerCalculator PRIVATE src)

# -------------------------------------------------------------------------------------------------
# LINKING LIBRARIES
# -------------------------------------------------------------------------------------------------

# Link specific Windows system libraries required for the GUI.
# - comctl32: Common Controls (buttons, progress bars, modern UI styles).
# - user32:   User Interface (windows, messages, input).
# - gdi32:    Graphics Device Interface (drawing shapes, text, colors).
# - kernel32: Core OS functions (memory, processes).
target_link_libraries(ThrillDiggerCalculator PRIVATE
    comctl32
    user32
    gdi32
    kernel32
)

# -------------------------------------------------------------------------------------------------
# PROPERTIES
# -------------------------------------------------------------------------------------------------

# Set specific properties for the target.
# WIN32_EXECUTABLE TRUE:  Reinforces that this is a GUI app (no command prompt window pops up).
# OUTPUT_NAME:            Ensures the final file is named "ThrillDiggerCalculator.exe".
set_target_properties(ThrillDiggerCalculator PROPERTIES
    WIN32_EXECUTABLE TRUE
    OUTPUT_NAME "ThrillDiggerCalculator"
)
