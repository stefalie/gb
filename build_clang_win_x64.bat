@echo off

set ExeName=gb.exe

set CodeFiles=..\code\gb.c
set SdlDir=..\external\SDL2-2.0.10

rem -Wno-language-extension-token is used to prevent clang from comlaining about
rem `typedef unsigned __int64 uint64_t` in SDL headers
set CompilerFlags=-o %ExeName% -I%SdlDir%\include -std=c99 -Wall -Werror -Wextra -pedantic-errors -Wno-unused-parameter -Wno-language-extension-token

set SdlLibs=%SdlDir%\lib\x64\SDL2.lib %SdlDir%\lib\x64\SDL2main.lib

rem -fuse-ld=lld Use clang lld linker instead of msvc link
set LinkerFlags=-fuse-ld=lld -Xlinker /INCREMENTAL:NO -Xlinker /OPT:REF -Xlinker /SUBSYSTEM:windows %SdlLibs%

if "%1" == "release" (
    set CompilerFlags=%CompilerFlags% -DNDEBUG -O2
) else (
    set CompilerFlags=%CompilerFlags% -g -Wno-unused-variable -Wno-unused-function
)

mkdir build
pushd build
copy "%SdlDir%\lib\x64\SDL2.dll" .
echo on
clang %CompilerFlags% %CodeFiles% %LinkerFlags%
@echo off
popd
