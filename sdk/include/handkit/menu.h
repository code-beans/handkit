#ifndef HK_MENU_H
#define HK_MENU_H

#include <handkit/runtime.h>
#ifdef __cplusplus
extern "C" {
#endif

/* NextUI-style in-game menu opened with the MENU button.
 * "Continue" and "Quit" are always present; apps add their own toggles and actions between them.
 *
 * Per frame, after hk_input_update():
 *     HkMenuEvent event = hk_menu_update(&menu, &input);
 *     if (!menu.open && event == HK_MENU_EVENT_NONE) { ...game input and simulation... }
 *     ...draw the game...
 *     hk_menu_draw(&menu, renderer);   // draws nothing while closed
 *     SDL_RenderPresent(renderer);
 * Skipping game input on the frame of any event keeps the closing A/B press from reaching the game.
 */

#define HK_MENU_MAX_ITEMS 16

/* Optional custom text rendering, so the menu can use the app's font.
 * `size` follows SDL's debug font: a line is `size` pixels tall, drawn with its top at `y`. */
typedef void (*HkMenuDrawText)(SDL_Renderer *renderer, float x, float y, const char *text, float size, SDL_Color color, void *user);
typedef float (*HkMenuTextWidth)(const char *text, float size, void *user);

typedef struct HkMenuItem {
    const char *label;
    bool *toggle; /* NULL for a slider, choice or action */
    int *slider;
    int minimum, maximum, step;
    int *choice; /* index into choices */
    const char *const *choices;
    int choice_count;
} HkMenuItem;

typedef struct HkMenu {
    const char *title;
    HkMenuItem items[HK_MENU_MAX_ITEMS];
    int item_count;
    int selected;
    int activated; /* item id for HK_MENU_EVENT_CHANGED / HK_MENU_EVENT_ACTION */
    bool open;
    Uint8 previous_dpad;
    HkMenuDrawText draw_text;   /* NULL uses SDL's debug font */
    HkMenuTextWidth text_width;
    void *text_user;
} HkMenu;

typedef enum HkMenuEvent {
    HK_MENU_EVENT_NONE,
    HK_MENU_EVENT_OPENED,
    HK_MENU_EVENT_CLOSED,  /* Continue, B or MENU */
    HK_MENU_EVENT_CHANGED, /* a toggle, slider or choice changed; the menu stays open */
    HK_MENU_EVENT_ACTION,  /* an action was chosen; the menu closes */
    HK_MENU_EVENT_QUIT     /* Quit was chosen; the menu closes */
} HkMenuEvent;

void hk_menu_init(HkMenu *menu, const char *title);
/* Return the item id reported in menu->activated, or -1 when the menu is full. */
int hk_menu_add_toggle(HkMenu *menu, const char *label, bool *value);
int hk_menu_add_slider(HkMenu *menu, const char *label, int *value, int minimum, int maximum, int step);
int hk_menu_add_action(HkMenu *menu, const char *label);
/* Steps through `count` named options; `choices` must outlive the menu. */
int hk_menu_add_choice(HkMenu *menu, const char *label, int *value, const char *const *choices, int count);
/* Draw menu text with the app's font instead of SDL's debug font. Pass NULLs to go back. */
void hk_menu_set_text_renderer(HkMenu *menu, HkMenuDrawText draw_text, HkMenuTextWidth text_width, void *user);
void hk_menu_open(HkMenu *menu);
HkMenuEvent hk_menu_update(HkMenu *menu, const HkInput *input);
/* Restores the renderer's draw color, blend mode and scale afterwards. */
void hk_menu_draw(const HkMenu *menu, SDL_Renderer *renderer);

#ifdef __cplusplus
}
#endif
#endif
