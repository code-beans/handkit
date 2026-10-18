#ifndef HK_SYSTEM_H
#define HK_SYSTEM_H

#include <handkit/menu.h>
#ifdef __cplusplus
extern "C" {
#endif

/* The system controls of a handheld app: Start + Select and the power button, and the
 * volume/brightness display. While an app runs, NextUI leaves these to the app; its keymon
 * daemon only changes the levels. They arrive as the SDL events a desktop or phone sends, so
 * code that handles those works:
 *
 *   - Start + Select: SDL_EVENT_QUIT, back to NextUI.
 *   - Hold power for a second: SDL_EVENT_QUIT. Save and exit; hk_system_quit() then has
 *     NextUI shut the device down.
 *   - Tap power: SDL_EVENT_WILL_ENTER_BACKGROUND and SDL_EVENT_DID_ENTER_BACKGROUND. Pause and
 *     close audio devices; the next hk_system_update() turns the screen off and blocks until
 *     power is pressed again, suspending to RAM after two minutes. It then pushes
 *     SDL_EVENT_WILL_ENTER_FOREGROUND and SDL_EVENT_DID_ENTER_FOREGROUND.
 *   - Volume, or MENU + volume for brightness: a level bar shows at the top right for a second.
 *
 * Per frame, after hk_input_update():
 *     hk_system_update(&input);
 *     ...handle SDL events as usual, draw the game and menu...
 *     hk_system_draw(renderer);
 *     SDL_RenderPresent(renderer);
 *
 * The levels come from NextUI's libmsettings, loaded at run time. Without it (outside NextUI)
 * the power button still works and no level bar shows.
 */

typedef enum HkSystemEvent {
    HK_SYSTEM_EVENT_NONE,
    HK_SYSTEM_EVENT_SLEEP,    /* power tapped: pause and close audio; the next update sleeps */
    HK_SYSTEM_EVENT_WAKE,     /* back from sleep: reopen audio */
    HK_SYSTEM_EVENT_POWER_OFF, /* power held: save and exit; SDL_EVENT_QUIT is pushed once */
    HK_SYSTEM_EVENT_QUIT       /* Start + Select: save and exit to NextUI */
} HkSystemEvent;

/* Call after hk_init(). Init and quit may pair up more than once in a process. */
void hk_system_init(void);
/* Pushes the SDL events above. Also returns them, for apps that would rather poll. */
HkSystemEvent hk_system_update(const HkInput *input);
/* True while the level bar shows and on the update it hides, for apps that skip redrawing
 * unchanged frames. */
bool hk_system_overlay_dirty(void);
/* Draw the level bar's label with the app's font instead of SDL's debug font. */
void hk_system_set_text_renderer(HkMenuDrawText draw_text, HkMenuTextWidth text_width, void *user);
/* Draws the level bar, if showing. Restores the renderer's draw color, blend mode and scale. */
void hk_system_draw(SDL_Renderer *renderer);
/* Call last before exiting. After HK_SYSTEM_EVENT_POWER_OFF, NextUI then shuts down. */
void hk_system_quit(void);

#ifdef __cplusplus
}
#endif
#endif
