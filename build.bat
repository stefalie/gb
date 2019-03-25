@echo off

set SdlDir=..\external\SDL2-2.0.9
set SdlLibs=%SdlDir%\lib\x64\SDL2.lib %SdlDir%\lib\x64\SDL2main.lib
set CodeFiles=..\code\gb.c

mkdir build
pushd build
copy "%SdlDir%\lib\x64\SDL2.dll" .
cl /Zi /I%SdlDir%\include %CodeFiles% %SdlLibs% /link /SUBSYSTEM:console
popd

