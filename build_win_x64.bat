@echo off

rem ============================================================================
rem Input Validation

if not "%1" == "Clang" (
    if not "%1" == "Msvc" (
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

set ExeName=gb.exe

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
rem A unity build is faster than listing required ImGui files.

set CodeFiles=..\code\main.cpp ..\code\gb.c ..\code\imgui_unity_build.cpp

if "%1" == "Clang" (
    set Compiler=clang

    rem -Wno-language-extension-token is used to prevent clang from complaining about
    rem `typedef unsigned __int64 uint64_t` (and the like) in SDL headers.
    rem Unfortunately we can't add -std=c11 because the same flags are also
    rem applied to C++.
    rem Include paths for SDL and Imgui come in two forms, with and without parent
    rem dir. This is because Imgui wants them without parent dir.
    set CompilerFlags=-o %ExeName% -I%SdlDir% -I..\external -I%SdlDir%\SDL2 -I%ImguiDir% -Wall -Werror -Wextra -pedantic-errors -Wno-unused-parameter -Wno-language-extension-token

    rem -fuse-ld=lld Use clang lld linker instead of msvc link.
    rem /SUBSYSTEM:console warns about both main and wmain being present.
    set LinkerFlags=-fuse-ld=lld -Xlinker /INCREMENTAL:NO -Xlinker /OPT:REF -Xlinker /SUBSYSTEM:windows -lShell32 -lOpenGL32 -lComdlg32 %SdlLibs%

    if "%2" == "Rel" (
        set OptFlags=-DNDEBUG -O3 -Wno-unused-function
    ) else (
        set OptFlags=-g
    )
) else (
    rem NOTE: You can actually use clang-cl here if you remove /std:c11 and /WL.
    rem But then it will use the MS toolchain for linking (I think).
    set Compiler=cl

    rem /Zi Generates complete debugging information.
    rem /FC Full paths in diagnostics
    rem /WX Warning become errors
    rem /WL One-line warnings/errors
    rem /GR- Diable rttr
    rem /EHa- Disable all exceptions
    rem See note under 'Clang' about duplicate include dirs.
    set CompilerFlags=/Zi /FC /Fe%ExeName% /I%SdlDir% /I../external /I%SdlDir%/SDL2 /I../external/imgui /std:c11 /WX /W4 /WL /GR- /EHa-

    if "%2" == "Rel" (
        rem /Zo Generates enhanced debugging information for optimized code.
        rem /Oi Generates intrinsic functions.
        set OptFlags=/Zo /O2 /Oi
    ) else (
        set OptFlags=/D_DEBUG
    )

    set LinkerFlags=/link /INCREMENTAL:NO /OPT:REF /SUBSYSTEM:windows /NOLOGO %SdlLibs% Shell32.lib OpenGL32.lib Comdlg32.lib
)

mkdir build
pushd build
copy "%SdlDir%\lib\x64\SDL2.dll" .

set StartTime=%time%
echo on
%Compiler% %CompilerFlags% %OptFlags% %CodeFiles% %LinkerFlags%
@echo off
set EndTime=%time%
popd

echo Start time: %StartTime%
echo End time:   %EndTime%

