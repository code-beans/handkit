#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <handkit/input.h>
#include "demo_modes.h"
#ifdef HK_SDK
#include <handkit/runtime.h>
#include <handkit/system.h>
#endif
#ifndef DEMO_ENTRY
#define DEMO_ENTRY main
#endif
#ifdef HK_GPU
#include <SDL3/SDL_opengles2.h>
#endif
#ifdef HK_FB
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#endif
static const char *button_label(unsigned button) {
#ifdef HK_SDK
    return hk_button_name((HkButton)button);
#else
    static const char *const names[] = {"B", "A", "Y", "X", "L1", "R1", "Select", "Start", "Menu", "L3", "R3", "F1", "F2", "Vol-", "Vol+", "Home"};
    return button < HK_BUTTON_COUNT ? names[button] : "Unknown";
#endif
}

static volatile sig_atomic_t running = 1;

static void stop(int sig) {
    (void)sig;
    running = 0;
#ifdef DEMO_COMBINED
    demo_shutdown = 1;
#endif
}

int DEMO_ENTRY(int argc, char **argv) {
    running = 1;
    int mode_request = DEMO_EXIT;
    unsigned duration = argc > 1 ? (unsigned)atoi(argv[1]) : 0;
    signal(SIGTERM, stop);
    signal(SIGINT, stop);
    int w = 1024;
    int h = 768;
    SDL_Surface *surface = NULL;
    SDL_Window *window = NULL;
    SDL_Renderer *r = NULL;
#ifdef HK_FB
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
#elif defined(HK_GPU)
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "handkit");
#endif
#ifdef HK_SDK
#ifdef HK_FB
    HkBackend backend = HK_BACKEND_SOFTWARE;
#else
    HkBackend backend = HK_BACKEND_GLES;
#endif
    if (!hk_init(backend)) {
        fprintf(stderr, "Init: %s\n", SDL_GetError());
        return 1;
    }
#else
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD)) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
#endif
#ifdef HK_FB
    int fd = open("/dev/fb0", O_RDWR);
    struct fb_var_screeninfo v;
    struct fb_fix_screeninfo f;
    if (fd < 0 || ioctl(fd, FBIOGET_VSCREENINFO, &v) < 0 || ioctl(fd, FBIOGET_FSCREENINFO, &f) < 0) {
        perror("framebuffer");
        return 1;
    }
    w = v.xres;
    h = v.yres;
    SDL_PixelFormat format = SDL_GetPixelFormatForMasks(v.bits_per_pixel, ((1u << v.red.length) - 1) << v.red.offset, ((1u << v.green.length) - 1) << v.green.offset, ((1u << v.blue.length) - 1) << v.blue.offset,
                                                        v.transp.length ? ((1u << v.transp.length) - 1) << v.transp.offset : 0);
    unsigned char *fb = mmap(NULL, f.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (fb == MAP_FAILED || format == SDL_PIXELFORMAT_UNKNOWN) {
        fprintf(stderr, "Unsupported framebuffer\n");
        return 1;
    }
    size_t offset = (size_t)v.yoffset * f.line_length + v.xoffset * (v.bits_per_pixel / 8);
    if (offset + (size_t)(h - 1) * f.line_length + w * (v.bits_per_pixel / 8) > f.smem_len) {
        return 1;
    }
    size_t saved_size = (size_t)h * f.line_length;
    unsigned char *saved = malloc(saved_size);
    if (!saved) {
        return 1;
    }
    memcpy(saved, fb + (size_t)v.yoffset * f.line_length, saved_size);
    surface = SDL_CreateSurface(w, h, format);
    if (surface) {
        r = SDL_CreateSoftwareRenderer(surface);
    }
    fprintf(stderr, "Framebuffer %dx%d stride=%u offset=%zu format=%s\n", w, h, f.line_length, offset, SDL_GetPixelFormatName(format));
#elif defined(HK_GPU)
    const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode(SDL_GetPrimaryDisplay());
    if (!mode) {
        fprintf(stderr, "Display: %s\n", SDL_GetError());
        return 1;
    }
    w = mode->w;
    h = mode->h;
    if (!hk_create_gles_renderer(&window, &r)) {
        fprintf(stderr, "GLES: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    if (r) {
        SDL_SetRenderVSync(r, 1);
        const GLubyte *(*get_string)(GLenum) = (void *)SDL_GL_GetProcAddress("glGetString");
        if (get_string) {
            fprintf(stderr, "GL vendor=%s renderer=%s version=%s\n", get_string(GL_VENDOR), get_string(GL_RENDERER), get_string(GL_VERSION));
        }
    }
#else
    if (!SDL_CreateWindowAndRenderer("Handkit SDL3 demo", w, h, 0, &window, &r)) {
        r = NULL;
    }
#endif
    if (!r) {
        fprintf(stderr, "Renderer: %s\n", SDL_GetError());
        return 1;
    }
    fprintf(stderr, "SDL %d renderer=%s\n", SDL_GetVersion(), SDL_GetRendererName(r));
    int count = 0;
    SDL_JoystickID *ids = SDL_GetJoysticks(&count);
    SDL_Joystick *joy = count ? SDL_OpenJoystick(ids[0]) : NULL;
    SDL_free(ids);
    fprintf(stderr, "Joysticks=%d name=%s\n", count, joy ? SDL_GetJoystickName(joy) : "none");
    SDL_Gamepad *pad = joy && SDL_IsGamepad(SDL_GetJoystickID(joy)) ? SDL_OpenGamepad(SDL_GetJoystickID(joy)) : NULL;
    fprintf(stderr, "Gamepad=%s axes=%d buttons=%d\n", pad ? SDL_GetGamepadName(pad) : "unmapped", joy ? SDL_GetNumJoystickAxes(joy) : 0, joy ? SDL_GetNumJoystickButtons(joy) : 0);
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
#ifdef HK_SDK
    /* Start + Select, the power button and the volume bar. */
    HkInput input;
    hk_input_init(&input);
    hk_system_init();
#endif
    float x = w / 2.0f;
    float y = h / 2.0f;
    unsigned presses = 0;
    char last[100] = "Press a button";
    unsigned frames = 0;
    unsigned fps_frames = 0;
    float fps = 0;
    Uint64 fps_start = SDL_GetTicks();
    Uint64 start = SDL_GetTicks();
    Uint64 prev = start;
    bool buttons[32] = {0};
    bool triggers[2] = {0};
    while (running && (!duration || SDL_GetTicks() - start < duration * 1000ULL)) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) {
                running = 0;
            }
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) {
                running = 0;
            }
            if (e.type == SDL_EVENT_JOYSTICK_BUTTON_DOWN || e.type == SDL_EVENT_JOYSTICK_BUTTON_UP) {
                if (e.jbutton.button < 32) {
                    buttons[e.jbutton.button] = e.jbutton.down;
                }
                if (e.jbutton.down) {
                    presses++;
                    SDL_snprintf(last, sizeof last, "%s / presses %u", button_label(e.jbutton.button), presses);
                    fprintf(stderr, "%s\n", last);
                }
            }
        }
#ifdef HK_SDK
        /* Start + Select and holding power arrive as SDL_EVENT_QUIT on the next poll. */
        hk_input_update(&input);
        hk_system_update(&input);
#endif
#ifdef DEMO_COMBINED
        // Switch on release, so holding X/Y cannot skip several modes.
        if (input.released[HK_BUTTON_X]) {
            mode_request = DEMO_NEXT;
            running = 0;
        }
        if (input.released[HK_BUTTON_Y]) {
            mode_request = DEMO_PREVIOUS;
            running = 0;
        }
#endif
        if (pad) {
            for (int i = 0; i < 2; i++) {
                Sint16 value = SDL_GetGamepadAxis(pad, i ? SDL_GAMEPAD_AXIS_RIGHT_TRIGGER : SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
                bool down = value > 8000;
                if (down && !triggers[i]) {
                    presses++;
                    SDL_snprintf(last, sizeof last, "%s / presses %u", i ? "R2" : "L2", presses);
                    fprintf(stderr, "%s axis=%d\n", last, value);
                }
                triggers[i] = down;
            }
        }
        Uint64 now = SDL_GetTicks();
        float dt = SDL_min((now - prev) / 1000.0f, 0.1f);
        prev = now;
        const bool *keys = SDL_GetKeyboardState(NULL);
        float dx = keys[SDL_SCANCODE_RIGHT] - keys[SDL_SCANCODE_LEFT];
        float dy = keys[SDL_SCANCODE_DOWN] - keys[SDL_SCANCODE_UP];
        if (joy) {
            for (int i = 0; i < SDL_GetNumJoystickHats(joy); i++) {
                Uint8 hat = SDL_GetJoystickHat(joy, i);
                dx += !!(hat & SDL_HAT_RIGHT) - !!(hat & SDL_HAT_LEFT);
                dy += !!(hat & SDL_HAT_DOWN) - !!(hat & SDL_HAT_UP);
            }
            if (SDL_GetNumJoystickAxes(joy) > 1) {
                Sint16 a = SDL_GetJoystickAxis(joy, 0);
                Sint16 b = SDL_GetJoystickAxis(joy, 1);
                if (abs(a) > 8000) {
                    dx += a / 32768.0f;
                }
                if (abs(b) > 8000) {
                    dy += b / 32768.0f;
                }
            }
#ifndef HK_SDK
            // Without the SDK library (the desktop build) the demo checks the chord itself.
            // Physical button mapping supplied by the device owner.
            if (SDL_GetNumJoystickButtons(joy) > HK_BUTTON_START && SDL_GetJoystickButton(joy, HK_BUTTON_SELECT) && SDL_GetJoystickButton(joy, HK_BUTTON_START)) {
                running = 0;
            }
#endif
        }
        x = SDL_clamp(x + dx * 300 * dt, 24, w - 24);
        y = SDL_clamp(y + dy * 300 * dt, 180, h - 170);
        SDL_SetRenderDrawColor(r, 13, 20, 35, 255);
        SDL_RenderClear(r);
        SDL_SetRenderDrawColor(r, 28, 42, 60, 255);
        for (int i = 0; i < w; i += 48) {
            SDL_RenderLine(r, i, 160, i, h - 160);
        }
        for (int i = 160; i < h - 160; i += 48) {
            SDL_RenderLine(r, 0, i, w, i);
        }
        SDL_SetRenderScale(r, 3, 3);
        SDL_SetRenderDrawColor(r, 92, 231, 197, 255);
#ifdef DEMO_COMBINED
#ifdef HK_GPU
        SDL_RenderDebugText(r, 12, 12, "2/3 / OPENGL ES 2D");
#else
        SDL_RenderDebugText(r, 12, 12, "1/3 / SOFTWARE 2D");
#endif
#else
        SDL_RenderDebugText(r, 12, 12, "SDL3 / LINUX / HANDKIT");
#endif
#ifdef HK_GPU
        SDL_RenderDebugText(r, w / 3.0f - 100, 12, "GPU / GLES2");
#endif
        SDL_SetRenderDrawColor(r, 222, 232, 245, 255);
        SDL_RenderDebugTextFormat(r, 12, 28, "%dx%d  SDL %d.%d.%d", w, h, SDL_VERSIONNUM_MAJOR(SDL_GetVersion()), SDL_VERSIONNUM_MINOR(SDL_GetVersion()), SDL_VERSIONNUM_MICRO(SDL_GetVersion()));
        SDL_RenderDebugText(r, 12, 42, last);
        SDL_RenderDebugTextFormat(r, w / 3.0f - 100, 28, "FPS %.1f", fps);
        SDL_SetRenderScale(r, 1, 1);
        SDL_SetRenderDrawColor(r, 92, 231, 197, 255);
        SDL_FRect box = {x - 20, y - 20, 40, 40};
        SDL_RenderFillRect(r, &box);
        int n = joy ? SDL_min(SDL_GetNumJoystickButtons(joy), 20) : 0;
        for (int i = 0; i < n; i++) {
            float cell = (w - 60.0f) / 8;
            SDL_FRect b = {30 + (i % 8) * cell, h - 145.0f + (i / 8) * 40, cell - 6, 32};
            SDL_SetRenderDrawColor(r, buttons[i] ? 92 : 45, buttons[i] ? 231 : 62, buttons[i] ? 197 : 80, 255);
            SDL_RenderFillRect(r, &b);
            SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
            SDL_RenderDebugText(r, b.x + 8, b.y + 10, button_label(i));
        }
        for (int i = 0; i < 2; i++) {
            SDL_FRect b = {30.0f + i * 100, h - 65.0f, 88, 32};
            SDL_SetRenderDrawColor(r, triggers[i] ? 92 : 45, triggers[i] ? 231 : 62, triggers[i] ? 197 : 80, 255);
            SDL_RenderFillRect(r, &b);
            SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
            SDL_RenderDebugText(r, b.x + 8, b.y + 10, i ? "R2" : "L2");
        }
        SDL_SetRenderScale(r, 2, 2);
        SDL_SetRenderDrawColor(r, 222, 232, 245, 255);
#ifdef DEMO_COMBINED
        SDL_RenderDebugText(r, 15, h / 2.0f - 12, "D-pad: move / X-Y: cycle modes / Start+Select: exit");
#else
        SDL_RenderDebugText(r, 15, h / 2.0f - 12, "D-pad: move / buttons: light up / Start+Select: exit");
#endif
        SDL_SetRenderScale(r, 1, 1);
        frames++;
        fps_frames++;
        if (now - fps_start >= 500) {
            fps = fps_frames * 1000.0f / (now - fps_start);
            fps_start = now;
            fps_frames = 0;
        }
        const char *capture = SDL_getenv("HANDKIT_DEMO_CAPTURE");
        const char *capture_at = SDL_getenv("HANDKIT_DEMO_CAPTURE_FRAME");
        if (capture && frames == (capture_at ? (unsigned)SDL_atoi(capture_at) : 90)) {
            SDL_Surface *shot = SDL_RenderReadPixels(r, NULL);
            if (!shot || !SDL_SaveBMP(shot, capture)) {
                fprintf(stderr, "Capture failed: %s\n", SDL_GetError());
            } else {
                fprintf(stderr, "Captured GPU/render output: %s\n", capture);
            }
            SDL_DestroySurface(shot);
        }
#ifdef HK_SDK
        hk_system_draw(r);
#endif
        if (!SDL_RenderPresent(r)) {
            fprintf(stderr, "Present failed: %s\n", SDL_GetError());
            running = 0;
        }
#ifdef HK_FB
        for (int row = 0; row < h; row++) {
            memcpy(fb + offset + (size_t)row * f.line_length, (unsigned char *)surface->pixels + (size_t)row * surface->pitch, w * (v.bits_per_pixel / 8));
        }
#endif
#ifndef HK_GPU
        SDL_Delay(16);
#endif
    }
    fprintf(stderr, "Exit: presses=%u frames=%u average_fps=%.1f\n", presses, frames, frames * 1000.0 / (SDL_GetTicks() - start + 1));
    if (pad) {
        SDL_CloseGamepad(pad);
    }
    if (joy) {
        SDL_CloseJoystick(joy);
    }
#ifdef HK_SDK
    hk_input_close(&input);
    hk_system_quit();
#endif
    SDL_DestroyRenderer(r);
    SDL_DestroySurface(surface);
    SDL_DestroyWindow(window);
    SDL_Quit();
#ifdef HK_FB
    memcpy(fb + (size_t)v.yoffset * f.line_length, saved, saved_size);
    free(saved);
    munmap(fb, f.smem_len);
    close(fd);
#endif
    return mode_request;
}
