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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>

#include <gmodule.h>

#include "cogl-private.h"
#include "cogl-context-private.h"
#include "cogl-driver-vulkan-private.h"
#include "cogl-util-vulkan-private.h"
#include "cogl-feature-private.h"
#include "cogl-renderer-private.h"
#include "cogl-error-private.h"
#include "cogl-buffer-vulkan-private.h"
#include "cogl-framebuffer-vulkan-private.h"
#include "cogl-pipeline-vulkan-private.h"
#include "cogl-sampler-vulkan-private.h"
#include "cogl-texture-2d-vulkan-private.h"
#include "driver/nop/cogl-texture-3d-nop-private.h"
#include "driver/nop/cogl-texture-rectangle-nop-private.h"

static CoglBool
_cogl_driver_pixel_format_from_gl_internal (CoglContext *context,
                                            GLenum gl_int_format,
                                            CoglPixelFormat *out_format)
{
  /* OpenG... What?? */
  return FALSE;
}

static CoglPixelFormat
_cogl_driver_pixel_format_to_gl (CoglContext *context,
                                 CoglPixelFormat  format,
                                 GLenum *out_glintformat,
                                 GLenum *out_glformat,
                                 GLenum *out_gltype)
{
  if (out_glintformat)
    *out_glintformat = format;
  if (out_glformat)
    *out_glformat = format;
  return format;
}

static CoglBool
_cogl_driver_update_features (CoglContext *ctx,
                              CoglError **error)
{
  CoglRenderer *renderer = ctx->display->renderer;
  CoglRendererVulkan *vk_renderer = renderer->winsys;
  uint32_t version = vk_renderer->physical_device_properties.apiVersion;

  _cogl_feature_check_vulkan_ext_functions (ctx,
                                            VK_VERSION_MAJOR (version),
                                            VK_VERSION_MINOR (version),
                                            VK_VERSION_PATCH (version),
                                            NULL);

  COGL_FLAGS_SET (ctx->private_features,
                  COGL_PRIVATE_FEATURE_OFFSCREEN_BLIT, TRUE);
  COGL_FLAGS_SET (ctx->private_features, COGL_PRIVATE_FEATURE_PBOS, TRUE);
  COGL_FLAGS_SET (ctx->private_features, COGL_PRIVATE_FEATURE_VBOS, TRUE);
  COGL_FLAGS_SET (ctx->private_features,
                  COGL_PRIVATE_FEATURE_SAMPLER_OBJECTS, TRUE);

  COGL_FLAGS_SET (ctx->features, COGL_FEATURE_ID_GLSL, TRUE);
  COGL_FLAGS_SET (ctx->features, COGL_FEATURE_ID_OFFSCREEN, TRUE);
  COGL_FLAGS_SET (ctx->features, COGL_FEATURE_ID_MAP_BUFFER_FOR_READ, TRUE);
  COGL_FLAGS_SET (ctx->features, COGL_FEATURE_ID_MAP_BUFFER_FOR_WRITE, TRUE);
  COGL_FLAGS_SET (ctx->features, COGL_FEATURE_ID_TEXTURE_RG, TRUE);

  if (vk_renderer->physical_device_properties.limits.pointSizeRange[1] > 1.0)
    COGL_FLAGS_SET (ctx->features, COGL_FEATURE_ID_POINT_SPRITE, TRUE);

  ctx->feature_flags |= COGL_FEATURE_SHADERS_GLSL;

  return TRUE;
}

CoglBool
_cogl_vulkan_renderer_init (CoglRenderer *renderer,
                            const char **extensions,
                            int n_extensions,
                            CoglError **error)
{
  CoglRendererVulkan *vk_renderer = renderer->winsys;
  VkPhysicalDevice *devices;
  VkPhysicalDeviceProperties device_properties;
  VkInstanceCreateInfo instance_info = {
    .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    .pApplicationInfo = &(VkApplicationInfo) {
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pApplicationName = "Cogl",
      .apiVersion = VK_MAKE_VERSION(1, 0, 5),
    },
    .enabledExtensionCount = n_extensions,
    .ppEnabledExtensionNames = extensions,
  };
  struct {
      const char *name;
      void *sym_ptr;
  } vulkan_symbols[] = {
#define VK_SYM(NAME) \
        { #NAME, &vk_renderer->NAME },
#include "cogl-vulkan-symbols.x"
#undef VK_SYM
  };

  VkResult result;
  uint32_t i, device_count;

  renderer->libgl_module = g_module_open(COGL_VULKAN_LIBNAME,
                                         G_MODULE_BIND_LAZY);

  for (i = 0; i < G_N_ELEMENTS(vulkan_symbols); i++)
    {

      if (!g_module_symbol(renderer->libgl_module,
                           vulkan_symbols[i].name,
                           vulkan_symbols[i].sym_ptr))
        {
          _cogl_set_error (error, COGL_DRIVER_ERROR,
                           COGL_DRIVER_ERROR_INTERNAL,
                           "Cannot find vulkan instance symbol %s",
                           vulkan_symbols[i].name);
          goto error;
        }
    }

  result = vk_renderer->vkCreateInstance (&instance_info, NULL,
                                          &vk_renderer->instance);
  if (result != VK_SUCCESS)
    {
      _cogl_set_error (error, COGL_DRIVER_ERROR,
                       COGL_DRIVER_ERROR_INTERNAL,
                       "Cannot create vulkan instance : %s",
                       _cogl_vulkan_error_to_string (result));
      goto error;
    }

  result = vk_renderer->vkEnumeratePhysicalDevices (vk_renderer->instance,
                                                    &device_count,
                                                    NULL);
  if (result != VK_SUCCESS)
    {
      _cogl_set_error (error, COGL_DRIVER_ERROR,
                       COGL_DRIVER_ERROR_INTERNAL,
                       "Cannot list vulkan physical devices : %s",
                       _cogl_vulkan_error_to_string (result));
      goto error;
    }

  if (device_count < 1)
    {
      _cogl_set_error (error, COGL_DRIVER_ERROR,
                       COGL_DRIVER_ERROR_INTERNAL,
                       "No physical device found");
      goto error;
    }

  devices = g_alloca (sizeof (VkPhysicalDevice) * device_count);

  result = vk_renderer->vkEnumeratePhysicalDevices (vk_renderer->instance,
                                                    &device_count,
                                                    devices);
  if (result != VK_SUCCESS)
    {
      _cogl_set_error (error, COGL_DRIVER_ERROR,
                       COGL_DRIVER_ERROR_INTERNAL,
                       "Cannot list vulkan physical devices : %s",
                       _cogl_vulkan_error_to_string (result));
      goto error;
    }

  if (G_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_VULKAN)))
    {
      for (i = 0; i < device_count; i++)
        {
          vk_renderer->vkGetPhysicalDeviceProperties (devices[i],
                                                      &device_properties);
          COGL_NOTE (VULKAN, "Device %i : %s", i, device_properties.deviceName);
        }
    }

  vk_renderer->physical_device = devices[0];
  vk_renderer->vkGetPhysicalDeviceFeatures (vk_renderer->physical_device,
                                            &vk_renderer->physical_device_features);
  vk_renderer->vkGetPhysicalDeviceProperties (vk_renderer->physical_device,
                                              &vk_renderer->physical_device_properties);

  vk_renderer->vkGetPhysicalDeviceMemoryProperties (vk_renderer->physical_device,
                                                    &vk_renderer->physical_device_memory_properties);


  result = vk_renderer->vkCreateDevice(vk_renderer->physical_device, &(VkDeviceCreateInfo) {
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .queueCreateInfoCount = 1,
      .pQueueCreateInfos = &(VkDeviceQueueCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = 0,
        .queueCount = 1,
      }
    },
    NULL,
    &vk_renderer->device);
  if (result != VK_SUCCESS)
    {
      _cogl_set_error (error, COGL_DRIVER_ERROR,
                       COGL_DRIVER_ERROR_INTERNAL,
                       "Cannot create device for physical device `%s'",
                       vk_renderer->physical_device_properties.deviceName);
      goto error;
    }

  return TRUE;

 error:
  _cogl_vulkan_renderer_deinit (renderer);

  return FALSE;
}

void
_cogl_vulkan_renderer_deinit (CoglRenderer *renderer)
{
  CoglRendererVulkan *vk_renderer = renderer->winsys;

  if (vk_renderer->device != VK_NULL_HANDLE)
    vk_renderer->vkDestroyDevice (vk_renderer->device, NULL);
  if (vk_renderer->instance != VK_NULL_HANDLE)
    vk_renderer->vkDestroyInstance (vk_renderer->instance, NULL);
}

CoglBool
_cogl_vulkan_context_init (CoglContext *context, CoglError **error)
{
  CoglRenderer *renderer = context->display->renderer;
  CoglRendererVulkan *vk_renderer = renderer->winsys;
  CoglContextVulkan *vk_ctx = g_slice_new0 (CoglContextVulkan);
  int i;

  context->winsys = vk_ctx;

  context->glsl_version_to_use = 430;

  cogl_matrix_init_identity (&vk_ctx->mat);
  vk_ctx->mat.yy = -1;
  vk_ctx->mat.zz = 0.5;
  vk_ctx->mat.zw = 0.5;

  vk_ctx->device = vk_renderer->device;

  VK ( context, vkGetDeviceQueue (vk_ctx->device, 0, 0, &vk_ctx->queue) );

  VK_ERROR ( context,
             vkCreateCommandPool (vk_ctx->device, &(const VkCommandPoolCreateInfo) {
                 .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                 .queueFamilyIndex = 0,
                 .flags = 0
               },
               NULL,
               &vk_ctx->cmd_pool),
             error, COGL_DRIVER_ERROR, COGL_DRIVER_ERROR_INTERNAL );

  /* Create a default set of attributes. */
  vk_ctx->default_attributes =
    _cogl_pipeline_ensure_default_attributes (context);

  /* Query all pixel format support we can get. */
  for (i = 0; i < G_N_ELEMENTS (vk_ctx->supported_formats); i++)
    {
      VK ( context,
           vkGetPhysicalDeviceFormatProperties (vk_renderer->physical_device,
                                                i,
                                                &vk_ctx->supported_formats[i]) );
    }

  return TRUE;

 error:
  _cogl_vulkan_context_deinit (context);

  return FALSE;
}

void
_cogl_vulkan_context_deinit (CoglContext *context)
{
  CoglContextVulkan *vk_ctx = context->winsys;

  if (vk_ctx->default_attributes)
    cogl_object_unref (vk_ctx->default_attributes);
  if (vk_ctx->cmd_pool != VK_NULL_HANDLE)
    VK ( context,
         vkDestroyCommandPool (vk_ctx->device, vk_ctx->cmd_pool, NULL) );

  g_slice_free (CoglContextVulkan, vk_ctx);
}

CoglBool
_cogl_vulkan_context_create_command_buffer (CoglContext *context,
                                            VkCommandBuffer *cmd_buffer,
                                            CoglError **error)
{
  CoglContextVulkan *vk_ctx = context->winsys;
  VkCommandBufferAllocateInfo buffer_allocate_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .commandPool = vk_ctx->cmd_pool,
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = 1,
  };
  VkCommandBufferBeginInfo buffer_begin_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = 0
  };

  VK_ERROR ( context,
             vkAllocateCommandBuffers (vk_ctx->device,
                                       &buffer_allocate_info,
                                       cmd_buffer),
             error, COGL_SYSTEM_ERROR, COGL_SYSTEM_ERROR_NO_MEMORY );

  VK_ERROR ( context,
             vkBeginCommandBuffer (*cmd_buffer, &buffer_begin_info),
             error, COGL_SYSTEM_ERROR, COGL_SYSTEM_ERROR_NO_MEMORY );

  return TRUE;

 error:

  if (*cmd_buffer != VK_NULL_HANDLE)
    VK ( context,
         vkFreeCommandBuffers (vk_ctx->device, vk_ctx->cmd_pool,
                               1, cmd_buffer) );

  return FALSE;
}

CoglBool
_cogl_vulkan_context_submit_command_buffer (CoglContext *context,
                                            VkCommandBuffer cmd_buffer,
                                            CoglError **error)
{
  CoglContextVulkan *vk_ctx = context->winsys;
  VkFence fence = VK_NULL_HANDLE;
  CoglBool ret = FALSE;

  VK ( context, vkEndCommandBuffer (cmd_buffer) );

  VK_ERROR ( context,
             vkCreateFence (vk_ctx->device, &(VkFenceCreateInfo) {
                 .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                 .flags = VK_FENCE_CREATE_SIGNALED_BIT,
               },
               NULL,
               &fence),
             error, COGL_TEXTURE_ERROR, COGL_TEXTURE_ERROR_BAD_PARAMETER );

  VK_ERROR ( context,
             vkResetFences (vk_ctx->device, 1, &fence),
             error, COGL_TEXTURE_ERROR, COGL_TEXTURE_ERROR_BAD_PARAMETER );

  VK_ERROR ( context,
             vkQueueSubmit (vk_ctx->queue, 1,
                            &(VkSubmitInfo) {
                              .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                              .commandBufferCount = 1,
                              .pCommandBuffers = &cmd_buffer,
                            }, fence),
             error, COGL_TEXTURE_ERROR, COGL_TEXTURE_ERROR_BAD_PARAMETER );

  VK_ERROR ( context,
             vkWaitForFences (vk_ctx->device, 1, &fence, VK_TRUE, INT64_MAX),
             error, COGL_TEXTURE_ERROR, COGL_TEXTURE_ERROR_BAD_PARAMETER );

  ret = TRUE;

 error:
  if (fence != VK_NULL_HANDLE)
    VK ( context,
         vkDestroyFence (vk_ctx->device, fence, NULL) );

  return ret;
}

uint32_t _cogl_vulkan_context_get_memory_heap (CoglContext *context,
                                               VkMemoryPropertyFlags flags)
{
  CoglRenderer *renderer = context->display->renderer;
  CoglRendererVulkan *vk_renderer = renderer->winsys;
  uint32_t i;

  for (i = 0;
       i < vk_renderer->physical_device_memory_properties.memoryTypeCount;
       i++)
    {
      const VkMemoryType *mem_type =
        &vk_renderer->physical_device_memory_properties.memoryTypes[i];

      if ((mem_type->propertyFlags & flags) == flags)
        return mem_type->heapIndex;
    }

  g_warning ("Cannot file memory heap for flags %x, default to first heap.",
             flags);

  return 0;
}

CoglFuncPtr
_cogl_vulkan_renderer_get_proc_address (CoglRenderer *renderer,
                                        const char *name,
                                        CoglBool in_core)
{
  CoglFuncPtr ptr;

  g_module_symbol (renderer->libgl_module, name, (gpointer *) &ptr);

  return ptr;
}

const CoglDriverVtable
_cogl_driver_vulkan =
  {
    _cogl_driver_pixel_format_from_gl_internal,
    _cogl_driver_pixel_format_to_gl,
    _cogl_driver_update_features,
    _cogl_offscreen_vulkan_allocate,
    _cogl_offscreen_vulkan_free,
    _cogl_framebuffer_vulkan_flush_state,
    _cogl_framebuffer_vulkan_clear,
    _cogl_framebuffer_vulkan_query_bits,
    _cogl_framebuffer_vulkan_finish,
    _cogl_framebuffer_vulkan_discard_buffers,
    _cogl_framebuffer_vulkan_draw_attributes,
    _cogl_framebuffer_vulkan_draw_indexed_attributes,
    _cogl_framebuffer_vulkan_read_pixels_into_bitmap,
    _cogl_texture_2d_vulkan_free,
    _cogl_texture_2d_vulkan_can_create,
    _cogl_texture_2d_vulkan_init,
    _cogl_texture_2d_vulkan_allocate,
    _cogl_texture_2d_vulkan_copy_from_framebuffer,
    _cogl_texture_2d_vulkan_get_gl_info,
    _cogl_texture_2d_vulkan_generate_mipmap,
    NULL, /* texture_2d_flush_legacy_filters */
    NULL, /* texture_2d_flush_legacy_wrap_modes */
    _cogl_texture_2d_vulkan_copy_from_bitmap,
    NULL, /* texture_2d_get_data */
    _cogl_texture_2d_vulkan_get_vulkan_info,
    _cogl_texture_2d_vulkan_vulkan_move_to,
    _cogl_texture_3d_nop_free,
    _cogl_texture_3d_nop_init,
    _cogl_texture_3d_nop_allocate,
    _cogl_texture_3d_nop_get_gl_handle,
    _cogl_texture_3d_nop_get_gl_format,
    _cogl_texture_3d_nop_generate_mipmap,
    NULL, /* texture_3d_flush_legacy_filters */
    NULL, /* texture_3d_flush_legacy_wrap_modes */
    _cogl_texture_3d_nop_get_vulkan_info,
    _cogl_texture_3d_nop_vulkan_move_to,
    _cogl_texture_rectangle_nop_free,
    _cogl_texture_rectangle_nop_init,
    _cogl_texture_rectangle_nop_allocate,
    _cogl_texture_rectangle_nop_get_gl_handle,
    _cogl_texture_rectangle_nop_get_gl_format,
    NULL, /* texture_rectangle_flush_legacy_filters */
    NULL, /* texture_rectangle_flush_legacy_wrap_modes */
    _cogl_texture_rectangle_nop_get_data,
    _cogl_vulkan_flush_attributes_state,
    _cogl_clip_stack_vulkan_flush,
    _cogl_buffer_vulkan_create,
    _cogl_buffer_vulkan_destroy,
    _cogl_buffer_vulkan_map_range,
    _cogl_buffer_vulkan_unmap,
    _cogl_buffer_vulkan_set_data,
    _cogl_sampler_vulkan_create,
    _cogl_sampler_vulkan_destroy,
  };
