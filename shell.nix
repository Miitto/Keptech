{ pkgs ? (import <nixpkgs> {}) }: 
with pkgs;
mkShell {
  nativeBuildInputs = [ autoreconfHook pkg-config ];
  packages = with xorg; [
    clang-tools
    clang
    sccache
    shader-slang
    vulkan-headers vulkan-loader vulkan-validation-layers vulkan-memory-allocator
    buildPackages.stdenv git makeWrapper cmake ninja alsa-lib libpulseaudio jack2 sndio mesa mesa_glu dbus systemd fcitx5
    wayland wayland-scanner
    ibus.dev
    libX11 libXext libXrandr libXcursor libXfixes libXi libXScrnSaver libxkbcommon libxcb
    glib pcre pcre2 libselinux libsepol util-linux
  ];

  # If it doesn’t get picked up through nix magic
  VULKAN_SDK = "${vulkan-validation-layers}/share/vulkan/explicit_layer.d";
}

