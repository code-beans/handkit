# Getting started

This guide takes you from a fresh checkout to your own app running on a handheld.

## Requirements

- Python 3, [Conan](https://conan.io) 2.33 or newer, and CMake 3.24 or newer.
- For deploying over USB: Android platform-tools (`adb`) and a handheld with USB ADB enabled. SSH is not needed.
- macOS or Linux, on ARM64 or x86_64. Windows hasn't been tried.

On macOS:

```sh
brew install conan cmake android-platform-tools
```

You don't need Docker, a chroot, a system cross-compiler or a system shader compiler. Conan downloads a checksum-verified Zig compiler and builds everything else.

## Build and run the demo

From the Handkit repository:

```sh
./tools/handkit build
./tools/handkit deploy
```

The first build exports Handkit's Conan recipes, downloads the compiler, and builds SDL3, the helper library and the shader compiler. That takes a few minutes; later builds reuse Conan's cache. If you have no Conan profile yet, the CLI creates one for your computer.

The result is a NextUI application folder:

```text
build/Release/dist/SDL3 Demo.pak/
├── app                 # ARM64 Linux executable
├── launch.sh           # NextUI launcher
└── assets/shaders/     # compiled SPIR-V shaders
```

`deploy` copies it to `/mnt/SDCARD/Tools/tg5040/` on the handheld. Open **Tools → SDL3 Demo**, then disconnect USB if you like.

| Control | Action |
| --- | --- |
| X / Y | Next / previous rendering mode, on release |
| Start + Select | Exit to NextUI |
| D-pad / left stick | Move the square in 2D; rotate the cube in 3D |
| A / B in 3D | Pause / reset |
| L1 / R1 in 3D | Zoom out / in |

The three modes are software 2D, OpenGL ES 2D and Vulkan 3D, each with an FPS counter. Switching destroys one mode's rendering resources before creating the next. A mode the device's driver can't run is skipped.

## Create your own app

Install the Handkit packages into your local Conan cache, then create an app from the starter template in a new directory:

```sh
./tools/handkit configure
./tools/handkit init ../my-app
```

`init` copies [`examples/minimal`](../examples/minimal) and refuses to write into an existing directory. Build and deploy it:

```sh
SDK_DIR="$(pwd)"
APP_DIR="$SDK_DIR/../my-app"

conan install "$APP_DIR" -of "$APP_DIR/build" \
  -pr:b default -pr:h "$SDK_DIR/profiles/aarch64" \
  --lockfile="$SDK_DIR/conan.lock" --build=missing
cmake -S "$APP_DIR" -B "$APP_DIR/build" \
  -DCMAKE_TOOLCHAIN_FILE="$APP_DIR/build/conan_toolchain.cmake" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build "$APP_DIR/build" --target my-app-pak
"$SDK_DIR/tools/handkit" deploy "$APP_DIR/build/dist/My Application.pak"
```

- `conan install` fetches the dependencies and writes a CMake toolchain file for the handheld.
- `cmake` configures, compiles and packages the app.
- `deploy` copies the `.pak` folder to the handheld.

Open **Tools → My Application**. Then edit:

- `main.c` for what the app does,
- `CMakeLists.txt` for the executable name, display name and assets,
- `conanfile.txt` for extra dependencies.

The starter uses `Handkit::Handkit` from Conan; it doesn't reference any files in the Handkit repository.

### Build and host profiles

Conan uses two profiles. The **build profile** (`default`) describes your computer, where the compiler and shader tools run. The **host profile** (`profiles/aarch64`) describes the handheld, where the app runs: ARM64 Linux, Clang 19, glibc 2.33. The same host profile builds for every supported handheld, because they share the CPU architecture and Handkit picks the GPU backend when the app starts. Keep the two apart when you add dependencies: tools go in `tool_requires`, libraries in `requires`.

## Link and package an app

Your app's `CMakeLists.txt` needs:

```cmake
find_package(Handkit CONFIG REQUIRED)
add_executable(my-app main.c)
target_link_libraries(my-app PRIVATE Handkit::Handkit)
hk_add_pak(my-app "My Application")
```

`Handkit::Handkit` brings in SDL3. `hk_add_pak()` adds a `my-app-pak` build target that creates the `.pak` folder. For images, shaders or other data:

```cmake
hk_add_pak(my-app "My Application"
  ASSETS "${CMAKE_CURRENT_SOURCE_DIR}/assets")
```

Assets are copied into the package's `assets/` folder. Find them at run time with `hk_asset_path(path, sizeof(path), "assets/image.bmp")`, which resolves them next to the executable instead of relying on the working directory.

The display name may contain letters, numbers, spaces, underscores and hyphens.

## Run, debug and test

```sh
./tools/handkit run --seconds 5
./tools/handkit run --smoke
./tools/handkit logs
```

- `run` starts the demo over USB while NextUI is at its main menu: it pauses NextUI, runs the app with its output in your terminal, then resumes NextUI.
- `--smoke` cycles through all demo modes twice and fails if any mode fails.
- `logs` prints the `app.log` that `launch.sh` writes when the app is started from Tools.

The time limit and smoke test are implemented by the demo. Your own app has to handle these arguments to behave the same way.

For another installed package:

```sh
./tools/handkit run --pak 'My Application.pak' --seconds 5
./tools/handkit logs --pak 'My Application.pak'
```

Add `--serial DEVICE_ID` when several ADB devices are connected. Set `ADB` to the `adb` executable if it isn't on your `PATH`.

### Screenshots

The demo saves one rendered frame as a BMP when `HANDKIT_DEMO_CAPTURE` is set. `HANDKIT_DEMO_CAPTURE_FRAME` picks the frame (default 90), and `HANDKIT_DEMO_MODE` (`software` or `vulkan`) the starting mode. With NextUI at its main menu:

```sh
adb shell 'cd "/mnt/SDCARD/Tools/tg5040/SDL3 Demo.pak" && HANDKIT_DEMO_MODE=vulkan HANDKIT_DEMO_CAPTURE=/tmp/shot.bmp ./app 3'
adb pull /tmp/shot.bmp
```

Pause NextUI while the demo runs (`kill -STOP`, then `kill -CONT` afterwards, as `tools/handkit run` does), so the two don't draw to the screen at the same time.

### Install as a game instead of a tool

`deploy --port` installs the app in NextUI's Ports folder, so it's listed with the games instead of under Tools:

```sh
./tools/handkit deploy --port 'build/Release/dist/My Application.pak'
```

### Device test package

```sh
./tools/handkit build --target handkit-test-pak
./tools/handkit deploy 'build/Release/dist/Handkit Test.pak'
```

`Handkit Test.pak` bundles the demo, the input self-test and a script that runs both and writes `handkit-report.txt` with system details. Releases ship it ready-made for testers; see [CONTRIBUTING.md](../CONTRIBUTING.md#report-a-device-result).

### Input self-test

```sh
./tools/handkit build --target sdk-input-check
```

This builds `build/Release/sdk-input-check`, a test program for the handheld. It checks button transitions, stick deadzones, reconnection and gamepad mappings using SDL's virtual controllers. Copy it to the device with `adb push` and run it with `adb shell`.

### Desktop preview

The demo's 2D mode also builds for your computer against a locally installed SDL3, which is handy for iterating on drawing code:

```sh
cmake -S . -B build-mac -DHANDKIT=OFF -DCMAKE_PREFIX_PATH=/opt/homebrew
cmake --build build-mac
./build-mac/handkit-sdl3
```

## Other firmwares and devices

`deploy` and `run` currently target NextUI on TrimUI devices. On other firmwares, copy the `.pak` folder to the device and start its `launch.sh` from the firmware's script or ports menu. [platforms.md](platforms.md) lists what's known to work where.
