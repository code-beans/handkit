/* No RGB or rumble hardware is exposed by this target. */
#include <handkit/feedback.h>

bool hk_led_set(Uint32 zones, Uint32 rgb, HkLedEffect effect, int ms) {
    (void)zones;
    (void)rgb;
    (void)effect;
    (void)ms;
    return SDL_SetError("RGB LEDs are not supported on R36S");
}

void hk_led_restore(void) {
}

bool hk_rumble(int ms) {
    (void)ms;
    return SDL_SetError("Rumble is not supported on R36S");
}

void hk_rumble_stop(void) {
}

void hk_rumble_set_strength(float strength) {
    (void)strength;
}
