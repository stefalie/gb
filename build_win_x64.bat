@echo off

rem ============================================================================
rem Input Validation

if not "%1" equ "Clang" (
	if not "%1" equ "Msvc" (
		goto :usage
	)
)
if not "%2" equ "Rel" (
	if not "%2" equ "Deb" (
		goto :usage
	)
)
if not "%3" equ "" (
	goto :usage
)

goto :build
:usage
@echo ERROR: Incorrect usage, use as follows:
@echo build_win_x64.bat (Clang^|Msvc) (Rel^|Deb)
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

rem -Wno-language-extension-token is used to prevent clang from complaining about
rem `typedef unsigned __int64 uint64_t` (and the like) in SDL headers.
rem Unfortunately we can't add -std=c11 because the same flags are also
rem applied to C++.
rem Include paths for SDL and Imgui come in two forms, with and without parent
rem dir. This is because Imgui wants them without parent dir.
rem We're explicitly allowing anonymous struct/unions. Unfortunately we can't use
rem the -std=c11 becasue clang doesn't like it when we also compile C++ files
rem at the same time.
set ClangCompilerFlags=-o %ExeName% -I%SdlDir% -I..\external -I%SdlDir%\SDL2 -I%ImguiDir% -Wall -Werror -Wextra -pedantic-errors -Wno-unused-parameter -Wno-language-extension-token -Wno-nested-anon-types -Wno-gnu-anonymous-struct -Wno-unused-but-set-variable
rem -fuse-ld=lld Use clang lld linker instead of msvc link.
rem /SUBSYSTEM:console warns about both main and wmain being present.
set ClangLinkerFlags=-fuse-ld=lld -Xlinker /INCREMENTAL:NO -Xlinker /OPT:REF -Xlinker /SUBSYSTEM:windows -lShell32 -lOpenGL32 -lComdlg32 %SdlLibs%
set ClangRelCompilerFlags=-DNDEBUG -O3 -Wno-unused-function
set ClangDebCompilerFlags=-g -fsanitize=address

rem /Zi Generates complete debugging information.
rem /FC Full paths in diagnostics
rem /WX Warning become errors
rem /WL One-line warnings/errors
rem /GR- Diable rttr
rem /EHa- Disable all exceptions
rem /wd4201 Disable warning about anonymous structs/unions, it's allowed in C11
rem See note under 'Clang' about duplicate include dirs.
set MsvcCompilerFlags=/Zi /FC /Fe%ExeName% /I%SdlDir% /I../external /I%SdlDir%/SDL2 /I../external/imgui /std:c11 /WX /W4 /WL /GR- /EHa- /wd4201
set MsvcLinkerFlags=/link /INCREMENTAL:NO /SUBSYSTEM:console /NOLOGO %SdlLibs% Shell32.lib OpenGL32.lib Comdlg32.lib
rem /Zo Generates enhanced debugging information for optimized code.
rem /Oi Generates intrinsic functions.
rem /GL Whole program optimization
set MsvcRelCompilerFlags=/Zo /O2 /Oi /GL
rem /LTCG (Link-time code generation)
set MsvcRelLinkerFlags=/LTCG /OPT:REF /OPT:ICF
rem /RTC1 Run-time error checks
rem Enabling asan seems to result in the generation of gl.lib and gl.exp files.
set MsvcDebCompilerFlags=/D_DEBUG /RTC1 /fsanitize=address
set MsvcDebLinkerFlags=/DEBUG

if "%1" equ "Clang" (
	set Compiler=clang
	if "%2" equ "Rel" (
		set CompilerFlags=%ClangCompilerFlags% %ClangRelCompilerFlags%
	) else (
		set CompilerFlags=%ClangCompilerFlags% %ClangDebCompilerFlags%
	)
	set LinkerFlags=%ClangLinkerFlags%
) else (
	rem NOTE: You can actually use clang-cl here if you remove /std:c11 and /WL.
	rem But then it will use the MS toolchain for linking (I think).
	set Compiler=cl
	if "%2" equ "Rel" (
		set CompilerFlags=%MsvcCompilerFlags% %MsvcRelCompilerFlags%
		set LinkerFlags=%MsvcLinkerFlags% %MsvcRelLinkerFlags%
	) else (
		set CompilerFlags=%MsvcCompilerFlags% %MsvcDebCompilerFlags%
		set LinkerFlags=%MsvcLinkerFlags% %MsvcDebLinkerFlags%
	)
)

mkdir build
pushd build
copy "%SdlDir%\lib\x64\SDL2.dll" .

set StartTime=%time%
echo on
%Compiler% %CompilerFlags% %CodeFiles% %LinkerFlags%
@echo off
set EndTime=%time%
popd

echo Start time: %StartTime%
echo End time:   %EndTime%

