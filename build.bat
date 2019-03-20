@echo off

mkdir build
pushd build
cl -Zi ..\code\gb.c
popd
