# Architecture

How Handkit's pieces fit together, and how to change them safely.

## Packages

Handkit is three Conan packages plus one from ConanCenter.

| Package | Context | Responsibility |
| --- | --- | --- |
| `handkit-toolchain/0.14.1` | Build | Pinned Zig 0.14.1 (Clang 19) with wrapper scripts targeting `aarch64-linux-gnu.2.33`, plus a CMake toolchain file |
| `handkit-sdl/3.2.10` | Host | SDL 3.2.10 with Handkit's display backend compiled in, as a static library with SDL's public and Khronos headers |
| `handkit/0.4.0` | Host | The helper library (`Handkit::Handkit`) and the `hk_add_pak()` CMake module |
| `glslang/1.4.357.0` | Build | Native shader compiler for the demo's Vulkan shaders |

The **build context** runs on your computer; the **host context** is the handheld. The host profile [`profiles/aarch64`](../profiles/aarch64) selects Linux, `armv8`, Clang 19 and glibc 2.33, which fits every supported handheld.

Apps link statically against SDL3 and the helper library. At run time they need only glibc and the firmware's GPU driver, which is loaded with `dlopen` and never redistributed.

## Display backend

`handkit-sdl` builds [`src/platform/`](../src/platform) into SDL through SDL's private-driver hook (`SDL_VIDEO_DRIVER_PRIVATE`). It uses SDL's internal headers but doesn't patch SDL's source. The driver is registered as `handkit`; `hk_init()` selects it.

The handhelds' GPU drivers draw straight to the framebuffer, without X11, Wayland or KMS:

- **OpenGL ES:** the backend reads the screen size from `/dev/fb0` and creates one fullscreen window at that size. It then picks the EGL native window by vendor:
  - **PowerVR** (A133P): native window `0`, the driver's NULL window system.
  - **Mali** (H700), when the EGL vendor contains "ARM": a pointer to a `{unsigned short width, height}` struct, Mali's `fbdev_window`. It lives in the window's data, because the driver keeps the pointer.

  SDL's `opengles2` renderer draws and swaps; no CPU copy is involved.
- **Vulkan:** the backend loads `libvulkan.so.1`, requires `VK_KHR_surface` and `VK_KHR_display`, and creates the surface with SDL's direct-display helper. SDL's GPU API runs on top. If the driver lacks these extensions, `SDL_CreateGPUDevice()` fails and the app can fall back to OpenGL ES.
- **Software:** SDL's `dummy` video driver. The demo draws with SDL's software renderer and copies the result into `/dev/fb0`, honoring its pixel masks, stride and visible offset, and restores the framebuffer afterwards.

Only one program can render at a time, so the firmware's frontend must release the screen while an app runs. NextUI does this for Tools and Ports. For USB testing, `tools/handkit run` pauses NextUI and resumes it afterwards.

## Input mapping

`hk_input_update()` opens the first controller and reads it through `SDL_Gamepad` whenever SDL has a mapping for it. Mappings are added in increasing priority:

1. SDL's built-in database (the TrimUI controller reports itself as an Xbox 360 pad),
2. Handkit's additions, through `SDL_AddGamepadMapping()`: the stock TrimUI controller by GUID, with F1/F2 and Home, and controllers matched by name, such as the H700's "Deeplay-keys",
3. `SDL_GAMECONTROLLERCONFIG` from the environment, which SDL ranks above any mapping added by the API.

`HkButton` names are translated to gamepad positions (A is east, B is south). Without a mapping, the enum values are used as raw joystick button numbers, which match TrimUI's stock controller.

## Helper library

[`sdk/src/`](../sdk/src):

- `runtime.c`: start-up, display size, the GLES helper, asset paths, input.
- `menu.c`: the in-game menu.
- `system.c`: Start + Select, power button and sleep, and the volume bar. Power key events are read straight from `/dev/input` without grabbing the device, so the firmware still sees the volume keys. Volume and brightness levels come from the firmware's `libmsettings.so` through `dlopen`.
- `feedback.c`: LEDs through `/sys/class/led_anim/`, and rumble through whichever motor switch exists: GPIO 227 on A133P, `axp2202-battery/moto` on H700. A background thread times the pulses.

## Repository layout

| Path | Contents |
| --- | --- |
| `sdk/include/handkit/` | Public headers |
| `sdk/src/` | Helper library |
| `sdk/cmake/` | `hk_add_pak()` and the `.pak` launcher template |
| `src/platform/` | SDL display backend |
| `recipes/` | Conan recipes for the toolchain, SDL and the SDK |
| `profiles/` | Host profile for the handhelds |
| `examples/demo/` | The demo app and its shaders |
| `examples/minimal/` | Starter project copied by `tools/handkit init` |
| `tests/` | Input self-test for the device |
| `tools/handkit` | Developer CLI: configure, build, init, deploy, run, logs |
| `conan.lock` | Pinned dependency versions and recipe revisions |

## Changing recipes or the SDK

`tools/handkit build` exports the local recipes and builds against the committed lockfile, so dependencies don't change unexpectedly. The lockfile pins recipe revisions too: after changing anything in `recipes/`, `sdk/` or `src/platform/`, refresh it, or the build keeps using the previous package:

```sh
./tools/handkit configure --update-lock
./tools/handkit build
```

Review the lockfile change before committing. Changing the public API also means bumping the version in `recipes/handkit/conanfile.py` and `HK_VERSION`.

Upgrading SDL means rebuilding the backend against SDL's new internal headers and repeating the device smoke test, since private interfaces can change between SDL releases.

Don't edit files in Conan's cache to make permanent changes; change the recipe or source here and let Conan rebuild. `conan cache path <reference>` finds a package if you need to look inside.
