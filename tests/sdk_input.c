#include <handkit/runtime.h>
#include <stdio.h>
#include <string.h>
#define CHECK(expr) \
    do { \
        if (!(expr)) { \
            fprintf(stderr, "Failed %s:%d: %s (%s)\n", __FILE__, __LINE__, #expr, SDL_GetError()); \
            return 1; \
        } \
    } while (0)
static void pump(void) {
    SDL_UpdateJoysticks();
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
    }
}

int main(void) {
    CHECK(SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD));
    CHECK(!strcmp(hk_button_name(HK_BUTTON_L1), "L1"));
    CHECK(!strcmp(hk_button_name((HkButton)999), "Unknown"));
    char tiny[1];
    CHECK(!hk_asset_path(tiny, sizeof tiny, "image.bmp"));
    SDL_VirtualJoystickDesc desc = {0};
    SDL_INIT_INTERFACE(&desc);
    desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.naxes = 6;
    desc.nbuttons = HK_BUTTON_COUNT;
    desc.nhats = 1;
    SDL_JoystickID id = SDL_AttachVirtualJoystick(&desc);
    CHECK(id);
    HkInput input;
    hk_input_init(&input);
    input.joystick = SDL_OpenJoystick(id);
    CHECK(input.joystick);
    CHECK(SDL_SetJoystickVirtualButton(input.joystick, HK_BUTTON_A, true));
    pump();
    hk_input_update(&input);
    CHECK(input.held[HK_BUTTON_A] && input.pressed[HK_BUTTON_A] && !input.released[HK_BUTTON_A]);
    hk_input_update(&input);
    CHECK(input.held[HK_BUTTON_A] && !input.pressed[HK_BUTTON_A]);
    CHECK(SDL_SetJoystickVirtualButton(input.joystick, HK_BUTTON_A, false));
    pump();
    hk_input_update(&input);
    CHECK(!input.held[HK_BUTTON_A] && input.released[HK_BUTTON_A]);
    CHECK(SDL_SetJoystickVirtualAxis(input.joystick, 0, 4000));
    pump();
    hk_input_update(&input);
    CHECK(input.left_x == 0);
    CHECK(SDL_SetJoystickVirtualAxis(input.joystick, 0, 16384));
    pump();
    hk_input_update(&input);
    CHECK(input.left_x == .5f);
    CHECK(SDL_DetachVirtualJoystick(id));
    pump();
    hk_input_update(&input);
    CHECK(!input.joystick || SDL_GetJoystickID(input.joystick) != id);
    hk_input_close(&input);

    /* The stock pad's mapping adds F1/F2 and Home to SDL's Xbox 360 one. */
    char *mapping = SDL_GetGamepadMappingForGUID(SDL_StringToGUID("0300a3845e0400008e02000014010000"));
    CHECK(mapping && SDL_strstr(mapping, "paddle2:b11") && SDL_strstr(mapping, "misc1:b15"));
    SDL_free(mapping);

    /* A mapped controller is read by button position, whatever its raw indices are. */
    SDL_VirtualJoystickDesc mapped = {0};
    SDL_INIT_INTERFACE(&mapped);
    mapped.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    mapped.naxes = SDL_GAMEPAD_AXIS_COUNT;
    mapped.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    mapped.button_mask = (1 << SDL_GAMEPAD_BUTTON_SOUTH) | (1 << SDL_GAMEPAD_BUTTON_EAST) | (1 << SDL_GAMEPAD_BUTTON_DPAD_UP) | (1 << SDL_GAMEPAD_BUTTON_DPAD_DOWN) |
                         (1 << SDL_GAMEPAD_BUTTON_DPAD_LEFT) | (1 << SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
    mapped.axis_mask = (1 << SDL_GAMEPAD_AXIS_LEFTX) | (1 << SDL_GAMEPAD_AXIS_LEFTY);
    id = SDL_AttachVirtualJoystick(&mapped);
    CHECK(id && SDL_IsGamepad(id));
    hk_input_init(&input);
    input.joystick = SDL_OpenJoystick(id);
    input.gamepad = SDL_OpenGamepad(id);
    CHECK(input.joystick && input.gamepad);
    /* Virtual gamepads number their buttons in SDL_GamepadButton order. */
    CHECK(SDL_SetJoystickVirtualButton(input.joystick, SDL_GAMEPAD_BUTTON_EAST, true));
    CHECK(SDL_SetJoystickVirtualButton(input.joystick, SDL_GAMEPAD_BUTTON_DPAD_UP, true));
    pump();
    hk_input_update(&input);
    CHECK(input.pressed[HK_BUTTON_A] && !input.held[HK_BUTTON_B]);
    CHECK(input.dpad == SDL_HAT_UP);
    CHECK(!input.held[HK_BUTTON_VOLUME_UP]);
    hk_input_close(&input);
    CHECK(SDL_DetachVirtualJoystick(id));
    SDL_Quit();
    puts("SDK input, transitions, deadzone, reconnect and gamepad mapping checks passed");
    return 0;
}
