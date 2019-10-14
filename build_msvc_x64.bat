@echo off

set ExeName=gb.exe

set CodeFiles=..\code\gb.c
set SdlDir=..\external\SDL2-2.0.10

rem /Zi Generates complete debugging information.
rem /FC Full paths in diagnostics
rem /WX Warning become errors
rem /wd4100 Suppress 'unreferenced formal parameter'
rem /WL One-line warnings/errors
rem /GR- Diable rttr
rem /EHa- Disable all exceptions
set CompilerFlags=/Zi /FC /Fe%ExeName% /I..\code /I%SdlDir%\include /WX /W4 /wd4100 /WL /GR- /EHa-					 

set SdlLibs=%SdlDir%\lib\x64\SDL2.lib %SdlDir%\lib\x64\SDL2main.lib

set LinkerFlags=/INCREMENTAL:NO /OPT:REF /SUBSYSTEM:windows /NOLOGO %SdlLibs%

if "%1" == "release" (
    rem /Zo Generates enhanced debugging information for optimized code.
    rem /Oi Generates intrinsic functions.
    set CompilerFlags=%CompilerFlags% /Zo /O2 /Oi
) else (
    set CompilerFlags=%CompilerFlags% /D_DEBUG
)

mkdir build
pushd build
copy "%SdlDir%\lib\x64\SDL2.dll" .
echo on
cl %CompilerFlags% %CodeFiles% /link %LinkerFlags%
@echo off
popd
