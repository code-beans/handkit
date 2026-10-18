#include <handkit/runtime.h>
#include <handkit/system.h>
#include <stdio.h>
#include <stdlib.h>
/* All drawing uses ordinary SDL3 APIs. The SDK owns device-specific setup. */
int main(int argc, char **argv) {
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    HkInput input;
    hk_input_init(&input);
    if (!hk_init(HK_BACKEND_GLES) || !hk_create_gles_renderer(&window, &renderer)) {
        fprintf(stderr, "Init: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    hk_system_init();
    unsigned duration = argc > 1 ? (unsigned)atoi(argv[1]) : 0;
    Uint64 start = SDL_GetTicks();
    Uint64 last = start;
    float x = 200;
    float y = 200;
    bool running = true;
    while (running && (!duration || SDL_GetTicks() - start < duration * 1000ULL)) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }
        hk_input_update(&input);
        /* Start + Select or holding power sends SDL_EVENT_QUIT; after the latter NextUI shuts
         * down. Sleep blocks inside the update; an app with audio would close it on
         * SDL_EVENT_WILL_ENTER_BACKGROUND and reopen it on SDL_EVENT_DID_ENTER_FOREGROUND. */
        hk_system_update(&input);
        Uint64 now = SDL_GetTicks();
        float dt = SDL_min((now - last) / 1000.0f, 0.1f);
        last = now;
        x += (input.left_x + !!(input.dpad & SDL_HAT_RIGHT) - !!(input.dpad & SDL_HAT_LEFT)) * dt * 250;
        y += (input.left_y + !!(input.dpad & SDL_HAT_DOWN) - !!(input.dpad & SDL_HAT_UP)) * dt * 250;
        int w;
        int h;
        SDL_GetRenderOutputSize(renderer, &w, &h);
        x = SDL_clamp(x, 0, w - 40);
        y = SDL_clamp(y, 120, h - 40);
        SDL_SetRenderDrawColor(renderer, 13, 20, 35, 255);
        SDL_RenderClear(renderer);
        SDL_SetRenderDrawColor(renderer, 92, 231, 197, 255);
        SDL_FRect box = {x, y, 40, 40};
        SDL_RenderFillRect(renderer, &box);
        SDL_SetRenderScale(renderer, 2, 2);
        SDL_RenderDebugText(renderer, 16, 16, "Handkit / SDL3 / minimal example");
        SDL_RenderDebugText(renderer, 16, 32, "D-pad / stick: move / Start+Select: exit");
        SDL_SetRenderScale(renderer, 1, 1);
        hk_system_draw(renderer);
        if (!SDL_RenderPresent(renderer)) {
            fprintf(stderr, "Present: %s\n", SDL_GetError());
            running = false;
        }
    }
    hk_input_close(&input);
    hk_system_quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
