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

#ifndef __COGL_FRAMEBUFFER_VULKAN_PRIVATE_H__
#define __COGL_FRAMEBUFFER_VULKAN_PRIVATE_H__

typedef struct _CoglFramebufferVulkan
{
  /* Not owned. Do not free. (Either this is a copy from CoglOffscreenVulkan
     or from whatever winsys we're running on.) */
  VkFramebuffer framebuffer;
  VkImage color_image;
  VkFormat color_format;

  /* Owned. */
  VkFormat depth_format;
  VkImage depth_image;
  VkImageView depth_image_view;
  VkDeviceMemory depth_memory;

  VkFence fence;

  VkRenderPass render_pass;
  GArray *cmd_buffers;

  /* A copy of the last item in cmd_buffers for conviniency. */
  VkCommandBuffer cmd_buffer;

  CoglBool render_pass_started;
  uint32_t cmd_buffer_length;

  /* An array of attributes buffers being used by the GPU. This needs to
     stay alive for as long as the GPU will use it. */
  GPtrArray *attribute_buffers;

  /* An array of pipeline being used by the GPU. Some of the data attached
     onto pipelines must be kept alive for as long as the command buffers
     using them haven't been processed. We keep a reference on those
     pipelines until we know they're not in use anymore. */
  GPtrArray *pipelines;

  /* This is a temporary variable used to work around Cogl internal API that
     is doesn't currently propagate vertices mode down to the pipeline
     flushing. We need to have an array of those because of reentrancy when
     the journal gets flushed as a result of a draw call. The API should be
     reworked to better fit Vulkan's pipeline setup. */
  CoglVerticesMode vertices_modes[10];
  uint32_t n_vertices_modes;
} CoglFramebufferVulkan;

typedef struct _CoglOffscreenVulkan
{
  CoglFramebufferVulkan parent;

  VkFramebuffer framebuffer;
  VkImageView image_view;
} CoglOffscreenVulkan;

CoglBool
_cogl_framebuffer_vulkan_init (CoglFramebuffer *framebuffer,
                               VkFormat color_format,
                               CoglError **error);

void
_cogl_framebuffer_vulkan_deinit (CoglFramebuffer *framebuffer);

VkResult
_cogl_framebuffer_vulkan_create_framebuffer (CoglFramebuffer *framebuffer,
                                             VkImageView vk_image_view,
                                             VkFramebuffer *vk_framebuffer);

void
_cogl_framebuffer_vulkan_update_framebuffer (CoglFramebuffer *framebuffer,
                                             VkFramebuffer vk_framebuffer,
                                             VkImage vk_image);

void
_cogl_clip_stack_vulkan_flush (CoglClipStack *clip,
                               CoglFramebuffer *framebuffer);

void
_cogl_framebuffer_vulkan_flush_state (CoglFramebuffer *draw_buffer,
                                      CoglFramebuffer *read_buffer,
                                      CoglFramebufferState state);

void
_cogl_framebuffer_vulkan_clear (CoglFramebuffer *framebuffer,
                                unsigned long buffers,
                                float red,
                                float green,
                                float blue,
                                float alpha);

void
_cogl_framebuffer_vulkan_query_bits (CoglFramebuffer *framebuffer,
                                     CoglFramebufferBits *bits);

void
_cogl_framebuffer_vulkan_finish (CoglFramebuffer *framebuffer);

void
_cogl_framebuffer_vulkan_discard_buffers (CoglFramebuffer *framebuffer,
                                          unsigned long buffers);

void
_cogl_framebuffer_vulkan_bind (CoglFramebuffer *framebuffer, GLenum target);

void
_cogl_framebuffer_vulkan_draw_attributes (CoglFramebuffer *framebuffer,
                                          CoglPipeline *pipeline,
                                          CoglVerticesMode mode,
                                          int first_vertex,
                                          int n_vertices,
                                          CoglAttribute **attributes,
                                          int n_attributes,
                                          CoglDrawFlags flags);

void
_cogl_framebuffer_vulkan_draw_indexed_attributes (CoglFramebuffer *framebuffer,
                                                  CoglPipeline *pipeline,
                                                  CoglVerticesMode mode,
                                                  int first_vertex,
                                                  int n_vertices,
                                                  CoglIndices *indices,
                                                  CoglAttribute **attributes,
                                                  int n_attributes,
                                                  CoglDrawFlags flags);

CoglBool
_cogl_framebuffer_vulkan_read_pixels_into_bitmap (CoglFramebuffer *framebuffer,
                                                  int x,
                                                  int y,
                                                  CoglReadPixelsFlags source,
                                                  CoglBitmap *bitmap,
                                                  CoglError **error);

CoglBool
_cogl_offscreen_vulkan_allocate (CoglOffscreen *offscreen,
                                 CoglError **error);

void
_cogl_offscreen_vulkan_free (CoglOffscreen *offscreen);

void
_cogl_framebuffer_vulkan_end (CoglFramebuffer *framebuffer,
                              CoglBool wait_fence);

void
_cogl_framebuffer_vulkan_begin_render_pass (CoglFramebuffer *framebuffer);

void
_cogl_framebuffer_vulkan_ensure_clean_command_buffer (CoglFramebuffer *framebuffer);


#endif /* __COGL_FRAMEBUFFER_VULKAN_PRIVATE_H__ */
