#ifndef SDL_HANDKIT_VULKAN_H
#define SDL_HANDKIT_VULKAN_H
#include "video/SDL_vulkan_internal.h"
bool HkVulkanLoad(SDL_VideoDevice *device, const char *path);
void HkVulkanUnload(SDL_VideoDevice *device);
const char *const *HkVulkanExtensions(SDL_VideoDevice *device, Uint32 *count);
bool HkVulkanSurface(SDL_VideoDevice *device, SDL_Window *window, VkInstance instance, const struct VkAllocationCallbacks *allocator, VkSurfaceKHR *surface);
void HkVulkanDestroySurface(SDL_VideoDevice *device, VkInstance instance, VkSurfaceKHR surface, const struct VkAllocationCallbacks *allocator);
bool HkVulkanPresentation(SDL_VideoDevice *device, VkInstance instance, VkPhysicalDevice physicalDevice, Uint32 queueFamilyIndex);
#endif
