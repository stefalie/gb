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

set MicrouiDir=..\external\microui

set ExeName=gb.exe
set CodeFiles=..\code\gb.c %MicrouiDir%\microui.c %MicrouiDir%\renderer.c

if "%1" == "Clang" (
    set Compiler=clang

    rem -Wno-language-extension-token is used to prevent clang from complaining about
    rem `typedef unsigned __int64 uint64_t` (and the like) in SDL headers.
    set CompilerFlags=-o %ExeName% -I%SdlDir% -I%MicrouiDir% -std=c11 -Wall -Werror -Wextra -pedantic-errors -Wno-unused-parameter -Wno-language-extension-token

    rem -fuse-ld=lld Use clang lld linker instead of msvc link.
    rem /SUBSYSTEM:console warns about both main and wmain being present.
    set LinkerFlags=-fuse-ld=lld -Xlinker /INCREMENTAL:NO -Xlinker /OPT:REF -Xlinker /SUBSYSTEM:windows -lShell32 -lOpenGL32 %SdlLibs%

    if "%2" == "Rel" (
        set OptFlags=-DNDEBUG -O3 -Wno-unused-function
    ) else (
        set OptFlags=-g
    )
) else (
    set Compiler=cl

    rem /Zi Generates complete debugging information.
    rem /FC Full paths in diagnostics
    rem /TC Use C globally
    rem /WX Warning become errors
    rem /WL One-line warnings/errors
    rem /GR- Diable rttr
    rem /EHa- Disable all exceptions
    set CompilerFlags=/Zi /FC /Fe%ExeName% /TC /std:c11 /I%SdlDir% /I%MicrouiDir% /WX /W4 /WL /GR- /EHa-

    if "%2" == "Rel" (
        rem /Zo Generates enhanced debugging information for optimized code.
        rem /Oi Generates intrinsic functions.
        set OptFlags=/Zo /O2 /Oi
    ) else (
        set OptFlags=/D_DEBUG
    )

    set LinkerFlags=/link /INCREMENTAL:NO /OPT:REF /SUBSYSTEM:windows /NOLOGO %SdlLibs% Shell32.lib OpenGL32.lib
)

mkdir build
pushd build
copy "%SdlDir%\lib\x64\SDL2.dll" .
echo on
%Compiler% %CompilerFlags% %OptFlags% %CodeFiles% %LinkerFlags%
@echo off
popd

