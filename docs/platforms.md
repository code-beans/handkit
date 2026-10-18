# Platforms

Handkit builds one ARM64 binary per app. At start-up the app picks the GPU backend from the driver it finds, reads the controller through SDL's gamepad mapping, and uses whichever LED and rumble hardware is present. So supporting a device mostly means checking that its firmware provides what that binary needs:

1. a 64-bit ARM (aarch64) system with **glibc 2.33 or newer**,
2. a GPU driver Handkit has a backend for: PowerVR or Mali, using the framebuffer without a window system,
3. a frontend that releases the screen while an app runs,
4. a controller SDL can map.

## Support tiers

| Tier | Meaning |
| --- | --- |
| **Tested** | Run on a device: the demo smoke test and the input self-test pass. |
| **Checked from source** | Each requirement above was confirmed in the firmware's published source or build files; not yet run on a device. |
| **Expected** | Shares the system and drivers of a tested or checked combination. |
| **Not supported** | A requirement is known to be missing. |

## Chips and backends

| Chip | GPU | Driver | Handkit backend | OpenGL ES | Vulkan |
| --- | --- | --- | --- | --- | --- |
| Allwinner A133P | PowerVR GE8300 | Vendor fbdev driver, no window system | PowerVR: EGL native window `0` | Tested | Tested (`VK_KHR_display`) |
| Allwinner H700 | Mali-G31 | Vendor `mali-g31-fbdev` driver | Mali: EGL native window is a pointer to `{width, height}` | Written, untested | Unknown: the driver ships a Vulkan 1.0 ICD; `VK_KHR_display` support unconfirmed |

The backend chooses Mali when the EGL vendor string contains "ARM", and PowerVR otherwise.

## TrimUI A133P: Brick, Brick Pro, Smart Pro

| Firmware | Tier | Notes |
| --- | --- | --- |
| **NextUI** | **Tested** (Brick Pro) | All three rendering modes at about 30, 59 and 59 FPS; controls, Start + Select, volume bar. `.pak` deployment with `tools/handkit deploy`. |
| Stock TrimUI OS, CrossMix | Expected | NextUI runs on the stock system, so drivers, glibc and controller are the same. Launching needs a stock `Apps/` entry, not yet generated. |
| spruce | Expected | An SD-card layer on the stock system; uses the same `trimui_inputd` controller. |
| knulli | Checked from source | Same 4.9.191 kernel, PowerVR fbdev driver (`libpvrNULL_WSEGL`) and vendor Vulkan as stock; glibc 2.40; runs `trimui_inputd`. Open: whether EmulationStation releases the screen while an app runs. |
| muOS | Checked from source | Loads `pvrsrvkm`; muOS builds its own A133P apps with fbdev EGL and Vulkan. Its frontend exits before an app starts. glibc 2.38 or newer. Its own input daemon creates a "muOS-Keys" controller with different button numbers, mapped through `SDL_GAMECONTROLLERCONFIG`. Open: LED and rumble may conflict with muOS's own services. |

On the stock controller, "TRIMUI Player1" reports itself as an Xbox 360 controller (USB `045e:028e`), so SDL already maps it; Handkit adds F1, F2 and Home.

## Anbernic H700: RG35XX, RG40XX, RG34XX, RG CubeXX, RG28XX

| Firmware | Tier | Notes |
| --- | --- | --- |
| knulli | Checked from source | 4.9.170 vendor kernel with `mali_kbase`, the `mali-g31-fbdev` driver, a Mali Vulkan ICD, glibc 2.40. The vendor kernel most likely provides the stock "Deeplay-keys" controller; unconfirmed. |
| muOS | Checked from source | Mali driver (muOS's own tools link `libmali`); glibc 2.38 or newer; "muOS-Keys" controller mapped through `SDL_GAMECONTROLLERCONFIG`. Open: whether its Mali driver is the fbdev build. |
| Stock Anbernic OS, MinUI | **Not supported** | The system is 32-bit (`arm-linux-gnueabihf`). Handkit needs a 32-bit build target first. |
| ROCKNIX | **Not supported** | Uses a mainline kernel with KMS/DRM, which needs SDL's `kmsdrm` driver; Handkit's SDL is built without it. |

Device notes:

- **Screens:** 640×480 on most models, 720×480 on the RG34XX, 720×720 on the RG CubeXX. Apps get the native size from `hk_display_size()`.
- **RG28XX:** its panel is mounted rotated and firmwares rotate the picture in software. Handkit doesn't rotate yet, so the picture will be sideways.
- **Controller:** the stock kernel driver is called "Deeplay-keys". Its mapping comes from MinUI's H700 button numbering (A 0, B 1, Y 2, X 3, L1 4, R1 5, Select 6, Start 7, Menu 8, L2 9, R2 10, d-pad 13–16) and needs confirming on a device.
- **Rumble:** writes `1`/`0` to `/sys/class/power_supply/axp2202-battery/moto`, as knulli does. Strengths below full use fast on/off switching, which this motor output may not handle well.
- **LEDs:** the RG40XX and RG CubeXX have RGB lights behind a serial microcontroller, which Handkit doesn't drive; `hk_led_set()` returns `false`.
- **Volume bar:** needs the firmware's `libmsettings.so`, which knulli and muOS don't have, so no bar is shown.

## Packaging per firmware

| Firmware | Format | Handkit support |
| --- | --- | --- |
| NextUI, MinUI | `.pak` folder in `Tools/<platform>/` or Ports | `hk_add_pak()`, `tools/handkit deploy` |
| muOS, knulli, ArkOS, ROCKNIX and more | PortMaster port | Planned |
| Stock TrimUI, CrossMix | `Apps/<name>/` with `config.json` | Not yet |

Until PortMaster packaging lands, run an app on muOS or knulli by copying its `.pak` folder to the SD card and starting its `launch.sh` from the firmware's ports or scripts menu.

## How these were checked

"Checked from source" entries come from reading each firmware's public repository at the commits below, plus the prebuilt binaries those repositories ship:

- knulli: `knulli-cfw/knulli-linux` at `57afa66`, its board configs, GPU driver packages and SDL patches.
- muOS: `MustardOS/internal` at `8450dbb`, `MustardOS/frontend` at `62ffc1d`, and `MustardOS/tool`. The glibc minimum comes from the symbol versions `internal/bin/joycal` requires.
- spruce: `spruceUI/spruceOS` at `03c3ff3`.
- MinUI: `shauninman/MinUI` and its `union-rg35xxplus-toolchain`.
- PortMaster: `PortsMaster/PortMaster-GUI` at `7d17fbb`.

Results on real devices replace these entries. [CONTRIBUTING.md](../CONTRIBUTING.md) explains how to report one.
