#include <handkit/runtime.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

bool hk_init(HkBackend backend) {
    if (backend < HK_BACKEND_SOFTWARE || backend > HK_BACKEND_VULKAN) {
        return SDL_SetError("Invalid Handkit backend");
    }
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, backend == HK_BACKEND_SOFTWARE ? "dummy" : "handkit");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    return SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD);
}

bool hk_display_size(int *width, int *height) {
    if (!width || !height) {
        return SDL_SetError("Display size requires two output pointers");
    }
    struct fb_var_screeninfo info;
    int fd = open("/dev/fb0", O_RDONLY);
    if (fd < 0) {
        return SDL_SetError("Cannot open /dev/fb0");
    }
    int result = ioctl(fd, FBIOGET_VSCREENINFO, &info);
    close(fd);
    if (result < 0) {
        return SDL_SetError("Cannot query /dev/fb0");
    }
    *width = info.xres;
    *height = info.yres;
    return true;
}

bool hk_create_gles_renderer(SDL_Window **window, SDL_Renderer **renderer) {
    if (!window || !renderer) {
        return SDL_SetError("Window and renderer outputs are required");
    }
    *window = NULL;
    *renderer = NULL;
    int width;
    int height;
    if (!hk_display_size(&width, &height)) {
        return false;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
    *window = SDL_CreateWindow("Handkit application", width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN);
    if (!*window) {
        return false;
    }
    *renderer = SDL_CreateRenderer(*window, "opengles2");
    if (!*renderer) {
        SDL_DestroyWindow(*window);
        *window = NULL;
        return false;
    }
    SDL_SetRenderVSync(*renderer, 1);
    return true;
}

bool hk_asset_path(char *output, size_t capacity, const char *relative) {
    const char *base = SDL_GetBasePath();
    if (!output || !capacity || !relative || !base) {
        return SDL_SetError("Invalid asset path arguments");
    }
    int n = SDL_snprintf(output, capacity, "%s%s", base, relative);
    if (n < 0 || (size_t)n >= capacity) {
        return SDL_SetError("Asset path is too long");
    }
    return true;
}

const char *hk_button_name(HkButton button) {
    static const char *const names[] = {"B", "A", "Y", "X", "L1", "R1", "Select", "Start", "Menu", "L3", "R3", "F1", "F2", "Vol-", "Vol+", "Home"};
    return (unsigned)button < HK_BUTTON_COUNT ? names[button] : "Unknown";
}

/* trimui_inputd's "TRIMUI Player1" pad reports itself as an Xbox 360 controller, so SDL already
 * maps it; this adds F1/F2 and Home. A mapping from SDL_GAMECONTROLLERCONFIG (muOS, PortMaster)
 * has a higher priority in SDL and still wins. */
#define TRIMUI_PAD_GUID "0300a3845e0400008e02000014010000"
static const char trimui_pad_mapping[] = TRIMUI_PAD_GUID ",TRIMUI Player1,a:b0,b:b1,x:b2,y:b3,back:b6,guide:b8,start:b7,leftstick:b9,rightstick:b10,"
                                                         "leftshoulder:b4,rightshoulder:b5,dpup:h0.1,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,leftx:a0,lefty:a1,"
                                                         "rightx:a3,righty:a4,lefttrigger:a2,righttrigger:a5,paddle2:b11,paddle1:b12,misc1:b15,";

/* Built-in controllers SDL has no mapping for. They are matched by name, since their GUIDs
 * depend on the kernel; a firmware mapping from SDL_GAMECONTROLLERCONFIG still wins. */
static const struct {
    const char *name;
    const char *mapping;
} named_mappings[] = {
    /* Anbernic H700 stock kernel. Numbering from MinUI's rg35xxplus platform; not yet checked on a device. */
    {"Deeplay-keys", "a:b1,b:b0,x:b2,y:b3,leftshoulder:b4,rightshoulder:b5,back:b6,start:b7,guide:b8,lefttrigger:b9,righttrigger:b10,"
                     "dpup:b13,dpleft:b14,dpright:b15,dpdown:b16,leftx:a0,lefty:a1,rightx:a2,righty:a3,"},
};

static void add_named_mapping(SDL_JoystickID id) {
    const char *name = SDL_GetJoystickNameForID(id);
    if (!name || SDL_IsGamepad(id)) {
        return;
    }
    for (size_t i = 0; i < SDL_arraysize(named_mappings); i++) {
        if (!SDL_strcmp(name, named_mappings[i].name)) {
            char guid[33];
            char *mapping = NULL;
            SDL_GUIDToString(SDL_GetJoystickGUIDForID(id), guid, sizeof guid);
            if (SDL_asprintf(&mapping, "%s,%s,%s", guid, name, named_mappings[i].mapping) > 0) {
                SDL_AddGamepadMapping(mapping);
                SDL_free(mapping);
            }
            return;
        }
    }
}

/* Buttons are named as printed on the handheld: A is the right face button, B the bottom one. */
static const SDL_GamepadButton gamepad_buttons[HK_BUTTON_COUNT] = {
    [HK_BUTTON_B] = SDL_GAMEPAD_BUTTON_SOUTH,
    [HK_BUTTON_A] = SDL_GAMEPAD_BUTTON_EAST,
    [HK_BUTTON_Y] = SDL_GAMEPAD_BUTTON_WEST,
    [HK_BUTTON_X] = SDL_GAMEPAD_BUTTON_NORTH,
    [HK_BUTTON_L1] = SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,
    [HK_BUTTON_R1] = SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
    [HK_BUTTON_SELECT] = SDL_GAMEPAD_BUTTON_BACK,
    [HK_BUTTON_START] = SDL_GAMEPAD_BUTTON_START,
    [HK_BUTTON_MENU] = SDL_GAMEPAD_BUTTON_GUIDE,
    [HK_BUTTON_L3] = SDL_GAMEPAD_BUTTON_LEFT_STICK,
    [HK_BUTTON_R3] = SDL_GAMEPAD_BUTTON_RIGHT_STICK,
    [HK_BUTTON_F1] = SDL_GAMEPAD_BUTTON_LEFT_PADDLE1,
    [HK_BUTTON_F2] = SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1,
    [HK_BUTTON_VOLUME_DOWN] = SDL_GAMEPAD_BUTTON_INVALID,
    [HK_BUTTON_VOLUME_UP] = SDL_GAMEPAD_BUTTON_INVALID,
    [HK_BUTTON_HOME] = SDL_GAMEPAD_BUTTON_MISC1,
};

void hk_input_init(HkInput *input) {
    SDL_zero(*input);
    SDL_AddGamepadMapping(trimui_pad_mapping);
}

void hk_input_close(HkInput *input) {
    if (input->gamepad) {
        SDL_CloseGamepad(input->gamepad);
    }
    if (input->joystick) {
        SDL_CloseJoystick(input->joystick);
    }
    SDL_zero(*input);
}

static float deadzone(Sint16 value) {
    return SDL_abs(value) > 8000 ? value / 32768.0f : 0;
}

static float read_axis(SDL_Joystick *joystick, int axis_index) {
    if (SDL_GetNumJoystickAxes(joystick) <= axis_index) {
        return 0;
    }
    return deadzone(SDL_GetJoystickAxis(joystick, axis_index));
}

/* The volume keys have no gamepad equivalent; only the stock pad reports them, as raw buttons. */
static bool is_trimui_pad(SDL_Joystick *joystick) {
    char guid[33];
    SDL_GUIDToString(SDL_GetJoystickGUID(joystick), guid, sizeof guid);
    return !SDL_strcmp(guid, TRIMUI_PAD_GUID);
}

static bool read_button(const HkInput *input, int button) {
    if (input->gamepad && gamepad_buttons[button] != SDL_GAMEPAD_BUTTON_INVALID) {
        return SDL_GetGamepadButton(input->gamepad, gamepad_buttons[button]);
    }
    if (input->gamepad && !is_trimui_pad(input->joystick)) {
        return false;
    }
    return button < SDL_GetNumJoystickButtons(input->joystick) && SDL_GetJoystickButton(input->joystick, button);
}

static Uint8 gamepad_dpad(SDL_Gamepad *gamepad) {
    return (SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_UP) ? SDL_HAT_UP : 0) | (SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT) ? SDL_HAT_RIGHT : 0) |
           (SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_DOWN) ? SDL_HAT_DOWN : 0) | (SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_LEFT) ? SDL_HAT_LEFT : 0);
}

void hk_input_update(HkInput *input) {
    bool previous[HK_BUTTON_COUNT];
    SDL_memcpy(previous, input->held, sizeof previous);
    if (input->joystick && !SDL_JoystickConnected(input->joystick)) {
        hk_input_close(input);
    }
    if (!input->joystick) {
        int count = 0;
        SDL_JoystickID *ids = SDL_GetJoysticks(&count);
        if (count) {
            /* Added again here in case hk_input_init ran before SDL_Init. */
            SDL_AddGamepadMapping(trimui_pad_mapping);
            add_named_mapping(ids[0]);
            input->joystick = SDL_OpenJoystick(ids[0]);
        }
        if (input->joystick && SDL_IsGamepad(ids[0])) {
            input->gamepad = SDL_OpenGamepad(ids[0]);
        }
        SDL_free(ids);
    }
    for (int i = 0; i < HK_BUTTON_COUNT; i++) {
        input->held[i] = input->joystick && read_button(input, i);
        input->pressed[i] = input->held[i] && !previous[i];
        input->released[i] = !input->held[i] && previous[i];
    }
    if (input->gamepad) {
        input->dpad = gamepad_dpad(input->gamepad);
        input->left_x = deadzone(SDL_GetGamepadAxis(input->gamepad, SDL_GAMEPAD_AXIS_LEFTX));
        input->left_y = deadzone(SDL_GetGamepadAxis(input->gamepad, SDL_GAMEPAD_AXIS_LEFTY));
        input->right_x = deadzone(SDL_GetGamepadAxis(input->gamepad, SDL_GAMEPAD_AXIS_RIGHTX));
        input->right_y = deadzone(SDL_GetGamepadAxis(input->gamepad, SDL_GAMEPAD_AXIS_RIGHTY));
        input->left_trigger = SDL_GetGamepadAxis(input->gamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) / 32767.0f;
        input->right_trigger = SDL_GetGamepadAxis(input->gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) / 32767.0f;
        return;
    }
    /* Without a mapping the buttons are read by their stock raw indices. */
    input->dpad = input->joystick && SDL_GetNumJoystickHats(input->joystick) ? SDL_GetJoystickHat(input->joystick, 0) : 0;
    input->left_x = input->joystick ? read_axis(input->joystick, 0) : 0;
    input->left_y = input->joystick ? read_axis(input->joystick, 1) : 0;
    input->right_x = input->joystick ? read_axis(input->joystick, 3) : 0;
    input->right_y = input->joystick ? read_axis(input->joystick, 4) : 0;
    input->left_trigger = 0;
    input->right_trigger = 0;
}
