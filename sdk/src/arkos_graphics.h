/* Internal ArkOS graphics selection, shared with the backend checks. */
#include <SDL3/SDL.h>
#include <limits.h>
#include <stdlib.h>

static void hk_arkos_select_graphics(const char *library_dir) {
    char path[PATH_MAX], gbm[PATH_MAX], gles[PATH_MAX];
    SDL_snprintf(path, sizeof(path), "%s/libgbm.so.1", library_dir);
    if (!realpath(path, gbm)) return;
    SDL_snprintf(path, sizeof(path), "%s/libGLESv2.so.2", library_dir);
    if (!realpath(path, gles) || SDL_strcmp(gbm, gles)) return;
    const char *name = SDL_strrchr(gbm, '/');
    if (!name) return;
    ++name;
    if (SDL_strncmp(name, "libmali", 7) && SDL_strncmp(name, "libMali", 7)) return;
    /* dArkOSRE may retain a different libEGL.so.1 beside the Mali aliases.
     * Use matching EGL/GLES/GBM without changing the firmware's symlinks.
     * Explicit application hints and environment settings take precedence. */
    SDL_SetHintWithPriority(SDL_HINT_EGL_LIBRARY, gbm, SDL_HINT_DEFAULT);
    SDL_SetHintWithPriority(SDL_HINT_OPENGL_LIBRARY, gles, SDL_HINT_DEFAULT);
#ifndef NDEBUG
    SDL_Log("Handkit EGL: %s; GLES: %s", SDL_GetHint(SDL_HINT_EGL_LIBRARY), SDL_GetHint(SDL_HINT_OPENGL_LIBRARY));
#endif
}
