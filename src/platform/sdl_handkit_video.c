/* Fullscreen fbdev EGL backend for SDL 3.2.10: PowerVR (TrimUI A133P) and Mali (Anbernic H700).
 * Uses SDL's private-driver extension point; upstream SDL is unmodified.
 * PowerVR's NULL window system takes native window 0; Mali's fbdev EGL takes a pointer
 * to the window size. The framebuffer is queried for dimensions only; GPU swap presents frames.
 */
#include "SDL_internal.h"
#include "video/SDL_sysvideo.h"
#include "video/SDL_egl_c.h"
#include "events/SDL_events_c.h"
#ifdef SDL_VIDEO_VULKAN
#include "sdl_handkit_vulkan.h"
#endif
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

struct SDL_WindowData {
    EGLSurface egl_surface;
    struct {
        unsigned short width;
        unsigned short height;
    } mali_window; /* Mali's fbdev_window; it must outlive the surface */
};

static bool IsMali(SDL_VideoDevice *device) {
    const char *vendor = device->egl_data->eglQueryString(device->egl_data->egl_display, EGL_VENDOR);
    return vendor && SDL_strstr(vendor, "ARM");
}

static bool HkVideoInit(SDL_VideoDevice *device) {
    struct fb_var_screeninfo info;
    int fd = open("/dev/fb0", O_RDONLY);
    if (fd < 0) {
        return SDL_SetError("handkit: cannot open /dev/fb0");
    }
    int result = ioctl(fd, FBIOGET_VSCREENINFO, &info);
    close(fd);
    if (result < 0) {
        return SDL_SetError("handkit: cannot query framebuffer dimensions");
    }
    SDL_DisplayMode mode;
    SDL_zero(mode);
    mode.format = SDL_PIXELFORMAT_XRGB8888;
    mode.w = info.xres;
    mode.h = info.yres;
    mode.refresh_rate = 60;
    return SDL_AddBasicVideoDisplay(&mode) != 0;
}

static void HkVideoQuit(SDL_VideoDevice *device) {
}

static void HkPumpEvents(SDL_VideoDevice *device) {
}

static bool HkSetDisplayMode(SDL_VideoDevice *device, SDL_VideoDisplay *display, SDL_DisplayMode *mode) {
    if (mode->w != display->desktop_mode.w || mode->h != display->desktop_mode.h) {
        return SDL_SetError("handkit: only the native display mode is supported");
    }
    return true;
}

static bool HkLoadGL(SDL_VideoDevice *device, const char *path) {
    return SDL_EGL_LoadLibrary(device, path, EGL_DEFAULT_DISPLAY, 0);
}

static bool HkCreateWindow(SDL_VideoDevice *device, SDL_Window *window, SDL_PropertiesID props) {
    if (!(window->flags & (SDL_WINDOW_OPENGL | SDL_WINDOW_VULKAN))) {
        return SDL_SetError("handkit: requires an OpenGL ES or Vulkan window");
    }
    SDL_VideoDisplay *display = SDL_GetVideoDisplayForWindow(window);
    if (!display) {
        return false;
    }
    if (window->w != display->desktop_mode.w || window->h != display->desktop_mode.h) {
        return SDL_SetError("handkit: window must match native resolution %dx%d", display->desktop_mode.w, display->desktop_mode.h);
    }
    if ((window->flags & SDL_WINDOW_OPENGL) && !device->egl_data) {
        return SDL_SetError("handkit: EGL was not loaded");
    }
    window->internal = SDL_calloc(1, sizeof(*window->internal));
    if (!window->internal) {
        return false;
    }
    if (window->flags & SDL_WINDOW_OPENGL) {
        NativeWindowType native = 0;
        if (IsMali(device)) {
            window->internal->mali_window.width = (unsigned short)window->w;
            window->internal->mali_window.height = (unsigned short)window->h;
            native = (NativeWindowType)&window->internal->mali_window;
        }
        window->internal->egl_surface = SDL_EGL_CreateSurface(device, window, native);
        if (window->internal->egl_surface == EGL_NO_SURFACE) {
            SDL_free(window->internal);
            window->internal = NULL;
            return false;
        }
    }
    window->x = window->y = 0;
    SDL_SetKeyboardFocus(window);
    SDL_SetMouseFocus(window);
    return true;
}

static void HkDestroyWindow(SDL_VideoDevice *device, SDL_Window *window) {
    if (window->internal) {
        if (window->internal->egl_surface != EGL_NO_SURFACE) {
            SDL_EGL_DestroySurface(device, window->internal->egl_surface);
        }
        SDL_free(window->internal);
        window->internal = NULL;
    }
}

static SDL_GLContext HkCreateContext(SDL_VideoDevice *device, SDL_Window *window) {
    return SDL_EGL_CreateContext(device, window->internal->egl_surface);
}

static bool HkMakeCurrent(SDL_VideoDevice *device, SDL_Window *window, SDL_GLContext context) {
    return SDL_EGL_MakeCurrent(device, window ? window->internal->egl_surface : EGL_NO_SURFACE, context);
}

static bool HkSwap(SDL_VideoDevice *device, SDL_Window *window) {
    return SDL_EGL_SwapBuffers(device, window->internal->egl_surface);
}

static void HkDeleteDevice(SDL_VideoDevice *device) {
    SDL_free(device);
}

static SDL_VideoDevice *HkCreateDevice(void) {
    SDL_VideoDevice *device = SDL_calloc(1, sizeof(*device));
    if (!device) {
        return NULL;
    }
    device->device_caps = VIDEO_DEVICE_CAPS_FULLSCREEN_ONLY | VIDEO_DEVICE_CAPS_MODE_SWITCHING_EMULATED;
    device->VideoInit = HkVideoInit;
    device->VideoQuit = HkVideoQuit;
    device->PumpEvents = HkPumpEvents;
    device->SetDisplayMode = HkSetDisplayMode;
    device->CreateSDLWindow = HkCreateWindow;
    device->DestroyWindow = HkDestroyWindow;
    device->GL_LoadLibrary = HkLoadGL;
    device->GL_UnloadLibrary = SDL_EGL_UnloadLibrary;
    device->GL_GetProcAddress = SDL_EGL_GetProcAddressInternal;
    device->GL_CreateContext = HkCreateContext;
    device->GL_DestroyContext = SDL_EGL_DestroyContext;
    device->GL_MakeCurrent = HkMakeCurrent;
    device->GL_SwapWindow = HkSwap;
    device->GL_SetSwapInterval = SDL_EGL_SetSwapInterval;
    device->GL_GetSwapInterval = SDL_EGL_GetSwapInterval;
#ifdef SDL_VIDEO_VULKAN
    device->Vulkan_LoadLibrary = HkVulkanLoad;
    device->Vulkan_UnloadLibrary = HkVulkanUnload;
    device->Vulkan_GetInstanceExtensions = HkVulkanExtensions;
    device->Vulkan_CreateSurface = HkVulkanSurface;
    device->Vulkan_DestroySurface = HkVulkanDestroySurface;
    device->Vulkan_GetPresentationSupport = HkVulkanPresentation;
#endif
    device->free = HkDeleteDevice;
    return device;
}

VideoBootStrap PRIVATE_bootstrap = {"handkit", "Handheld fbdev EGL (PowerVR, Mali)", HkCreateDevice, NULL, false};
