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
convert `/home/ark/.emulationstation/es_input.cfg` with `tools/arkos-gamepads.py`
and set `SDL_GAMECONTROLLERCONFIG_FILE` to the resulting file. This preserves
the firmware's configured physical buttons without assuming raw button indices.
An application-local gamecontrollerdb.txt can be used instead. Handkit A/B names
still mean the right/bottom buttons. Map Select to MENU in the application if
needed: Fn is not consistently exposed by R36S firmware.

`hk_system_update` handles Start+Select with a quit event. Power, brightness and
volume remain owned by ArkOS. The system overlay is absent, feedback requests
return an unsupported error, and feedback cleanup is a no-op. Applications
should hide their haptic and RGB options for this target.

Launch through an ArkOS Ports shell script with assets beside the executable.
This requires no PortMaster installation or runtime. SDL/Box2D and Handkit can
be linked statically while the firmware supplies graphics and audio drivers.

Game launch, volume and power-button behavior have been confirmed by a user on
an R36S running dArkOSRE. Original ArkOS and other device/firmware combinations
remain untested; container checks alone cannot verify their display or controls.

For backend checks, configure with `-DHANDKIT_BUILD_TESTS=ON`, build the
`handkit-arkos-check` target and run CTest in the SDK's binary directory.
Run `python3 tests/arkos_gamepads.py` to check controller configuration conversion.
