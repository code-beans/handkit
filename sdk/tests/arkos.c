#include <handkit/system.h>
#include <stdio.h>
#include <unistd.h>
#include "../src/arkos_graphics.h"
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s: %s\n", __FILE__, __LINE__, #x, SDL_GetError()); return 1; } } while (0)
int main(void) {
    char directory[] = "/tmp/handkit-graphics-XXXXXX";
    CHECK(mkdtemp(directory));
    char vendor[PATH_MAX], gbm[PATH_MAX], gles[PATH_MAX];
    SDL_snprintf(vendor, sizeof(vendor), "%s/libmali-test.so", directory);
    SDL_snprintf(gbm, sizeof(gbm), "%s/libgbm.so.1", directory);
    SDL_snprintf(gles, sizeof(gles), "%s/libGLESv2.so.2", directory);
    FILE *file = fopen(vendor, "w"); CHECK(file); fclose(file);
    CHECK(symlink(vendor, gbm) == 0);
    // Incomplete driver installation must not force an EGL implementation.
    hk_arkos_select_graphics(directory);
    CHECK(!SDL_GetHint(SDL_HINT_EGL_LIBRARY));
    CHECK(symlink(vendor, gles) == 0);
    hk_arkos_select_graphics(directory);
    CHECK(!SDL_strcmp(SDL_GetHint(SDL_HINT_EGL_LIBRARY), vendor));
    CHECK(!SDL_strcmp(SDL_GetHint(SDL_HINT_OPENGL_LIBRARY), vendor));
    SDL_SetHint(SDL_HINT_EGL_LIBRARY, "custom-egl.so");
    hk_arkos_select_graphics(directory);
    CHECK(!SDL_strcmp(SDL_GetHint(SDL_HINT_EGL_LIBRARY), "custom-egl.so"));
    SDL_ResetHint(SDL_HINT_EGL_LIBRARY);
    SDL_ResetHint(SDL_HINT_OPENGL_LIBRARY);
    CHECK(unlink(gles) == 0);
    file = fopen(gles, "w"); CHECK(file); fclose(file);
    hk_arkos_select_graphics(directory);
    CHECK(!SDL_GetHint(SDL_HINT_EGL_LIBRARY));
    unlink(gles); unlink(gbm); unlink(vendor); rmdir(directory);
    CHECK(!hk_init(HK_BACKEND_SOFTWARE));
    CHECK(!hk_init(HK_BACKEND_VULKAN));
    CHECK(SDL_Init(SDL_INIT_EVENTS | SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD));
    HkInput input = {0};
    hk_system_init();
    input.held[HK_BUTTON_SELECT] = true;
    CHECK(hk_system_update(&input) == HK_SYSTEM_EVENT_NONE);
    input.held[HK_BUTTON_START] = true;
    CHECK(hk_system_update(&input) == HK_SYSTEM_EVENT_QUIT);
    CHECK(SDL_HasEvent(SDL_EVENT_QUIT));
    SDL_FlushEvent(SDL_EVENT_QUIT);
    CHECK(hk_system_update(&input) == HK_SYSTEM_EVENT_NONE);
    CHECK(!SDL_HasEvent(SDL_EVENT_QUIT));
    input.held[HK_BUTTON_START] = false;
    CHECK(hk_system_update(&input) == HK_SYSTEM_EVENT_NONE);
    input.held[HK_BUTTON_START] = true;
    CHECK(hk_system_update(&input) == HK_SYSTEM_EVENT_QUIT);
    CHECK(!hk_system_overlay_dirty());
    CHECK(!hk_rumble(20));
    CHECK(!hk_led_set(HK_LED_ALL, 0, HK_LED_STATIC, 300));
    hk_rumble_stop(); hk_led_restore(); hk_system_quit();
    HkMenu menu;
    hk_menu_init(&menu, "Test");
    int volume = 50;
    hk_menu_add_slider(&menu, "Volume", &volume, 0, 100, 10);
    SDL_zero(input);
    input.pressed[HK_BUTTON_MENU] = true;
    CHECK(hk_menu_update(&menu, &input) == HK_MENU_EVENT_OPENED);
    SDL_zero(input); input.dpad = SDL_HAT_DOWN;
    hk_menu_update(&menu, &input);
    input.dpad = SDL_HAT_RIGHT;
    CHECK(hk_menu_update(&menu, &input) == HK_MENU_EVENT_CHANGED && volume == 60);
    SDL_zero(input); input.pressed[HK_BUTTON_B] = true;
    CHECK(hk_menu_update(&menu, &input) == HK_MENU_EVENT_CLOSED);
    SDL_Quit();
    puts("ArkOS backend, quit chord and controller menu checks passed");
    return 0;
}
