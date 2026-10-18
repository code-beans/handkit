#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "demo_modes.h"
volatile sig_atomic_t demo_shutdown = 0;
int main(int argc, char **argv) {
    int mode = 1; // Start with the accelerated 2D demo.
    const char *initial = getenv("HANDKIT_DEMO_MODE");
    if (initial && !strcmp(initial, "software")) {
        mode = 0;
    }
    if (initial && !strcmp(initial, "vulkan")) {
        mode = 2;
    }
    const char *names[] = {"Software 2D", "OpenGL ES 2D", "Vulkan 3D"};
    int (*demos[])(int, char **) = {hk_software_demo, hk_gles_demo, hk_vulkan_demo};
    int smoke = argc > 1 && !strcmp(argv[1], "--smoke");
    char *smoke_argv[] = {argv[0], "2", NULL};
    int unavailable[3] = {0};
    for (int run = 0; !demo_shutdown; run++) {
        fprintf(stderr, "Mode %d/3: %s\n", mode + 1, names[mode]);
        int result = demos[mode](smoke ? 2 : argc, smoke ? smoke_argv : argv);
        if (demo_shutdown) {
            return 0;
        }
        if (result == DEMO_ERROR) {
            // Smoke tests need every mode; interactively, skip one the driver cannot run.
            unavailable[mode] = 1;
            if (smoke || (unavailable[0] && unavailable[1] && unavailable[2])) {
                return 1;
            }
            fprintf(stderr, "Skipping %s on this device\n", names[mode]);
            result = DEMO_NEXT;
        }
        if (smoke) {
            if (run == 5) {
                fprintf(stderr, "All three modes passed twice in one process\n");
                return 0;
            }
            mode = (mode + 1) % 3;
        } else if (result == DEMO_NEXT || result == DEMO_PREVIOUS) {
            int step = result == DEMO_NEXT ? 1 : 2;
            do {
                mode = (mode + step) % 3;
            } while (unavailable[mode]);
        } else {
            return 0;
        }
    }
    return 0;
}
