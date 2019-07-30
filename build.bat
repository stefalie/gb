@echo off

set ExeName=gb.exe

set SdlDir=..\external\SDL2-2.0.9
set SdlLibs=%SdlDir%\lib\x64\SDL2.lib %SdlDir%\lib\x64\SDL2main.lib
set CodeFiles=..\code\gb.c

set CompilerFlags=/Zi /FC /Fe%ExeName% /I..\code /I%SdlDir%\include /WX /W4 /wd4100 /WL
set LinkerFlags=/INCREMENTAL:NO /OPT:REF /SUBSYSTEM:console /NOLOGO %SdlLibs%

if "%1" == "release" (
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

