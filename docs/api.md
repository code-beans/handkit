# API guide

Handkit's helper library is a small C API on top of SDL3. Include [`handkit/runtime.h`](../sdk/include/handkit/runtime.h), which also brings in input, LEDs and rumble; the menu and system controls have their own headers. Everything else is plain SDL3: you own your windows, renderers, GPU devices and audio.

Functions report failures by returning `false` and setting `SDL_GetError()`, like SDL itself.

## Start up and render

Call `hk_init()` with the rendering backend you want, instead of `SDL_Init()`. It selects Handkit's display driver and initializes SDL's video, joystick and gamepad subsystems.

| Backend | Use for | How it draws |
| --- | --- | --- |
| `HK_BACKEND_GLES` | 2D games and UIs | SDL's `opengles2` renderer on the GPU |
| `HK_BACKEND_VULKAN` | 3D, shaders | SDL's GPU API on Vulkan |
| `HK_BACKEND_SOFTWARE` | Pixel-exact or CPU drawing | You copy a surface to `/dev/fb0` yourself |

The window always covers the whole screen at its native resolution; `hk_display_size()` returns it.

### OpenGL ES 2D

```c
if (!hk_init(HK_BACKEND_GLES)) { /* SDL_GetError() */ }
SDL_Window *window;
SDL_Renderer *renderer;
if (!hk_create_gles_renderer(&window, &renderer)) { /* ... */ }
/* Poll events and draw with SDL's renderer API, vsynced. */
SDL_DestroyRenderer(renderer);
SDL_DestroyWindow(window);
SDL_Quit();
```

`hk_create_gles_renderer()` creates the fullscreen window and an `opengles2` renderer with vsync, and cleans up after itself if either step fails. The [starter](../examples/minimal/main.c) is a complete example.

### Vulkan 3D

Create a fullscreen `SDL_WINDOW_VULKAN` window at the display size and use SDL's GPU API. [`demo3d.c`](../examples/demo/demo3d.c) and its [shaders](../examples/demo/shaders/) show device and window setup, uniforms, a depth buffer, text overlay and presentation. Vulkan works on TrimUI's PowerVR GPU. On Mali (H700) it depends on the firmware's driver; check the result of `SDL_CreateGPUDevice()` and fall back to OpenGL ES.

### Software

The demo's software mode in [`main.c`](../examples/demo/main.c) shows how to draw into an SDL surface and copy it to the framebuffer, respecting its pixel format, stride and visible offset. This is an example, not a library API yet.

## Assets

```c
char path[512];
hk_asset_path(path, sizeof path, "assets/image.bmp");
```

Resolves a path relative to the executable, which is where `hk_add_pak(... ASSETS ...)` puts your files. Don't rely on the working directory.

## Input

```c
HkInput input;
hk_input_init(&input);
while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) { /* ... */ }
    hk_input_update(&input);   /* once per frame, after polling */
    if (input.pressed[HK_BUTTON_A]) { /* ... */ }
}
hk_input_close(&input);
```

`HkInput` holds, for each `HkButton`:

- `held`: down this frame,
- `pressed`: went down since the last update,
- `released`: went up since the last update.

It also has `dpad` (SDL hat flags such as `SDL_HAT_UP`), the sticks `left_x`, `left_y`, `right_x` and `right_y` from -1 to 1 with a deadzone, and the analog triggers `left_trigger` and `right_trigger` from 0 to 1. L2 and R2 are triggers, not buttons. `hk_button_name()` returns a button's printed name.

**Buttons are named as printed on the handheld.** `HK_BUTTON_A` is the right face button and `HK_BUTTON_B` the bottom one, as on Nintendo and most handhelds. Behind the names, buttons are read by position through SDL's gamepad mapping, so the same code works across devices and firmwares:

1. A mapping the firmware provides in `SDL_GAMECONTROLLERCONFIG`, as muOS and PortMaster do, always wins.
2. Otherwise Handkit adds mappings for the built-in controllers it knows: TrimUI's stock controller (adding F1/F2 and Home to SDL's own mapping) and the H700's stock "Deeplay-keys" controller.
3. A controller without any mapping is read by TrimUI's stock raw button numbers.

The volume keys aren't part of any gamepad layout; they're read on TrimUI's stock controller only.

Controllers are reattached after a disconnect. Input is sampled once per frame; if you need every quick tap, also handle SDL's gamepad events.

## In-game menu

[`handkit/menu.h`](../sdk/include/handkit/menu.h) provides a NextUI-style menu on the MENU button. NextUI's own in-game menu is part of its emulator frontend, so other apps can't open it; this one looks the same.

```c
HkMenu menu;
hk_menu_init(&menu, "My Game");
int sound = hk_menu_add_toggle(&menu, "Sound", &sound_on);
int speed = hk_menu_add_slider(&menu, "Speed", &speed_value, 1, 10, 1);
int mode = hk_menu_add_choice(&menu, "Mode", &mode_index, mode_names, 3);
int restart = hk_menu_add_action(&menu, "Restart");
```

Every menu has **Continue** and **Quit**; your items go between them. Each frame, after `hk_input_update()`:

1. Call `hk_menu_update(&menu, &input)`. MENU opens the menu; up/down select, A chooses, left/right change toggles, sliders and choices, B or MENU closes it.
2. While `menu.open`, or on any frame that returns an event, skip your game's input and simulation. This pauses the game and stops the closing button press from reaching it.
3. Handle the event: `HK_MENU_EVENT_CHANGED` (the menu stays open) and `HK_MENU_EVENT_ACTION` report the item in `menu.activated`; `HK_MENU_EVENT_QUIT` means exit.
4. Draw your game, then `hk_menu_draw(&menu, renderer)`, then present.

The menu uses SDL's debug font. To use your app's font, call `hk_menu_set_text_renderer()` with a function that draws a string and one that measures it; `size` is the line height in pixels, with the text's top at `y`.

## System controls

[`handkit/system.h`](../sdk/include/handkit/system.h) handles what NextUI leaves to the running app: Start + Select, the power button, and the volume and brightness bar. Call `hk_system_init()` after `hk_init()`, then each frame after `hk_input_update()`:

```c
hk_system_update(&input);
/* ...handle events, draw the game and the menu... */
hk_system_draw(renderer);
SDL_RenderPresent(renderer);
```

The controls arrive as the SDL events a desktop or phone sends, so code that handles those needs nothing device-specific:

| What happens | Events | What to do |
| --- | --- | --- |
| Start + Select | `SDL_EVENT_QUIT` | Save and exit |
| Power held for a second | `SDL_EVENT_QUIT` | Save and exit; `hk_system_quit()` then has NextUI shut down |
| Power tapped | `SDL_EVENT_WILL_ENTER_BACKGROUND`, `SDL_EVENT_DID_ENTER_BACKGROUND` | Pause and close audio devices. The next update turns the screen and LEDs off and waits for power, suspending to RAM after two minutes. |
| Back from sleep | `SDL_EVENT_WILL_ENTER_FOREGROUND`, `SDL_EVENT_DID_ENTER_FOREGROUND` | Reopen audio |
| Volume, or MENU + volume | none | A level bar shows at the top right for a second |

`hk_system_update()` also returns these as `HK_SYSTEM_EVENT_QUIT`, `_POWER_OFF`, `_SLEEP` and `_WAKE` if you'd rather poll. If your app skips drawing unchanged frames, keep drawing while `hk_system_overlay_dirty()` is true. Call `hk_system_quit()` last before exiting.

The volume and brightness levels come from the firmware's `libmsettings.so` (NextUI and MinUI), loaded at run time. Without it, the power button still works and no level bar is shown.

## LEDs and rumble

[`handkit/feedback.h`](../sdk/include/handkit/feedback.h):

```c
hk_led_set(HK_LED_ALL, 0xFF8800, HK_LED_BREATH, 1500);
hk_rumble(120);
/* before exiting */
hk_led_restore();
hk_rumble_stop();
```

- **LEDs** (TrimUI A133P handhelds): `hk_led_set()` loops an effect in a color on a set of zones: the stick rings, middle, F1 and F2. Repeated identical calls are skipped, so calling it every frame is fine. The first call saves the launcher's LED setup; `hk_led_restore()` puts it back.
- **Rumble**: `hk_rumble(ms)` runs the motor; overlapping calls extend the pulse, and a background thread switches it off. `hk_rumble_set_strength()` lowers the strength by pulsing the on/off motor. It uses GPIO 227 on the A133P handhelds and the power chip's motor output on the H700 ones.

Both write to the firmware's sysfs files, which apps can access as root on these firmwares. Where a device lacks the hardware, the calls return `false` with `SDL_GetError()` set.

## Lifecycle

Destroy your windows, renderers and GPU devices before `SDL_Quit()`. Close input with `hk_input_close()`, call `hk_led_restore()` and `hk_rumble_stop()`, and call `hk_system_quit()` last.
