# GB - GameBoy Emulator

GB is a simple GameBoy emulator for Windows written in C.

The emulator itself, `gb.h` and `gb.c`, is in pure C while all surrounding glue, UI, windowing code is in C-like C++ (this is because ImGui does not compile in C mode).

TODO: screenshot

## Dependencies

This project is shipped with third-party dependencies, each of which may have independent licensing (see [`external`](external) directory):

- [SDL](https://libsdl.org/)
- [ImGui](https://github.com/ocornut/imgui)
- [DMCA Sans Serif font](https://typedesign.netlify.app/dmcasansserif.html)

## Building on Windows (only)

You require the [ImGui](https://github.com/ocornut/imgui) sub-module:

```
git submodule init && git submodule update
```

The build is triggered via the `build_win_x64.bat` batch script ([I'm really sick of CMake and the like](http://www.youtube.com/watch?v=Ee3EtYb8d1o&t=19m45s)) and can either use Clang or the Visual Studio Compiler (only Clang supports [computed gotos](https://gcc.gnu.org/onlinedocs/gcc/Labels-as-Values.html) however).

### Clang

Execute `build_win_x64.bat Clang All Rel`.

### Visual Studio

Call `shell_msvc.bat` to setup the development environment (you might have to open it and adjust the path to `vcvarsall.bat` on your system), then execute `build_win_x64.bat Msvc All Rel`.

### Development & Debugging

The build script can be called with these options: `build_win_x64.bat (Clang|Msvc) (Rel|Deb)`.

- `Clang` or `Msvc` to choose the compiler
- `Rel` or `Deb` for release or debug flags

For debugging call `devenv build\gb.exe` in the build directory (make sure `msvc_shell.bat` has been called first), or simply use the VSCode project by calling `code .` in the project root (make sure `shell_msvc.bat` has been called first).

## Resources

TODO: cleanup or remove

https://eli.thegreenplace.net/2012/07/12/computed-goto-for-efficient-dispatch-tables
https://twitter.com/mknejp/status/1185310491545128961?s=09, also see Eli Bendersky's example
https://github.com/ThomasRinsma/dromaius
consider switching to clang-cl instead of clang (then you can just use the same compile params as for msvc)
