/* Fullscreen Vulkan presentation via VK_KHR_display on the vendor driver (PowerVR; Mali where its ICD offers it). */
#include "SDL_internal.h"
#include "video/SDL_sysvideo.h"
#include "sdl_handkit_vulkan.h"

void HkVulkanUnload(SDL_VideoDevice *device) {
    if (device->vulkan_config.loader_handle) {
        SDL_UnloadObject(device->vulkan_config.loader_handle);
    }
    device->vulkan_config.loader_handle = NULL;
    device->vulkan_config.vkGetInstanceProcAddr = NULL;
    device->vulkan_config.vkEnumerateInstanceExtensionProperties = NULL;
}

bool HkVulkanLoad(SDL_VideoDevice *device, const char *path) {
    if (device->vulkan_config.loader_handle) {
        return SDL_SetError("handkit: Vulkan already loaded");
    }
    if (!path) {
        path = SDL_GetHint(SDL_HINT_VULKAN_LIBRARY);
    }
    if (!path) {
        path = "libvulkan.so.1";
    }
    device->vulkan_config.loader_handle = SDL_LoadObject(path);
    if (!device->vulkan_config.loader_handle) {
        return false;
    }
    PFN_vkGetInstanceProcAddr get = (PFN_vkGetInstanceProcAddr)SDL_LoadFunction(device->vulkan_config.loader_handle, "vkGetInstanceProcAddr");
    if (!get) {
        HkVulkanUnload(device);
        return false;
    }
    device->vulkan_config.vkGetInstanceProcAddr = (SDL_FunctionPointer)get;
    device->vulkan_config.vkEnumerateInstanceExtensionProperties = (SDL_FunctionPointer)get(VK_NULL_HANDLE, "vkEnumerateInstanceExtensionProperties");
    Uint32 count = 0;
    VkExtensionProperties *extensions = SDL_Vulkan_CreateInstanceExtensionsList((PFN_vkEnumerateInstanceExtensionProperties)device->vulkan_config.vkEnumerateInstanceExtensionProperties, &count);
    bool hasSurface = false, hasDisplay = false;
    if (extensions) {
        for (Uint32 i = 0; i < count; i++) {
            hasSurface |= SDL_strcmp(extensions[i].extensionName, VK_KHR_SURFACE_EXTENSION_NAME) == 0;
            hasDisplay |= SDL_strcmp(extensions[i].extensionName, VK_KHR_DISPLAY_EXTENSION_NAME) == 0;
        }
        SDL_free(extensions);
    }
    if (!hasSurface || !hasDisplay) {
        HkVulkanUnload(device);
        return SDL_SetError("handkit: driver requires VK_KHR_surface and VK_KHR_display");
    }
    SDL_strlcpy(device->vulkan_config.loader_path, path, sizeof(device->vulkan_config.loader_path));
    return true;
}

const char *const *HkVulkanExtensions(SDL_VideoDevice *device, Uint32 *count) {
    static const char *const extensions[] = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_DISPLAY_EXTENSION_NAME};
    if (count) {
        *count = SDL_arraysize(extensions);
    }
    return extensions;
}

bool HkVulkanSurface(SDL_VideoDevice *device, SDL_Window *window, VkInstance instance, const struct VkAllocationCallbacks *allocator, VkSurfaceKHR *surface) {
    if (!device->vulkan_config.loader_handle) {
        return SDL_SetError("handkit: Vulkan not loaded");
    }
    return SDL_Vulkan_Display_CreateSurface((void *)device->vulkan_config.vkGetInstanceProcAddr, instance, allocator, surface);
}

void HkVulkanDestroySurface(SDL_VideoDevice *device, VkInstance instance, VkSurfaceKHR surface, const struct VkAllocationCallbacks *allocator) {
    SDL_Vulkan_DestroySurface_Internal((void *)device->vulkan_config.vkGetInstanceProcAddr, instance, surface, allocator);
}

bool HkVulkanPresentation(SDL_VideoDevice *device, VkInstance instance, VkPhysicalDevice physicalDevice, Uint32 queueFamilyIndex) {
    // Actual surface/queue support is checked again by the Vulkan GPU backend.
    PFN_vkGetInstanceProcAddr get = (PFN_vkGetInstanceProcAddr)device->vulkan_config.vkGetInstanceProcAddr;
    PFN_vkGetPhysicalDeviceDisplayPropertiesKHR displays = (PFN_vkGetPhysicalDeviceDisplayPropertiesKHR)get(instance, "vkGetPhysicalDeviceDisplayPropertiesKHR");
    Uint32 count = 0;
    return displays && displays(physicalDevice, &count, NULL) == VK_SUCCESS && count > 0;
}
