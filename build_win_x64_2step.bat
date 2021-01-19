@echo off

rem ============================================================================
rem Input Validation

if not "%1" == "Clang" (
    if not "%1" == "Msvc" (
        echo argh
        goto :usage
    )
)
if not "%2" == "Rel" (
    if not "%2" == "Deb" (
        goto :usage
    )
)
if not "%3" == "" (
    goto :usage
)

goto :build
:usage
@echo ERROR: Incorrect usage, use as follows:
@echo build.bat (Clang^|Msvc) (Rel^|Deb)
goto :eof
:build

rem ============================================================================
rem Build

set SdlDir=..\external\SDL2-2.0.14
set SdlLibs=%SdlDir%\lib\x64\SDL2.lib %SdlDir%\lib\x64\SDL2main.lib
rem NOTE: SDL also needs Shell32.lib (if SDL2main.lib is used):
rem https://discourse.libsdl.org/t/windows-build-fails-with-missing-symbol-imp-commandlinetoargvw/27256
rem This is handled differently in Msvc and Clang, see below.
rem NOTE: If we define SDL_MAIN_HANDLED before including SDL.h, SDL2main.lib
rem (and Shell32.lib) are not required but it will force you to use subsystem
rem console. Subsequently also Shell32.lib won't be required anymore.
rem If that is used, I guess one should also call SDL_SetMainReady(), see:
rem https://wiki.libsdl.org/SDL_SetMainReady

set ImguiDir=..\external\imgui
set ImguiSources=%ImguiDir%\

set ExeName=gb.exe

rem With Clang, /SUBSYSTEM:console warns about both main and wmain being present.
set LinkerFlags=/OUT:%ExeName% /INCREMENTAL:NO /OPT:REF /SUBSYSTEM:windows /NOLOGO %SdlLibs% Shell32.lib OpenGL32.lib

if "%1" == "Clang" (
    rem TODO(stefalie): Consider using clang-cl, then all the args will become
    rem equivalent (or at least more similar).
    set Compiler=clang
    set Linker=lld-link
    set CompileExt=o

    rem -Wno-language-extension-token is used to prevent clang from complaining about
    rem `typedef unsigned __int64 uint64_t` (and the like) in SDL headers.
    set CompilerFlags=-c -I%SdlDir% -I..\external -Wall -Werror -Wextra -pedantic-errors -Wno-unused-parameter -Wno-language-extension-token
    set CFlags=-std=c11
    rem If compile and link is done in one step, /DEFAULTLIB:libcmt.lib seems
    rem to be added automatically:
    rem https://stackoverflow.com/questions/36783764/lld-undefined-symbol-maincrtstartup
    set LinkerFlags=%LinkerFlags% libcmt.lib

    if "%2" == "Rel" (
        set OptFlags=-DNDEBUG -O3 -Wno-unused-function
    ) else (
        set OptFlags=-g
    )
) else (
    rem NOTE: You can actually use clang-cl here if you remove /std:c11 and /WL.
    set Compiler=cl
    set Linker=link
    set CompileExt=obj

    rem /Zi Generates complete debugging information.
    rem /FC Full paths in diagnostics
    rem /c to compile without linking instead of using /Fe%ExeName%
    rem /WX Warning become errors
    rem /WL One-line warnings/errors
    rem /GR- Diable rttr
    rem /EHa- Disable all exceptions
    set CompilerFlags=/Zi /FC /c /I%SdlDir% /I../external /WX /W4 /WL /GR- /EHa-
    set CFlags=/TC /std:c11

    if "%2" == "Rel" (
        rem /Zo Generates enhanced debugging information for optimized code.
        rem /Oi Generates intrinsic functions.
        set OptFlags=/Zo /O2 /Oi
    ) else (
        set OptFlags=/D_DEBUG
    )
)

set CFiles=..\code\gb.c
set CppFiles=..\code\main.cpp
set Objects=main.%CompileExt% gb.%CompileExt%

mkdir build
pushd build
copy "%SdlDir%\lib\x64\SDL2.dll" .
echo on
%Compiler% %CompilerFlags% %CFlags% %OptFlags% %CFiles%
%Compiler% %CompilerFlags% %OptFlags% %CppFiles%
%Linker% %LinkerFlags% %Objects%
@echo off
popd

