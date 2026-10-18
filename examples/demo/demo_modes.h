#ifndef DEMO_MODES_H
#define DEMO_MODES_H
#include <signal.h>
typedef enum DemoResult {
    DEMO_EXIT = 0,
    DEMO_ERROR = 1,
    DEMO_NEXT = 2,
    DEMO_PREVIOUS = 3
} DemoResult;
int hk_software_demo(int argc, char **argv);
int hk_gles_demo(int argc, char **argv);
int hk_vulkan_demo(int argc, char **argv);
#ifdef DEMO_COMBINED
extern volatile sig_atomic_t demo_shutdown;
#endif
#endif
