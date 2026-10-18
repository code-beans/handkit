#include <handkit/feedback.h>
#include <fcntl.h>
#include <unistd.h>

#define LED_ANIM_DIR "/sys/class/led_anim/"

enum { ZONE_COUNT = 4 };
static const char *const zone_names[ZONE_COUNT] = {"lr", "m", "f1", "f2"};
/* Color, duration and cycles must be written first; writing effect_<zone> starts the animation. */
static const char *const led_settings[] = {"effect_rgb_hex_", "effect_duration_", "effect_cycles_", "effect_"};
enum { SETTING_COUNT = SDL_arraysize(led_settings) };

typedef struct LedState {
    Uint32 rgb;
    int effect;
    int duration_ms;
    bool valid;
} LedState;

static LedState led_current[ZONE_COUNT];
static char led_saved[ZONE_COUNT][SETTING_COUNT][32];
static bool led_has_saved;

static SDL_Mutex *rumble_mutex;
static SDL_Condition *rumble_wake;
static SDL_Thread *rumble_thread;
static Uint64 rumble_until;
static float rumble_strength = 1.0f;
static bool rumble_quit;

static bool write_sysfs(const char *path, const char *value) {
    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        return SDL_SetError("Cannot open %s", path);
    }
    size_t length = SDL_strlen(value);
    bool ok = write(fd, value, length) == (ssize_t)length;
    close(fd);
    return ok ? true : SDL_SetError("Cannot write %s", path);
}

static void read_sysfs(const char *path, char *output, size_t capacity) {
    output[0] = 0;
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return;
    }
    ssize_t n = read(fd, output, capacity - 1);
    close(fd);
    output[n > 0 ? n : 0] = 0;
}

static void led_save(void) {
    char path[96];
    for (int z = 0; z < ZONE_COUNT; z++) {
        for (int s = 0; s < SETTING_COUNT; s++) {
            SDL_snprintf(path, sizeof path, LED_ANIM_DIR "%s%s", led_settings[s], zone_names[z]);
            read_sysfs(path, led_saved[z][s], sizeof led_saved[z][s]);
        }
    }
    led_has_saved = true;
}

bool hk_led_set(Uint32 zones, Uint32 rgb, HkLedEffect effect, int duration_ms) {
    if ((unsigned)effect > HK_LED_BLINK3 || duration_ms <= 0) {
        return SDL_SetError("Invalid LED effect or duration");
    }
    if (!led_has_saved) {
        led_save();
    }
    bool ok = true;
    for (int z = 0; z < ZONE_COUNT; z++) {
        LedState *current = &led_current[z];
        if (!(zones & (1u << z))) {
            continue;
        }
        if (current->valid && current->rgb == rgb && current->effect == (int)effect && current->duration_ms == duration_ms) {
            continue;
        }
        char values[SETTING_COUNT][16];
        SDL_snprintf(values[0], sizeof values[0], "%06X ", (unsigned)(rgb & 0xFFFFFF));
        SDL_snprintf(values[1], sizeof values[1], "%d", duration_ms);
        SDL_snprintf(values[2], sizeof values[2], "-1");
        SDL_snprintf(values[3], sizeof values[3], "%d", (int)effect);
        bool zone_ok = true;
        for (int s = 0; s < SETTING_COUNT; s++) {
            char path[96];
            SDL_snprintf(path, sizeof path, LED_ANIM_DIR "%s%s", led_settings[s], zone_names[z]);
            zone_ok = write_sysfs(path, values[s]) && zone_ok;
        }
        *current = (LedState){rgb, (int)effect, duration_ms, zone_ok};
        ok = ok && zone_ok;
    }
    return ok;
}

void hk_led_restore(void) {
    if (!led_has_saved) {
        return;
    }
    char path[96];
    for (int z = 0; z < ZONE_COUNT; z++) {
        for (int s = 0; s < SETTING_COUNT; s++) {
            if (!led_saved[z][s][0]) {
                continue;
            }
            SDL_snprintf(path, sizeof path, LED_ANIM_DIR "%s%s", led_settings[s], zone_names[z]);
            write_sysfs(path, led_saved[z][s]);
        }
        led_current[z].valid = false;
    }
    led_has_saved = false;
}

/* The motor's on/off switch: a GPIO on the A133P handhelds, the power chip's motor
 * output on the H700 ones. */
static const char *const rumble_paths[] = {"/sys/class/gpio/gpio227/value", "/sys/class/power_supply/axp2202-battery/moto"};

static const char *rumble_path(void) {
    for (size_t i = 0; i < SDL_arraysize(rumble_paths); i++) {
        if (access(rumble_paths[i], W_OK) == 0) {
            return rumble_paths[i];
        }
    }
    return rumble_paths[0];
}

/* The motor is a plain on/off switch, so strengths below 1 are made with PWM: a
 * full-power kick gets the motor spinning, then a 10 ms on/off cycle with the
 * strength as its on share holds it at a lower speed. */
#define RUMBLE_PERIOD_NS (10 * SDL_NS_PER_MS)
#define RUMBLE_KICK_MS 30

static void set_motor(int fd, bool on) {
    if (fd < 0 || pwrite(fd, on ? "1" : "0", 1, 0) != 1) {
        write_sysfs(rumble_path(), on ? "1" : "0");
    }
}

static int SDLCALL rumble_thread_main(void *userdata) {
    (void)userdata;
    int fd = open(rumble_path(), O_WRONLY);
    Uint64 started = 0;
    SDL_LockMutex(rumble_mutex);
    while (!rumble_quit) {
        Uint64 now = SDL_GetTicks();
        if (now >= rumble_until) {
            set_motor(fd, false);
            started = 0;
            SDL_WaitCondition(rumble_wake, rumble_mutex);
            continue;
        }
        if (!started) {
            started = now;
        }
        float strength = rumble_strength;
        if (strength >= 1.0f || now < started + RUMBLE_KICK_MS) {
            Uint64 until = strength >= 1.0f ? rumble_until : SDL_min(rumble_until, started + RUMBLE_KICK_MS);
            set_motor(fd, true);
            SDL_WaitConditionTimeout(rumble_wake, rumble_mutex, (Sint32)(until - now));
            continue;
        }
        SDL_UnlockMutex(rumble_mutex);
        Uint64 on_ns = (Uint64)(RUMBLE_PERIOD_NS * strength);
        set_motor(fd, true);
        SDL_DelayNS(on_ns);
        set_motor(fd, false);
        SDL_DelayNS(RUMBLE_PERIOD_NS - on_ns);
        SDL_LockMutex(rumble_mutex);
    }
    set_motor(fd, false);
    SDL_UnlockMutex(rumble_mutex);
    if (fd >= 0) {
        close(fd);
    }
    return 0;
}

void hk_rumble_set_strength(float strength) {
    if (rumble_mutex) {
        SDL_LockMutex(rumble_mutex);
    }
    rumble_strength = SDL_clamp(strength, 0.0f, 1.0f);
    if (rumble_mutex) {
        SDL_UnlockMutex(rumble_mutex);
    }
}

bool hk_rumble(int ms) {
    if (ms <= 0) {
        return SDL_SetError("Rumble duration must be positive");
    }
    if (!rumble_mutex && !(rumble_mutex = SDL_CreateMutex())) {
        return false;
    }
    if (!rumble_wake && !(rumble_wake = SDL_CreateCondition())) {
        return false;
    }
    if (rumble_strength <= 0.0f) {
        return true;
    }
    if (!rumble_thread && !(rumble_thread = SDL_CreateThread(rumble_thread_main, "rumble", NULL))) {
        return false;
    }
    SDL_LockMutex(rumble_mutex);
    Uint64 until = SDL_GetTicks() + (Uint64)ms;
    if (until > rumble_until) {
        rumble_until = until;
    }
    SDL_SignalCondition(rumble_wake);
    SDL_UnlockMutex(rumble_mutex);
    return true;
}

void hk_rumble_stop(void) {
    if (rumble_thread) {
        SDL_LockMutex(rumble_mutex);
        rumble_quit = true;
        rumble_until = 0;
        SDL_SignalCondition(rumble_wake);
        SDL_UnlockMutex(rumble_mutex);
        SDL_WaitThread(rumble_thread, NULL);
        rumble_thread = NULL;
        rumble_quit = false;
    }
    write_sysfs(rumble_path(), "0");
}
