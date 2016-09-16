/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2016 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *
 */

#ifndef _COGL_DRIVER_VULKAN_PRIVATE_H_
#define _COGL_DRIVER_VULKAN_PRIVATE_H_

#include "cogl-context.h"
#include "cogl-matrix.h"
#include "cogl-vulkan-header.h"

typedef struct _CoglRendererVulkan
{
  VkInstance instance;
  VkDevice device;
  VkPhysicalDevice physical_device;
  VkPhysicalDeviceFeatures physical_device_features;
  VkPhysicalDeviceProperties physical_device_properties;
  VkPhysicalDeviceMemoryProperties physical_device_memory_properties;

  PFN_vkCreateInstance vkCreateInstance;
  PFN_vkDestroyInstance vkDestroyInstance;
  PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
  PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices;
  PFN_vkGetPhysicalDeviceFeatures vkGetPhysicalDeviceFeatures;
  PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties;
  PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties;
  PFN_vkCreateDevice vkCreateDevice;
  PFN_vkDestroyDevice vkDestroyDevice;
  PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr;
} CoglRendererVulkan;

typedef struct _CoglContextVulkan
{
  VkQueue queue;
  VkCommandPool cmd_pool;

  CoglBuffer *default_attributes;
  CoglMatrix mat;

  /* Not owned, this is a copy from CoglRendererVulkan. */
  VkDevice device;

  VkFormatProperties supported_formats[VK_FORMAT_END_RANGE];
} CoglContextVulkan;

CoglBool
_cogl_vulkan_renderer_init (CoglRenderer *renderer,
                            const char **extensions,
                            int n_extensions,
                            CoglError **error);

void
_cogl_vulkan_renderer_deinit (CoglRenderer *renderer);

CoglBool
_cogl_vulkan_context_init (CoglContext *context, CoglError **error);

void
_cogl_vulkan_context_deinit (CoglContext *context);

CoglBool
_cogl_vulkan_context_create_command_buffer (CoglContext *context,
                                            VkCommandBuffer *cmd_buffer,
                                            CoglError **error);

CoglBool
_cogl_vulkan_context_submit_command_buffer (CoglContext *context,
                                            VkCommandBuffer cmd_buffer,
                                            CoglError **error);

uint32_t
_cogl_vulkan_context_get_memory_heap (CoglContext *context,
                                      VkMemoryPropertyFlags flags);

CoglFuncPtr
_cogl_vulkan_renderer_get_proc_address (CoglRenderer *renderer,
                                        const char *name,
                                        CoglBool in_core);

#endif /* _COGL_DRIVER_VULKAN_PRIVATE_H_ */
