# ArkOS / R36S

Build SDL3 with KMSDRM and OpenGL ES 2 support, then add this SDK with
`-DHANDKIT_DEVICE=arkos`. The SDL3::SDL3 target may already exist in the parent
project. Otherwise the SDK finds an installed SDL3 package as before.

`hk_init(HK_BACKEND_GLES)` uses SDL's KMS/DRM backend and queries the display mode
through SDL. Software and Vulkan backends are currently rejected. Use the
firmware's libEGL, libGLESv2, libgbm and libdrm; do not ship Mesa replacements for
its Mali driver. When GBM and GLES resolve to the same Mali library, Handkit
selects that library for EGL as well. This handles dArkOSRE images with a separate
`libEGL.so.1`; explicit SDL library hints/environment settings still take priority.
Release builds do this silently; debug builds log the selected libraries.
Build against glibc 2.28 or older for original ArkOS compatibility.

Input, assets and menus retain the same API. Before starting the application,
convert `/home/ark/.emulationstation/es_input.cfg` (falling back to
`/etc/emulationstation/es_input.cfg`) with `sdk/cmake/arkos-gamepads.py` (installed beside `HandkitTools.cmake`)
and set `SDL_GAMECONTROLLERCONFIG_FILE` to the resulting file. This preserves
the firmware's configured physical buttons without assuming raw button indices.
An application-local gamecontrollerdb.txt can be used instead. Handkit A/B names
still mean the right/bottom buttons. If no SDL mapping exists, the built-in
`GO-Super Gamepad` mapping covers dArkOSRE's R36S controller, including
Select/Start on raw buttons 12/13. Map Select to MENU in the application if
needed: Fn is not consistently exposed by R36S firmware.

`hk_system_update` handles Start+Select with a quit event. Power, brightness and
volume remain owned by ArkOS. The system overlay is absent, feedback requests
return an unsupported error, and feedback cleanup is a no-op. Applications
should hide their haptic and RGB options for this target.

Launch through an ArkOS Ports shell script with assets beside the executable.
This requires no PortMaster installation or runtime. SDL and Handkit can
be linked statically while the firmware supplies graphics and audio drivers.

Game launch, Start+Select, volume and power-button behavior have been confirmed by a user on
an R36S running dArkOSRE. Original ArkOS and other device/firmware combinations
remain untested; container checks alone cannot verify their display or controls.

For library checks, use the separate test project with an SDL3 build for the
machine running the tests (including an ARM64 container):

```sh
cmake -S tests -B build/checks -DHANDKIT_DEVICE=arkos -DSDL3_DIR=/path/to/SDL3/cmake/config
cmake --build build/checks
ctest --test-dir build/checks --output-on-failure
```

The tests link the actual helper library, exercise virtual controllers without a
screen and verify graphics-library selection. They do not test the real GPU.
The Python conversion test can also run with `python3 tests/arkos_gamepads.py`.

The standard Conan packages and `tools/handkit build` still use the default
TrimUI integration and glibc 2.33 toolchain. ArkOS currently uses source integration
with an application-supplied SDL3 and compatible toolchain; choosing
`HANDKIT_DEVICE=arkos` does not retarget those Conan packages or enable KMS/DRM
in their SDL build.
