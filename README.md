# Keptech

## Cloning

The repository vendors dependencies that don't support CMake natively using git submodules. To clone the repository with the submodules, run

```bash
git clone --recurse-submodules https://github.com/Miitto/Keptech.git
```

If the repository has already been cloned without the `--recurse-submodules` flag, the submodules can be initialized and updated by running

```bash
git submodule update --init --recursive
```

## Building

### Dependencies

[Slang](https://github.com/shader-slang/slang) needs to be installed on the system and the `bin` directory of its install needs to be in the system's PATH.

> NOTE\
> Building Slang on Windows requires being run in an x64 Visual Studio Developer Command Prompt, likely with Administrator privileges.
>
> If using the DX12 backend, the `dxcompiler.dll` and `dxil.dll` files will also need to be copied over from the Slang build output to the Slang install `bin` directory. This will be from `<slang-repo>\build\<build-type>\bin\` to `C:\Program Files (x86)\slang\bin\`

The included presets require [Ninja](https://ninja-build.org/).

All other dependencies can be fetched automatically.

[SDL3](https://github.com/libsdl-org/SDL) and [KTX Tools](https://github.com/KhronosGroup/KTX-Software) will use system versions if available.

#### Vulkan Backend

The [Vulkan SDK](https://vulkan.lunarg.com/) and [VulkanMemoryAllocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator) must be installed on the system. Vulkan Memory Allocator can be installed with the Vulkan SDK.

#### DX12 Backend

The Windows SDK must be installed on the system. This can be done via the Visual Studio Installer.

### CMake

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
