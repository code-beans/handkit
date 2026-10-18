#ifndef HK_RUNTIME_H
#define HK_RUNTIME_H

#include <SDL3/SDL.h>
#include <handkit/input.h>
#include <handkit/feedback.h>
#ifdef __cplusplus
extern "C" {
#endif

#define HK_VERSION "0.4.0"
typedef enum HkBackend {
    HK_BACKEND_SOFTWARE,
    HK_BACKEND_GLES,
    HK_BACKEND_VULKAN
} HkBackend;

/* Errors are reported through SDL_GetError. Destroy windows/renderers before SDL_Quit. */
bool hk_init(HkBackend backend);
bool hk_display_size(int *width, int *height);
bool hk_create_gles_renderer(SDL_Window **window, SDL_Renderer **renderer);
bool hk_asset_path(char *output, size_t capacity, const char *relative_path);
const char *hk_button_name(HkButton button);
/* Call SDL_PollEvent normally, then hk_input_update once per frame.
 * The helper attaches/reconnects controllers and exposes transitions plus normalized axes.
 */
typedef struct HkInput {
    SDL_Joystick *joystick;
    SDL_Gamepad *gamepad;
    bool held[HK_BUTTON_COUNT];
    bool pressed[HK_BUTTON_COUNT];
    bool released[HK_BUTTON_COUNT];
    float left_x;
    float left_y;
    float right_x;
    float right_y;
    float left_trigger;
    float right_trigger;
    Uint8 dpad;
} HkInput;

void hk_input_init(HkInput *input);
void hk_input_update(HkInput *input);
void hk_input_close(HkInput *input);
#ifdef __cplusplus
}
#endif
#endif
