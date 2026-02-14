# Keptech

## Building

The included presets require [Ninja](https://ninja-build.org/). The `clang-x64` preset is recommended.

The project uses [SDL3](https://github.com/libsdl-org/SDL) and [Slang](https://github.com/shader-slang/slang). If they are not installed on the system, CMake will download and build them as part of the build process. It is reccommended to have them installed on the system if the engine itself is being modified.

> Building Slang on Windows requires being run in a Visual Studio Developer Command Prompt. If on x64 ensure the x64 developer command prompt is used. This is only required on a clean build, if Slang has already been built, the project can be built from any terminal.

To generate the build files, run

```bash
cmake --preset <preset>
```

Then to build, run

```bash
cmake --build --preset <preset>
```

To build for release, append `--config Release` to the build command. `Debug` is the default configuration.

The example projects are built by default. To skip building them set `KT_BUILD_EXAMPLES` to `OFF`.

Assuming a provided preset is used, examples can be run from the project root with:

```bash
./__output/<preset>/examples/<example>/Debug/<example>[.exe]
```

`.vscode/launch.json` also provides launch configurations for the examples.
