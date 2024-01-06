# GB - GameBoy Emulator

TODO:
http://gameboy.mongenel.com/dmg/opcodes.html
https://cturt.github.io/cinoop.html

GB is a simple GameBoy emulator for Windows written in C11.

The emulator itself, `gb.h` and `gb.c`, is in pure C11 while all surrounding glue, UI, windowing code is in C-like C++ (this is because ImGui does not compile in C mode).

TODO: screenshot

## Dependencies

This project is shipped with third-party dependencies, each of which may have independent licensing (see [`external`](external) directory):

- [SDL](https://libsdl.org/)
- [ImGui](https://github.com/ocornut/imgui)
- [DMCA Sans Serif font](https://typedesign.netlify.app/dmcasansserif.html)

## Building on Windows

Windows is the only supported OS.

You require the [ImGui](https://github.com/ocornut/imgui) and [imgui_club][https://github.com/ocornut/imgui_club] (for the hex editor) sub-modules:

```
git submodule init && git submodule update
```

The build is triggered via the `build_win_x64.bat` batch script ([I'm really sick of CMake and the like](http://www.youtube.com/watch?v=Ee3EtYb8d1o&t=19m45s)) and can either use Clang or the Visual Studio Compiler.

### Clang

Execute `build_win_x64.bat Clang All Rel`.

### Visual Studio

Call `shell_msvc.bat` to setup the development environment (you might have to open it and adjust the path to `vcvarsall.bat` on your system depending on the Visual Studio version), then execute `build_win_x64.bat Msvc All Rel`.

### Development & Debugging

The build script can be called with these options: `build_win_x64.bat (Clang|Msvc) (Rel|Deb)`.

- `Clang` or `Msvc` to choose the compiler
- `Rel` or `Deb` for release or debug flags

For debugging call `devenv build\gb.exe` in the project directory (make sure `msvc_shell.bat` has been called first), or simply use the VSCode project by calling `code .` in the project directory (this also requires prior setup of the environment via `shell_msvc.bat`).

When running any of the [Blargg test ROMs](https://github.com/retrio/gb-test-roms/tree/master), set the `BLARGG_TEST_ENABLE` macro to `1` (otherwise, loading the ROM fails because it CGB ROM type).
If you want to see the output written to the serial port of the GameBoy, the build script needs to be modified to use `SUBSYSTEM:console` (otherwise, the printf output won't show).

## Resources

TODO: cleanup or remove

GameBoy CPU Manual
GameBoy Programming Manual
https://izik1.github.io/gbops/
blargg
cinoop
https://github.com/ThomasRinsma/dromaius
consider switching to clang-cl instead of clang (then you can just use the same compile params as for msvc)
