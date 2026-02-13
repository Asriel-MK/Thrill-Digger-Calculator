@echo off
REM =================================================================================================
REM FILE: build.bat
REM
REM DESCRIPTION:
REM This is a Windows batch script designed to compile the project manually using the 
REM Microsoft Visual C++ compiler (MSVC). It acts as a lightweight alternative to using
REM CMake or the Visual Studio IDE.
REM
REM IMPORTANCE:
REM Allows for "one-click" building from the command line. It sets up the required environment
REM variables and invokes the compiler with the necessary flags.
REM
REM INTERACTION:
REM - Sets up the MSVC environment (vcvarsall.bat).
REM - Compiles 'src/main.cpp'.
REM - Links standard Windows libraries.
REM - Cleans up temporary object files (.obj).
REM
REM HOW IT WORKS:
REM It first looks for the Visual Studio Build Tools environment script. If found, it runs it
REM to make commands like `cl` (compiler) available. Then it compiles the code.
REM =================================================================================================

REM Setup the Visual Studio compiler environment for x64 architecture.
REM >nul 2>&1 hides the verbose output from this command to keep the terminal clean.
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1

REM -------------------------------------------------------------------------------------------------
REM COMPILATION COMMAND
REM -------------------------------------------------------------------------------------------------
REM cl                      : The MSVC compiler command.
REM /EHsc                   : Enable C++ Exception Handling (standard synchronous).
REM /O2                     : Optimize for maximum speed.
REM /std:c++17              : Use C++17 language standard.
REM /Fe:ThrillDiggerCalculator.exe : Name of the output executable file.
REM /DNDEBUG                : Define NDEBUG macro to disable debug asserts.
REM /W4                     : Warning level 4 (high) to catch potential issues.
REM src\main.cpp            : The source file to compile.
REM /link                   : Pass the following flags to the linker.
REM /SUBSYSTEM:WINDOWS      : Create a GUI application (no console window).
REM user32.lib ...          : Libraries to link against (User interface, graphics, controls).
cl /EHsc /O2 /std:c++17 /Fe:ThrillDiggerCalculator.exe /DNDEBUG /W4 src\main.cpp /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib comctl32.lib

REM -------------------------------------------------------------------------------------------------
REM CLEANUP
REM -------------------------------------------------------------------------------------------------
REM Delete the intermediate object file (main.obj) created during compilation.
REM It is no longer needed after the .exe is created.
del main.obj 2>nul

echo.
echo Build complete.
