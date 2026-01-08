{ pkgs ? (import <nixpkgs> {}) }: 
with pkgs;
mkShell {
  nativeBuildInputs = [ autoreconfHook pkg-config ];
  packages = with xorg; [
    git
    
    cmake
    ninja
    clang-tools

    llvmPackages_latest.lldb
    gdb

    llvmPackages_latest.libstdcxxClang

    cppcheck
    llvmPackages_latest.libllvm
    llvmPackages_latest.libcxx

    sccache
    shader-slang
    vulkan-headers vulkan-loader vulkan-validation-layers vulkan-memory-allocator
    alsa-lib libpulseaudio jack2 sndio mesa mesa_glu dbus systemd fcitx5
    wayland wayland-scanner
    ibus.dev
    libX11 libXext libXrandr libXcursor libXfixes libXi libXScrnSaver libxkbcommon libxcb
    glib pcre pcre2 libselinux libsepol util-linux
    sdl3
  ];

  # If it doesn’t get picked up through nix magic
  VULKAN_SDK = "${vulkan-validation-layers}/share/vulkan/explicit_layer.d";
  LIBCLANG_PATH="${pkgs.llvmPackages.libclang}/lib";
  SDL_VIDEO_DRIVER="wayland,x11";
}

