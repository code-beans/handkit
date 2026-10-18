#ifndef HK_FEEDBACK_H
#define HK_FEEDBACK_H

#include <SDL3/SDL.h>
#ifdef __cplusplus
extern "C" {
#endif

/* RGB LEDs, driven through the firmware's led_anim driver (TrimUI A133P handhelds).
 * Zones are a bit mask; HK_LED_ALL covers every zone on the Brick Pro.
 * Elsewhere the calls return false with SDL_GetError() set.
 */
typedef enum HkLedZone {
    HK_LED_STICKS = 1 << 0, /* left and right joystick rings */
    HK_LED_MIDDLE = 1 << 1,
    HK_LED_F1 = 1 << 2,
    HK_LED_F2 = 1 << 3,
    HK_LED_ALL = HK_LED_STICKS | HK_LED_MIDDLE | HK_LED_F1 | HK_LED_F2
} HkLedZone;

/* Values match the driver's effect_names. */
typedef enum HkLedEffect {
    HK_LED_OFF = 0,
    HK_LED_LINEAR = 1,
    HK_LED_BREATH = 2,
    HK_LED_SNIFF = 3,
    HK_LED_STATIC = 4,
    HK_LED_BLINK1 = 5,
    HK_LED_BLINK2 = 6,
    HK_LED_BLINK3 = 7
} HkLedEffect;

/* Loops `effect` in color 0xRRGGBB on `zones`; duration_ms is one animation cycle.
 * Unchanged requests are skipped, so this is safe to call every frame.
 * The first call saves the launcher's LED setup; call hk_led_restore() before exit.
 */
bool hk_led_set(Uint32 zones, Uint32 rgb, HkLedEffect effect, int duration_ms);
void hk_led_restore(void);

/* Runs the vibration motor for at least `ms` milliseconds: GPIO 227 on the A133P
 * handhelds, the power chip's motor output on the H700 ones. Overlapping calls extend
 * the pulse. A background thread stops it; no per-frame call is needed.
 * Call hk_rumble_stop() before SDL_Quit().
 */
bool hk_rumble(int ms);
void hk_rumble_stop(void);

/* Sets how hard later pulses vibrate, from 0 (off) to 1 (full, the default). The motor
 * is on/off only, so below 1 it is driven with PWM after a short full-power kick.
 */
void hk_rumble_set_strength(float strength);

#ifdef __cplusplus
}
#endif
#endif
