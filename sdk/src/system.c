#include <handkit/system.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

/* Timings follow NextUI's PWR_update(). */
#define POWER_OFF_MS 1000
#define RESUME_IGNORE_MS 1000
#define DEEP_SLEEP_MS 120000
#define OVERLAY_MS 1000
#define MAX_POWER_KEYS 8 /* /dev/input devices that report KEY_POWER */

/* The levels live in NextUI's libmsettings, shared with keymon. */
static struct {
    int (*get_volume)(void);     /* 0-20 */
    int (*get_brightness)(void); /* 0-10 */
    void (*set_volume)(int);
    void (*set_brightness)(int);
    void (*set_raw_brightness)(int);
} msettings;
static bool have_msettings;

static int power_keys[MAX_POWER_KEYS];
static int power_key_count;
static Uint64 power_pressed_at; /* 0 while the key is up */
static Uint64 resumed_at;
static bool sleep_pending; /* the app has seen the background events; sleep on the next update */
static bool power_off;     /* checked by hk_system_quit() */

typedef enum Overlay { OVERLAY_NONE, OVERLAY_VOLUME, OVERLAY_BRIGHTNESS } Overlay;
static Overlay overlay;
static Uint64 overlay_until;
static int shown_volume = -1; /* catches changes made without the keys, like the mute switch */
static bool overlay_dirty;
static HkMenuDrawText overlay_draw_text;
static HkMenuTextWidth overlay_text_width;
static void *overlay_text_user;

/* Loaded at run time: the library only exists on the device, in NextUI's LD_LIBRARY_PATH. */
static void load_msettings(void) {
    void *lib = dlopen("libmsettings.so", RTLD_NOW);
    if (!lib) {
        fprintf(stderr, "No volume overlay: %s\n", dlerror());
        return;
    }
    void (*init)(void) = (void (*)(void))dlsym(lib, "InitSettings");
    msettings.get_volume = (int (*)(void))dlsym(lib, "GetVolume");
    msettings.get_brightness = (int (*)(void))dlsym(lib, "GetBrightness");
    msettings.set_volume = (void (*)(int))dlsym(lib, "SetVolume");
    msettings.set_brightness = (void (*)(int))dlsym(lib, "SetBrightness");
    msettings.set_raw_brightness = (void (*)(int))dlsym(lib, "SetRawBrightness");
    if (!init || !msettings.get_volume || !msettings.get_brightness || !msettings.set_volume ||
        !msettings.set_brightness || !msettings.set_raw_brightness) {
        fprintf(stderr, "No volume overlay: libmsettings is missing functions\n");
        dlclose(lib);
        return;
    }
    /* Maps the settings keymon already shares; every NextUI app calls this at startup. */
    init();
    have_msettings = true;
    shown_volume = msettings.get_volume();
}

/* The power key is its own input device, not part of the gamepad SDL sees. The devices are
 * opened without grabbing them, so keymon still gets the volume keys. */
static void open_power_keys(void) {
    const size_t bits = 8 * sizeof(long);
    for (int i = 0; i < 16 && power_key_count < MAX_POWER_KEYS; i++) {
        char path[32];
        snprintf(path, sizeof(path), "/dev/input/event%d", i);
        int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) {
            continue;
        }
        /* Bit mask of the keys this device can report. */
        unsigned long keys[KEY_MAX / (8 * sizeof(long)) + 1] = {0};
        if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keys)), keys) >= 0 && (keys[KEY_POWER / bits] >> (KEY_POWER % bits) & 1)) {
            power_keys[power_key_count++] = fd;
        } else {
            close(fd);
        }
    }
}

void hk_system_init(void) {
    static bool loaded;
    if (!loaded) {
        load_msettings();
        loaded = true;
    }
    if (!power_key_count) {
        open_power_keys();
    }
}

/* Drains queued power key events, noting whether it went down or came up. */
static void read_power_key(bool *down, bool *up) {
    *down = *up = false;
    struct input_event event;
    for (int i = 0; i < power_key_count; i++) {
        while (read(power_keys[i], &event, sizeof(event)) == sizeof(event)) {
            if (event.type != EV_KEY || event.code != KEY_POWER) {
                continue;
            }
            if (event.value == 1) {
                *down = true;
            }
            if (event.value == 0) {
                *up = true;
            }
        }
    }
}

/* NextUI's hybrid sleep: screen and lights off and its daemons paused until power is pressed
 * again, suspending to RAM after a while. */
static void sleep_until_power(void) {
    hk_rumble_stop();
    hk_led_set(HK_LED_ALL, 0x000000, HK_LED_OFF, 300);
    if (have_msettings) {
        msettings.set_raw_brightness(0);
    }
    /* Paused so volume presses or battery checks don't act while the screen is off. */
    system("killall -STOP keymon.elf batmon.elf audiomon.elf 2>/dev/null");
    /* Saves reach the SD card in case the battery runs out asleep. */
    sync();
    char suspend[256];
    const char *system_path = getenv("SYSTEM_PATH");
    snprintf(suspend, sizeof(suspend), "%s/bin/suspend", system_path ? system_path : "/mnt/SDCARD/.system/tg5040");
    Uint64 asleep_at = SDL_GetTicks();
    for (;;) {
        bool down, up;
        read_power_key(&down, &up);
        if (up) {
            break;
        }
        SDL_Delay(50);
        if (SDL_GetTicks() - asleep_at >= DEEP_SLEEP_MS) {
            /* Returns once the power button wakes the device. */
            if (system(suspend) == 0) {
                break;
            }
            asleep_at = SDL_GetTicks();
        }
    }
    system("killall -CONT keymon.elf batmon.elf audiomon.elf 2>/dev/null");
    if (have_msettings) {
        /* The same order as NextUI: the Brick's backlight needs a raw value before the
         * scaled one, and setting the volume reapplies it to the mixer. */
        msettings.set_raw_brightness(8);
        msettings.set_brightness(msettings.get_brightness());
        msettings.set_volume(msettings.get_volume());
    }
    /* Drop the press that woke the device. */
    bool down, up;
    read_power_key(&down, &up);
    power_pressed_at = 0;
    resumed_at = SDL_GetTicks();
}

/* keymon changes the level; this only decides what to show. */
static void update_overlay(const HkInput *input, Uint64 now) {
    if (!have_msettings) {
        return;
    }
    bool was_shown = overlay != OVERLAY_NONE;
    int volume = msettings.get_volume();
    bool keys = input->held[HK_BUTTON_VOLUME_UP] || input->held[HK_BUTTON_VOLUME_DOWN];
    if (keys || volume != shown_volume) {
        /* keymon treats MENU + volume as brightness. */
        overlay = keys && input->held[HK_BUTTON_MENU] ? OVERLAY_BRIGHTNESS : OVERLAY_VOLUME;
        overlay_until = now + OVERLAY_MS;
        shown_volume = volume;
    }
    if (overlay != OVERLAY_NONE && now >= overlay_until) {
        overlay = OVERLAY_NONE;
    }
    overlay_dirty = was_shown || overlay != OVERLAY_NONE;
}

static void push_event(SDL_EventType type) {
    SDL_PushEvent(&(SDL_Event){.type = type});
}

HkSystemEvent hk_system_update(const HkInput *input) {
    if (sleep_pending) {
        sleep_pending = false;
        sleep_until_power();
        push_event(SDL_EVENT_WILL_ENTER_FOREGROUND);
        push_event(SDL_EVENT_DID_ENTER_FOREGROUND);
        return HK_SYSTEM_EVENT_WAKE;
    }
    Uint64 now = SDL_GetTicks();
    update_overlay(input, now);
    /* Already quitting; SDL_EVENT_QUIT went out when power was held. */
    if (power_off) {
        return HK_SYSTEM_EVENT_POWER_OFF;
    }
    /* NextUI's exit chord, reported once as the second button goes down. */
    if (input->held[HK_BUTTON_START] && input->held[HK_BUTTON_SELECT] &&
        (input->pressed[HK_BUTTON_START] || input->pressed[HK_BUTTON_SELECT])) {
        push_event(SDL_EVENT_QUIT);
        return HK_SYSTEM_EVENT_QUIT;
    }
    bool down, up;
    read_power_key(&down, &up);
    /* Waking takes a press too; ignore the bounce right after it. */
    if (down && now - resumed_at >= RESUME_IGNORE_MS) {
        power_pressed_at = now;
    }
    if (power_pressed_at && now - power_pressed_at >= POWER_OFF_MS) {
        power_off = true;
        push_event(SDL_EVENT_QUIT);
        return HK_SYSTEM_EVENT_POWER_OFF;
    }
    if (up && power_pressed_at) {
        /* The app pauses on these events; the next update sleeps. */
        power_pressed_at = 0;
        sleep_pending = true;
        push_event(SDL_EVENT_WILL_ENTER_BACKGROUND);
        push_event(SDL_EVENT_DID_ENTER_BACKGROUND);
        return HK_SYSTEM_EVENT_SLEEP;
    }
    return HK_SYSTEM_EVENT_NONE;
}

bool hk_system_overlay_dirty(void) {
    return overlay_dirty;
}

void hk_system_set_text_renderer(HkMenuDrawText draw_text, HkMenuTextWidth text_width, void *user) {
    overlay_draw_text = draw_text;
    overlay_text_width = text_width;
    overlay_text_user = user;
}

static void draw_text(SDL_Renderer *renderer, float x, float y, const char *text, float size, SDL_Color color) {
    if (overlay_draw_text && overlay_text_width) {
        overlay_draw_text(renderer, x, y, text, size, color, overlay_text_user);
        SDL_SetRenderScale(renderer, 1, 1);
        return;
    }
    float s = size / SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE;
    SDL_SetRenderScale(renderer, s, s);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderDebugText(renderer, x / s, y / s, text);
    SDL_SetRenderScale(renderer, 1, 1);
}

static float text_width(const char *text, float size) {
    if (overlay_draw_text && overlay_text_width) {
        return overlay_text_width(text, size, overlay_text_user);
    }
    return (float)SDL_strlen(text) * size;
}

/* A rectangle with fully rounded ends, drawn as a triangle fan. Same as the menu's. */
static void fill_pill(SDL_Renderer *renderer, float x, float y, float w, float h, SDL_Color color) {
    enum { ARC = 12, POINTS = 2 * (ARC + 1) };
    float r = h * 0.5f;
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

void hk_system_draw(SDL_Renderer *renderer) {
    if (overlay == OVERLAY_NONE) {
        return;
    }
    bool brightness = overlay == OVERLAY_BRIGHTNESS;
    int level = brightness ? msettings.get_brightness() : msettings.get_volume();
    float fraction = SDL_clamp(level / (brightness ? 10.0f : 20.0f), 0.0f, 1.0f);
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
    /* Scaled like the menu, so the two match. */
    float ui = SDL_clamp(height / 480.0f, 0.8f, 2.5f);
    float h = 28 * ui, pad = 12 * ui, text_size = 14 * ui, bar_width = 100 * ui, bar_height = 4 * ui;
    const char *label = brightness ? "BRIGHTNESS" : level == 0 ? "MUTED" : "VOLUME";
    /* Sized for the longest label, so the pill keeps its width when the label changes. */
    float label_width = text_width("BRIGHTNESS", text_size) + pad;
    float w = pad + label_width + bar_width + pad;
    float x = width - 20 * ui - w, y = 20 * ui;
    fill_pill(renderer, x, y, w, h, (SDL_Color){0, 0, 0, 200});
    draw_text(renderer, x + pad, y + (h - text_size) * 0.5f, label, text_size, (SDL_Color){255, 255, 255, 255});
    float bar_x = x + pad + label_width, bar_y = y + (h - bar_height) * 0.5f;
    SDL_SetRenderDrawColor(renderer, 150, 150, 150, 110);
    SDL_RenderFillRect(renderer, &(SDL_FRect){bar_x, bar_y, bar_width, bar_height});
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderFillRect(renderer, &(SDL_FRect){bar_x, bar_y, bar_width * fraction, bar_height});

    SDL_SetRenderScale(renderer, old_scale_x, old_scale_y);
    SDL_SetRenderDrawBlendMode(renderer, old_blend);
    SDL_SetRenderDrawColor(renderer, old_r, old_g, old_b, old_a);
}

void hk_system_quit(void) {
    for (int i = 0; i < power_key_count; i++) {
        close(power_keys[i]);
    }
    power_key_count = 0;
    if (!power_off) {
        return;
    }
    /* The screen goes dark right away so the press feels answered. NextUI's launch loop then
     * shuts the device down when it finds this file. */
    if (have_msettings) {
        msettings.set_raw_brightness(0);
    }
    FILE *flag = fopen("/tmp/poweroff", "w");
    if (flag) {
        fclose(flag);
    }
    sync();
}
