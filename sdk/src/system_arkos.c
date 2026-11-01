/* ArkOS owns power, volume and brightness. No TrimUI sysfs or NextUI calls. */
#include <handkit/system.h>

static bool quit_sent;
void hk_system_init(void) { quit_sent = false; }
HkSystemEvent hk_system_update(const HkInput *input) {
    bool quit = input->held[HK_BUTTON_START] && input->held[HK_BUTTON_SELECT];
    if (quit && !quit_sent) {
        SDL_Event event = {.type = SDL_EVENT_QUIT};
        SDL_PushEvent(&event);
        quit_sent = true;
        return HK_SYSTEM_EVENT_QUIT;
    }
    if (!quit) quit_sent = false;
    return HK_SYSTEM_EVENT_NONE;
}
void hk_system_quit(void) { quit_sent = false; }
bool hk_system_overlay_dirty(void) { return false; }
void hk_system_draw(SDL_Renderer *renderer) { (void)renderer; }
void hk_system_set_text_renderer(HkMenuDrawText draw, HkMenuTextWidth width, void *user) {
    (void)draw; (void)width; (void)user;
}
bool hk_led_set(Uint32 zones, Uint32 rgb, HkLedEffect effect, int ms) {
    (void)zones; (void)rgb; (void)effect; (void)ms;
    return SDL_SetError("RGB LEDs are not supported on R36S");
}
void hk_led_restore(void) {}
bool hk_rumble(int ms) {
    (void)ms;
    return SDL_SetError("Rumble is not supported on R36S");
}
void hk_rumble_stop(void) {}
void hk_rumble_set_strength(float strength) { (void)strength; }
