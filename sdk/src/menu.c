#include <handkit/menu.h>

/* Rows are: Continue, the app's items, Quit. */
static int row_count(const HkMenu *menu) {
    return menu->item_count + 2;
}

void hk_menu_init(HkMenu *menu, const char *title) {
    SDL_zero(*menu);
    menu->title = title;
    menu->activated = -1;
}

static int add_item(HkMenu *menu, const char *label, bool *toggle) {
    if (menu->item_count >= HK_MENU_MAX_ITEMS) {
        SDL_SetError("Menu holds at most %d items", HK_MENU_MAX_ITEMS);
        return -1;
    }
    menu->items[menu->item_count] = (HkMenuItem){.label = label, .toggle = toggle};
    return menu->item_count++;
}

int hk_menu_add_toggle(HkMenu *menu, const char *label, bool *value) {
    if (!value) {
        SDL_SetError("A menu toggle needs a value");
        return -1;
    }
    return add_item(menu, label, value);
}

int hk_menu_add_action(HkMenu *menu, const char *label) {
    return add_item(menu, label, NULL);
}

int hk_menu_add_slider(HkMenu *menu, const char *label, int *value, int minimum, int maximum, int step) {
    if (!value || minimum >= maximum || step <= 0) {
        SDL_SetError("A menu slider needs a value and a valid range");
        return -1;
    }
    int id = add_item(menu, label, NULL);
    if (id >= 0) {
        menu->items[id].slider = value;
        menu->items[id].minimum = minimum;
        menu->items[id].maximum = maximum;
        menu->items[id].step = step;
    }
    return id;
}

int hk_menu_add_choice(HkMenu *menu, const char *label, int *value, const char *const *choices, int count) {
    if (!value || !choices || count < 2) {
        SDL_SetError("A menu choice needs a value and at least two options");
        return -1;
    }
    int id = add_item(menu, label, NULL);
    if (id >= 0) {
        menu->items[id].choice = value;
        menu->items[id].choices = choices;
        menu->items[id].choice_count = count;
    }
    return id;
}

void hk_menu_set_text_renderer(HkMenu *menu, HkMenuDrawText draw_text, HkMenuTextWidth text_width, void *user) {
    menu->draw_text = draw_text;
    menu->text_width = text_width;
    menu->text_user = user;
}

void hk_menu_open(HkMenu *menu) {
    menu->open = true;
    menu->selected = 0;
    menu->activated = -1;
}

static HkMenuEvent close_with(HkMenu *menu, HkMenuEvent event) {
    menu->open = false;
    return event;
}

HkMenuEvent hk_menu_update(HkMenu *menu, const HkInput *input) {
    /* Track the hat even while closed so a direction held when opening is not a press. */
    Uint8 dpad_pressed = input->dpad & ~menu->previous_dpad;
    menu->previous_dpad = input->dpad;
    if (!menu->open) {
        if (input->pressed[HK_BUTTON_MENU]) {
            hk_menu_open(menu);
            return HK_MENU_EVENT_OPENED;
        }
        return HK_MENU_EVENT_NONE;
    }
    if (input->pressed[HK_BUTTON_MENU] || input->pressed[HK_BUTTON_B]) {
        return close_with(menu, HK_MENU_EVENT_CLOSED);
    }
    int rows = row_count(menu);
    if (dpad_pressed & SDL_HAT_UP) {
        menu->selected = (menu->selected + rows - 1) % rows;
    }
    if (dpad_pressed & SDL_HAT_DOWN) {
        menu->selected = (menu->selected + 1) % rows;
    }
    bool accept = input->pressed[HK_BUTTON_A];
    if (menu->selected == 0) {
        return accept ? close_with(menu, HK_MENU_EVENT_CLOSED) : HK_MENU_EVENT_NONE;
    }
    if (menu->selected == rows - 1) {
        return accept ? close_with(menu, HK_MENU_EVENT_QUIT) : HK_MENU_EVENT_NONE;
    }
    int id = menu->selected - 1;
    HkMenuItem *item = &menu->items[id];
    if (item->slider && (dpad_pressed & (SDL_HAT_LEFT | SDL_HAT_RIGHT))) {
        int direction = (dpad_pressed & SDL_HAT_RIGHT) ? 1 : -1;
        int value = SDL_clamp(*item->slider + direction * item->step, item->minimum, item->maximum);
        if (value == *item->slider) return HK_MENU_EVENT_NONE;
        *item->slider = value;
        menu->activated = id;
        return HK_MENU_EVENT_CHANGED;
    }
    if (item->choice && (accept || (dpad_pressed & (SDL_HAT_LEFT | SDL_HAT_RIGHT)))) {
        /* A and RIGHT step forward, LEFT back, wrapping both ways. */
        int direction = (dpad_pressed & SDL_HAT_LEFT) && !accept ? -1 : 1;
        *item->choice = (SDL_clamp(*item->choice, 0, item->choice_count - 1) + direction + item->choice_count) % item->choice_count;
        menu->activated = id;
        return HK_MENU_EVENT_CHANGED;
    }
    if (item->toggle && (accept || (dpad_pressed & (SDL_HAT_LEFT | SDL_HAT_RIGHT)))) {
        *item->toggle = !*item->toggle;
        menu->activated = id;
        return HK_MENU_EVENT_CHANGED;
    }
    if (!item->toggle && !item->slider && !item->choice && accept) {
        menu->activated = id;
        return close_with(menu, HK_MENU_EVENT_ACTION);
    }
    return HK_MENU_EVENT_NONE;
}

static void draw_text(const HkMenu *menu, SDL_Renderer *renderer, float x, float y, const char *text, float size, SDL_Color color) {
    if (menu->draw_text && menu->text_width) {
        menu->draw_text(renderer, x, y, text, size, color, menu->text_user);
        SDL_SetRenderScale(renderer, 1, 1);
        return;
    }
    float s = size / SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE;
    SDL_SetRenderScale(renderer, s, s);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderDebugText(renderer, x / s, y / s, text);
    SDL_SetRenderScale(renderer, 1, 1);
}

static float text_width(const HkMenu *menu, const char *text, float size) {
    if (menu->draw_text && menu->text_width) {
        return menu->text_width(text, size, menu->text_user);
    }
    return (float)SDL_strlen(text) * size;
}

/* A rectangle with fully rounded ends, drawn as a triangle fan. */
static void fill_pill(SDL_Renderer *renderer, float x, float y, float w, float h, SDL_Color color) {
    enum { ARC = 12, POINTS = 2 * (ARC + 1) };
    float r = h * 0.5f;
    if (w < h) {
        w = h;
    }
    SDL_FColor c = {color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f};
    SDL_Vertex v[POINTS + 1];
    int indices[POINTS * 3];
    v[0] = (SDL_Vertex){{x + w * 0.5f, y + r}, c, {0, 0}};
    for (int i = 0; i <= ARC; i++) {
        float a = SDL_PI_F * 0.5f + SDL_PI_F * i / ARC; /* left end, top to bottom */
        v[1 + i] = (SDL_Vertex){{x + r + SDL_cosf(a) * r, y + r - SDL_sinf(a) * r}, c, {0, 0}};
        a = -SDL_PI_F * 0.5f + SDL_PI_F * i / ARC; /* right end, bottom to top */
        v[2 + ARC + i] = (SDL_Vertex){{x + w - r + SDL_cosf(a) * r, y + r - SDL_sinf(a) * r}, c, {0, 0}};
    }
    for (int i = 0; i < POINTS; i++) {
        indices[i * 3] = 0;
        indices[i * 3 + 1] = 1 + i;
        indices[i * 3 + 2] = 1 + (i + 1) % POINTS;
    }
    SDL_RenderGeometry(renderer, NULL, v, POINTS + 1, indices, POINTS * 3);
}

void hk_menu_draw(const HkMenu *menu, SDL_Renderer *renderer) {
    if (!menu->open) {
        return;
    }
    Uint8 old_r, old_g, old_b, old_a;
    SDL_BlendMode old_blend;
    float old_scale_x, old_scale_y;
    SDL_GetRenderDrawColor(renderer, &old_r, &old_g, &old_b, &old_a);
    SDL_GetRenderDrawBlendMode(renderer, &old_blend);
    SDL_GetRenderScale(renderer, &old_scale_x, &old_scale_y);
    SDL_SetRenderScale(renderer, 1, 1);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    int width, height;
    SDL_GetRenderOutputSize(renderer, &width, &height);
    float ui = SDL_clamp(height / 480.0f, 0.8f, 2.5f);
    const SDL_Color white = {255, 255, 255, 255}, dark = {20, 20, 20, 255}, dim = {150, 150, 150, 255};

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200);
    SDL_RenderFillRect(renderer, NULL);

    float margin = 20 * ui;
    float text_size = 14 * ui;
    float pill_height = 28 * ui;
    float pad = 12 * ui;
    float row_height = 34 * ui;
    if (menu->title) {
        draw_text(menu, renderer, margin, margin, menu->title, 16 * ui, white);
    }

    int rows = row_count(menu);
    float list_width = SDL_min(width - 2 * margin, 420 * ui);
    float list_x = margin;
    float list_y = SDL_max(margin + 40 * ui, (height - rows * row_height) * 0.5f);
    for (int row = 0; row < rows; row++) {
        const HkMenuItem *item = row > 0 && row < rows - 1 ? &menu->items[row - 1] : NULL;
        const char *label = row == 0 ? "Continue" : row == rows - 1 ? "Quit" : item->label;
        char slider_value[16];
        const char *value = item && item->toggle ? (*item->toggle ? "On" : "Off") : NULL;
        if (item && item->slider) {
            SDL_snprintf(slider_value, sizeof(slider_value), "%d%%", *item->slider);
            value = slider_value;
        }
        if (item && item->choice) {
            value = item->choices[SDL_clamp(*item->choice, 0, item->choice_count - 1)];
        }
        bool selected = row == menu->selected;
        float y = list_y + row * row_height;
        float text_y = y + (pill_height - text_size) * 0.5f;
        if (selected) {
            float w = value ? list_width : text_width(menu, label, text_size) + 2 * pad;
            fill_pill(renderer, list_x, y, w, pill_height, white);
        }
        draw_text(menu, renderer, list_x + pad, text_y, label, text_size, selected ? dark : white);
        if (value) {
            float value_x = list_x + list_width - pad - text_width(menu, value, text_size);
            draw_text(menu, renderer, value_x, text_y, value, text_size, selected ? dark : dim);
        }
        if (item && item->slider) {
            float bar_width = 100 * ui;
            float bar_x = list_x + list_width - pad - 48 * ui - bar_width;
            float fraction = SDL_clamp((float)(*item->slider - item->minimum) /
                                       (item->maximum - item->minimum), 0.0f, 1.0f);
            SDL_Color bar = selected ? dark : dim;
            SDL_SetRenderDrawColor(renderer, bar.r, bar.g, bar.b, 110);
            SDL_RenderFillRect(renderer, &(SDL_FRect){bar_x, y + 12 * ui, bar_width, 4 * ui});
            SDL_SetRenderDrawColor(renderer, bar.r, bar.g, bar.b, 255);
            SDL_RenderFillRect(renderer, &(SDL_FRect){bar_x, y + 12 * ui, bar_width * fraction, 4 * ui});
        }
    }

    const char *hint = "LEFT/RIGHT ADJUST   B BACK";
    float hint_size = 11 * ui;
    draw_text(menu, renderer, width - margin - text_width(menu, hint, hint_size), height - margin - hint_size, hint, hint_size, dim);

    SDL_SetRenderScale(renderer, old_scale_x, old_scale_y);
    SDL_SetRenderDrawBlendMode(renderer, old_blend);
    SDL_SetRenderDrawColor(renderer, old_r, old_g, old_b, old_a);
}
