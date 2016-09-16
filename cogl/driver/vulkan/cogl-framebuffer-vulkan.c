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

#include <glib.h>
#include <string.h>

#include "cogl-buffer-vulkan-private.h"
#include "cogl-context-private.h"
#include "cogl-driver-vulkan-private.h"
#include "cogl-error-private.h"
#include "cogl-framebuffer-private.h"
#include "cogl-framebuffer-vulkan-private.h"
#include "cogl-indices-private.h"
#include "cogl-pipeline-vulkan-private.h"
#include "cogl-texture-private.h"
#include "cogl-texture-2d-private.h"
#include "cogl-texture-2d-vulkan-private.h"
#include "cogl-util-vulkan-private.h"

static void
_cogl_framebuffer_vulkan_unref_pipelines (CoglFramebuffer *framebuffer,
                                          CoglFramebufferVulkan *vk_fb)
{
  guint i;

  for (i = 0; i < vk_fb->pipelines->len; i++) {
    CoglPipeline *pipeline = g_ptr_array_index (vk_fb->pipelines, i);
    _cogl_pipeline_vulkan_discard_framebuffer (pipeline, framebuffer);
    cogl_object_unref (pipeline);
  }
  g_ptr_array_set_size (vk_fb->pipelines, 0);
}

static void
_cogl_offscreen_vulkan_prepare_for_rendering (CoglFramebuffer *framebuffer)
{
  CoglFramebufferVulkan *vk_fb = framebuffer->winsys;
  CoglOffscreen *offscreen = COGL_OFFSCREEN (framebuffer);

  _cogl_texture_2d_vulkan_vulkan_move_to (COGL_TEXTURE_2D (offscreen->texture),
                                          COGL_TEXTURE_DOMAIN_ATTACHMENT,
                                          vk_fb->cmd_buffer);
}

static void
_cogl_framebuffer_vulkan_ensure_command_buffer (CoglFramebuffer *framebuffer)
{
  CoglContext *ctx = framebuffer->context;
  CoglFramebufferVulkan *vk_fb = framebuffer->winsys;

  if (vk_fb->cmd_buffer != VK_NULL_HANDLE)
    return;

  if (!_cogl_vulkan_context_create_command_buffer (ctx,
                                                   &vk_fb->cmd_buffer, NULL))
    return;

  g_array_append_val (vk_fb->cmd_buffers, vk_fb->cmd_buffer);
}

static CoglBool
_cogl_framebuffer_vulkan_allocate_depth_buffer (CoglFramebuffer *framebuffer,
                                                CoglError **error)
{
  CoglContext *ctx = framebuffer->context;
  CoglContextVulkan *vk_ctx = ctx->winsys;
  CoglFramebufferVulkan *vk_fb = framebuffer->winsys;
  VkImageCreateInfo image_info = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .pNext = NULL,
    .flags = 0,
    .imageType = VK_IMAGE_TYPE_2D,
    .format = vk_fb->depth_format,
    .extent = {
      .width = framebuffer->width,
      .height = framebuffer->height,
      .depth = 1,
    },
    .mipLevels = 1,
    .arrayLayers = 1,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    .tiling = VK_IMAGE_TILING_OPTIMAL,
    .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  };
  VkImageViewCreateInfo image_view_info = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .pNext = NULL,
    .image = VK_NULL_HANDLE,
    .format = vk_fb->depth_format,
    .subresourceRange = {
      .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
      .baseMipLevel = 0,
      .levelCount = 1,
      .baseArrayLayer = 0,
      .layerCount = 1,
    },
    .flags = 0,
    .viewType = VK_IMAGE_VIEW_TYPE_2D,
  };
  VkMemoryAllocateInfo mem_info = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
  };
  VkMemoryRequirements mem_reqs;
  VkImageMemoryBarrier image_barrier = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .srcAccessMask = 0,
    .dstAccessMask = (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT),
    .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    .newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .subresourceRange = {
      .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
      .baseMipLevel = 0,
      .levelCount = 1,
      .baseArrayLayer = 0,
      .layerCount = 1,
    },
  };

  VK_RET_VAL_ERROR ( ctx,
                     vkCreateImage (vk_ctx->device, &image_info, NULL,
                                    &vk_fb->depth_image),
                     FALSE,
                     error, COGL_FRAMEBUFFER_ERROR,
                     COGL_FRAMEBUFFER_ERROR_ALLOCATE );

  VK ( ctx,
       vkGetImageMemoryRequirements (vk_ctx->device,
                                     vk_fb->depth_image, &mem_reqs) );

  mem_info.allocationSize = mem_reqs.size;
  mem_info.memoryTypeIndex =
        _cogl_vulkan_context_get_memory_heap (framebuffer->context,
                                              mem_reqs.memoryTypeBits);
  VK_RET_VAL_ERROR ( ctx,
                     vkAllocateMemory (vk_ctx->device, &mem_info, NULL,
                                       &vk_fb->depth_memory),
                     FALSE,
                     error, COGL_FRAMEBUFFER_ERROR,
                     COGL_FRAMEBUFFER_ERROR_ALLOCATE );

  VK_RET_VAL_ERROR ( ctx,
                     vkBindImageMemory (vk_ctx->device, vk_fb->depth_image,
                                        vk_fb->depth_memory, 0),
                     FALSE,
                     error, COGL_FRAMEBUFFER_ERROR,
                     COGL_FRAMEBUFFER_ERROR_ALLOCATE );

  image_view_info.image = vk_fb->depth_image;
  VK_RET_VAL_ERROR ( ctx,
                     vkCreateImageView (vk_ctx->device, &image_view_info, NULL,
                                        &vk_fb->depth_image_view),
                     FALSE,
                     error, COGL_FRAMEBUFFER_ERROR,
                     COGL_FRAMEBUFFER_ERROR_ALLOCATE );

  _cogl_framebuffer_vulkan_ensure_command_buffer (framebuffer);

  image_barrier.image = vk_fb->depth_image;

  VK ( ctx, vkCmdPipelineBarrier (vk_fb->cmd_buffer,
                                  VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
                                  VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
                                  0,
                                  0, NULL,
                                  0, NULL,
                                  1, &image_barrier) );

  return TRUE;
}

void
_cogl_framebuffer_vulkan_deinit (CoglFramebuffer *framebuffer)
{
  CoglContext *ctx = framebuffer->context;
  CoglContextVulkan *vk_ctx = ctx->winsys;
  CoglFramebufferVulkan *vk_fb = framebuffer->winsys;

  if (vk_fb->cmd_buffers->len > 0)
    {
      /* TODO: We might need to wait for any in flight command buffer. */
      VK ( ctx,
           vkFreeCommandBuffers (vk_ctx->device, vk_ctx->cmd_pool,
                                 vk_fb->cmd_buffers->len,
                                 (VkCommandBuffer *) vk_fb->cmd_buffers->data) );
    }
  g_array_unref (vk_fb->cmd_buffers);

  g_ptr_array_unref (vk_fb->attribute_buffers);
  _cogl_framebuffer_vulkan_unref_pipelines (framebuffer, vk_fb);
  g_ptr_array_unref (vk_fb->pipelines);

  if (vk_fb->render_pass != VK_NULL_HANDLE)
    VK ( ctx, vkDestroyRenderPass (vk_ctx->device, vk_fb->render_pass, NULL) );
  if (vk_fb->fence != VK_NULL_HANDLE)
    VK ( ctx, vkDestroyFence (vk_ctx->device, vk_fb->fence, NULL) );
  if (vk_fb->depth_image_view != VK_NULL_HANDLE)
    VK ( ctx, vkDestroyImageView (vk_ctx->device, vk_fb->depth_image_view,
                                  NULL) );
  if (vk_fb->depth_image != VK_NULL_HANDLE)
    VK ( ctx, vkDestroyImage (vk_ctx->device, vk_fb->depth_image, NULL) );
  if (vk_fb->depth_memory != VK_NULL_HANDLE)
    VK ( ctx, vkFreeMemory (vk_ctx->device, vk_fb->depth_memory, NULL) );
}

CoglBool
_cogl_framebuffer_vulkan_init (CoglFramebuffer *framebuffer,
                               VkFormat color_format,
                               CoglError **error)
{
  CoglContext *ctx = framebuffer->context;
  CoglContextVulkan *vk_ctx = ctx->winsys;
  CoglFramebufferVulkan *vk_fb = framebuffer->winsys;
  VkAttachmentDescription attachments_description[2] = {
    [0] = {
      .format = color_format,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    },
    [1] = {
      .format = VK_FORMAT_D24_UNORM_S8_UINT,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .initialLayout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      .finalLayout =
      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    },
  };
  VkAttachmentReference color_reference = {
    .attachment = 0,
    .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
  };
  VkAttachmentReference depth_reference = {
    .attachment = 1,
    .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
  };
  VkSubpassDescription subpass_description = {
    .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
    .flags = 0,
    .inputAttachmentCount = 0,
    .pInputAttachments = NULL,
    .colorAttachmentCount = 1,
    .pColorAttachments = &color_reference,
    .pResolveAttachments = NULL,
    .pDepthStencilAttachment = NULL,
    .preserveAttachmentCount = 0,
    .pPreserveAttachments = NULL,
  };
  VkRenderPassCreateInfo render_pass_info = {
    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
    .pNext = NULL,
    .attachmentCount = framebuffer->depth_writing_enabled ? 2 : 1,
    .pAttachments = attachments_description,
    .subpassCount = 1,
    .pSubpasses = &subpass_description,
    .dependencyCount = 0,
    .pDependencies = NULL,
  };

  vk_fb->cmd_buffers = g_array_new (FALSE, TRUE, sizeof (VkCommandBuffer));
  vk_fb->attribute_buffers =
    g_ptr_array_new_full (20, (GDestroyNotify) cogl_object_unref);
  vk_fb->pipelines = g_ptr_array_new_full (10, NULL);

  if (framebuffer->depth_writing_enabled)
    {
      vk_fb->depth_format = VK_FORMAT_D24_UNORM_S8_UINT;
      if (!_cogl_framebuffer_vulkan_allocate_depth_buffer (framebuffer, error))
        return FALSE;

      subpass_description.pDepthStencilAttachment = &depth_reference;
    }

  vk_fb->color_format = color_format;

  VK_RET_VAL_ERROR ( ctx,
                     vkCreateFence (vk_ctx->device, &(VkFenceCreateInfo) {
                         .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                         .flags = VK_FENCE_CREATE_SIGNALED_BIT,
                       },
                       NULL,
                       &vk_fb->fence),
                     FALSE,
                     error, COGL_FRAMEBUFFER_ERROR,
                     COGL_FRAMEBUFFER_ERROR_ALLOCATE );

  VK_RET_VAL_ERROR ( ctx,
                     vkCreateRenderPass (vk_ctx->device, &render_pass_info,
                                         NULL, &vk_fb->render_pass),
                     FALSE,
                     error, COGL_FRAMEBUFFER_ERROR,
                     COGL_FRAMEBUFFER_ERROR_ALLOCATE );

  return TRUE;
}

VkResult
_cogl_framebuffer_vulkan_create_framebuffer (CoglFramebuffer *framebuffer,
                                             VkImageView vk_image_view,
                                             VkFramebuffer *vk_framebuffer)
{
  CoglContext *ctx = framebuffer->context;
  CoglContextVulkan *vk_ctx = ctx->winsys;
  CoglFramebufferVulkan *vk_fb = framebuffer->winsys;
  VkImageView image_views[2] = {
    vk_image_view,
    vk_fb->depth_image_view
  };
  VkFramebufferCreateInfo framebuffer_info = {
    .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
    .attachmentCount = framebuffer->depth_writing_enabled ? 2 : 1,
    .pAttachments = image_views,
    .width = framebuffer->width,
    .height = framebuffer->height,
    .layers = 1,
    .renderPass = vk_fb->render_pass,
  };

  return VK (ctx, vkCreateFramebuffer (vk_ctx->device, &framebuffer_info, NULL,
                                       vk_framebuffer) );
}

void
_cogl_framebuffer_vulkan_update_framebuffer (CoglFramebuffer *framebuffer,
                                             VkFramebuffer vk_framebuffer,
                                             VkImage vk_image)
{
  CoglFramebufferVulkan *vk_fb = framebuffer->winsys;

  _cogl_framebuffer_vulkan_flush_state (framebuffer, framebuffer, 0);

  vk_fb->framebuffer = vk_framebuffer;
  vk_fb->color_image = vk_image;
}

void
_cogl_framebuffer_vulkan_begin_render_pass (CoglFramebuffer *framebuffer)
{
  CoglContext *ctx = framebuffer->context;
  CoglFramebufferVulkan *vk_fb = framebuffer->winsys;
  VkClearValue clear_values[2];
  VkRenderPassBeginInfo render_begin_info = {
    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
    .renderPass = vk_fb->render_pass,
    .framebuffer = vk_fb->framebuffer,
    .renderArea = {
      { 0, 0 },
      { cogl_framebuffer_get_width (framebuffer),
        cogl_framebuffer_get_height (framebuffer) },
    },
    .pClearValues = clear_values,
    .clearValueCount = framebuffer->depth_writing_enabled ? 2 : 1,
  };

  if (vk_fb->render_pass_started)
    return;

  _cogl_framebuffer_vulkan_ensure_command_buffer (framebuffer);

  if (cogl_is_offscreen (framebuffer))
    _cogl_offscreen_vulkan_prepare_for_rendering (framebuffer);

  memset (clear_values, 0, sizeof (clear_values));

  VK ( ctx,
       vkCmdBeginRenderPass (vk_fb->cmd_buffer, &render_begin_info,
                             VK_SUBPASS_CONTENTS_INLINE) );

  vk_fb->render_pass_started = TRUE;
}

static void
_cogl_framebuffer_vulkan_flush_viewport_state (CoglFramebuffer *framebuffer)
{
  CoglContext *ctx = framebuffer->context;
  CoglFramebufferVulkan *vk_fb = framebuffer->winsys;
  VkViewport vk_viewport;

  g_assert (framebuffer->viewport_width >=0 &&
            framebuffer->viewport_height >=0);

  _cogl_framebuffer_vulkan_begin_render_pass (framebuffer);

  vk_viewport.x = framebuffer->viewport_x;
  vk_viewport.y = framebuffer->viewport_y;
  vk_viewport.width = framebuffer->viewport_width;
  vk_viewport.height = framebuffer->viewport_height;
  vk_viewport.minDepth = 0;
  vk_viewport.maxDepth = 1;

  /* Convert the Cogl viewport y offset to an OpenGL viewport y offset
   * NB: OpenGL defines its window and viewport origins to be bottom
   * left, while Cogl defines them to be top left.
   * NB: We render upside down to offscreen framebuffers so we don't
   * need to convert the y offset in this case. */
  /* if (cogl_is_offscreen (framebuffer)) */
  /*   vk_viewport.y = framebuffer->viewport_y; */
  /* else */
  /*   vk_viewport.y = framebuffer->height - */
  /*     (framebuffer->viewport_y + framebuffer->viewport_height); */

  COGL_NOTE (VULKAN, "Setting viewport to (%f, %f, %f, %f)",
             vk_viewport.x,
             vk_viewport.y,
             vk_viewport.width,
             vk_viewport.height);

  VK ( ctx, vkCmdSetViewport (vk_fb->cmd_buffer, 0, 1, &vk_viewport) );
}

void
_cogl_clip_stack_vulkan_flush (CoglClipStack *stack,
                               CoglFramebuffer *framebuffer)
{
  CoglContext *ctx = framebuffer->context;
  CoglFramebufferVulkan *vk_fb;
  int x0, y0, x1, y1;
  VkRect2D vk_rect;

  if (G_UNLIKELY (!framebuffer->allocated))
    cogl_framebuffer_allocate (framebuffer, NULL);

  vk_fb = framebuffer->winsys;
  _cogl_framebuffer_vulkan_begin_render_pass (framebuffer);

  _cogl_clip_stack_get_bounds (stack, &x0, &y0, &x1, &y1);

  vk_rect.offset.x = x0;
  vk_rect.offset.y = y0;
  vk_rect.extent.width = x1 - x0;
  vk_rect.extent.height = y1 - y0;

  VK ( ctx, vkCmdSetScissor (vk_fb->cmd_buffer, 0, 1, &vk_rect) );
}

static void
_cogl_framebuffer_vulkan_end_render_pass (CoglFramebuffer *framebuffer)
{
  CoglContext *ctx = framebuffer->context;
  CoglFramebufferVulkan *vk_fb = framebuffer->winsys;

  if (!vk_fb->render_pass_started)
    return;

  VK ( ctx, vkCmdEndRenderPass (vk_fb->cmd_buffer) );
  vk_fb->render_pass_started = FALSE;
}

void
_cogl_framebuffer_vulkan_end (CoglFramebuffer *framebuffer, CoglBool wait_fence)
{
  CoglContext *ctx = framebuffer->context;
  CoglContextVulkan *vk_ctx = ctx->winsys;
  CoglFramebufferVulkan *vk_fb = framebuffer->winsys;
  CoglError *error = NULL;

  /* We only want to flush if commands have been emitted. */
  if (vk_fb->cmd_buffer == VK_NULL_HANDLE)
    return;

  _cogl_framebuffer_vulkan_end_render_pass (framebuffer);

  VK ( ctx, vkEndCommandBuffer (vk_fb->cmd_buffer) );

  if (wait_fence)
    {
      VK_ERROR ( ctx, vkResetFences (vk_ctx->device, 1, &vk_fb->fence),
                 &error, COGL_DRIVER_ERROR, COGL_DRIVER_ERROR_INTERNAL );

      VK_ERROR ( ctx,
                 vkQueueSubmit (vk_ctx->queue, 1,
                                &(VkSubmitInfo) {
                                  .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                  .commandBufferCount = 1,
                                  .pCommandBuffers = &vk_fb->cmd_buffer,
                                }, vk_fb->fence),
                 &error, COGL_DRIVER_ERROR, COGL_DRIVER_ERROR_INTERNAL );

      VK_ERROR (ctx,
                vkWaitForFences (vk_ctx->device,
                                 1, (VkFence[]) { vk_fb->fence },
                                 VK_TRUE, INT64_MAX),
                &error, COGL_DRIVER_ERROR, COGL_DRIVER_ERROR_INTERNAL );

      VK ( ctx,
           vkFreeCommandBuffers (vk_ctx->device, vk_ctx->cmd_pool,
                                 vk_fb->cmd_buffers->len,
                                 (VkCommandBuffer *) vk_fb->cmd_buffers->data) );

      /* Do this first to avoid reentrant calls when freeing the pipelines. */
      vk_fb->cmd_buffer = VK_NULL_HANDLE;
      vk_fb->cmd_buffer_length = 0;

      g_array_set_size (vk_fb->cmd_buffers, 0);
      g_ptr_array_set_size (vk_fb->attribute_buffers, 0);
      _cogl_framebuffer_vulkan_unref_pipelines (framebuffer, vk_fb);
    }
  else
    {
      VK_ERROR ( ctx,
                 vkQueueSubmit (vk_ctx->queue, 1,
                                &(VkSubmitInfo) {
                                  .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                  .commandBufferCount = 1,
                                  .pCommandBuffers = &vk_fb->cmd_buffer,
                                }, VK_NULL_HANDLE),
                 &error, COGL_DRIVER_ERROR, COGL_DRIVER_ERROR_INTERNAL );

      vk_fb->cmd_buffer = VK_NULL_HANDLE;
      vk_fb->cmd_buffer_length = 0;
    }

  return;

 error:
  g_warning ("%s", error->message);
  _cogl_clear_error (&error);
}

void
_cogl_framebuffer_vulkan_ensure_clean_command_buffer (CoglFramebuffer *framebuffer)
{
  CoglFramebufferVulkan *vk_fb = framebuffer->winsys;

  if (vk_fb->cmd_buffer_length < 1)
    return;

  _cogl_framebuffer_vulkan_end (framebuffer, TRUE);
  _cogl_framebuffer_vulkan_begin_render_pass (framebuffer);
}

void
_cogl_framebuffer_vulkan_flush_state (CoglFramebuffer *draw_buffer,
                                      CoglFramebuffer *read_buffer,
                                      CoglFramebufferState state)
{
  CoglContext *ctx = draw_buffer->context;

  if (state & COGL_FRAMEBUFFER_STATE_INDEX_MODELVIEW)
    _cogl_context_set_current_modelview_entry (draw_buffer->context,
                                               _cogl_framebuffer_get_modelview_entry (draw_buffer));

  if (state & COGL_FRAMEBUFFER_STATE_INDEX_PROJECTION)
    _cogl_context_set_current_projection_entry (draw_buffer->context,
                                                _cogl_framebuffer_get_projection_entry (draw_buffer));

  /* Ensure ordering by ending the previous drawn to framebuffer. */
  if (ctx->current_draw_buffer != NULL &&
      ctx->current_draw_buffer != draw_buffer)
    _cogl_framebuffer_vulkan_end (ctx->current_draw_buffer, FALSE);

  ctx->current_draw_buffer = draw_buffer;
}

void
_cogl_framebuffer_vulkan_clear (CoglFramebuffer *framebuffer,
                                unsigned long buffers,
                                float red,
                                float green,
                                float blue,
                                float alpha)
{
  CoglContext *ctx = framebuffer->context;
  CoglFramebufferVulkan *vk_fb;
  VkClearAttachment clear_attachments[2];
  VkClearRect rect = {
    .rect = {
      .offset = {
        .x = 0,
        .y = 0,
      },
      .extent = {
        .width = framebuffer->width,
        .height = framebuffer->height,
      },
    },
    .baseArrayLayer = 0,
    .layerCount = 1,
  };
  uint32_t count = 0;

  /* TODO: maybe move this into
     _cogl_framebuffer_vulkan_ensure_command_buffer ? */
  if (G_UNLIKELY (!framebuffer->allocated))
    cogl_framebuffer_allocate (framebuffer, NULL);

  vk_fb = framebuffer->winsys;

  _cogl_framebuffer_vulkan_begin_render_pass (framebuffer);

  memset (clear_attachments, 0, sizeof (clear_attachments));
  if (buffers & COGL_BUFFER_BIT_COLOR)
    {
      clear_attachments[count].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      clear_attachments[count].colorAttachment = 0;
      clear_attachments[count].clearValue.color.float32[0] = red;
      clear_attachments[count].clearValue.color.float32[1] = green;
      clear_attachments[count].clearValue.color.float32[2] = blue;
      clear_attachments[count].clearValue.color.float32[3] = alpha;
      count++;
    }
  if (buffers & COGL_BUFFER_BIT_DEPTH)
    {
      clear_attachments[count].aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
      clear_attachments[count].clearValue.depthStencil.depth = 1.0;
      clear_attachments[count].clearValue.depthStencil.stencil = 0;
      count++;
    }

  VK ( ctx, vkCmdClearAttachments (vk_fb->cmd_buffer,
                                   count, clear_attachments,
                                   1, &rect) );
}

void
_cogl_framebuffer_vulkan_query_bits (CoglFramebuffer *framebuffer,
                                     CoglFramebufferBits *bits)
{
  uint64_t bit_field =
    _cogl_pixel_format_query_bits (framebuffer->internal_format);

  bits->alpha = _COGL_COLOR_BITS_GET_ALPHA (bit_field);
  bits->red = _COGL_COLOR_BITS_GET_RED (bit_field);
  bits->green = _COGL_COLOR_BITS_GET_GREEN (bit_field);
  bits->blue = _COGL_COLOR_BITS_GET_BLUE (bit_field);

  /* TODO: Hardcoded for now. */
  bits->depth = 16;
  bits->stencil = 0;
}

void
_cogl_framebuffer_vulkan_finish (CoglFramebuffer *framebuffer)
{
  _cogl_framebuffer_vulkan_end (framebuffer, TRUE);
}

void
_cogl_framebuffer_vulkan_discard_buffers (CoglFramebuffer *framebuffer,
                                          unsigned long buffers)
{
}

void
_cogl_framebuffer_vulkan_draw_attributes (CoglFramebuffer *framebuffer,
                                          CoglPipeline *pipeline,
                                          CoglVerticesMode mode,
                                          int first_vertex,
                                          int n_vertices,
                                          CoglAttribute **attributes,
                                          int n_attributes,
                                          CoglDrawFlags flags)
{
  CoglContext *ctx = framebuffer->context;
  CoglFramebufferVulkan *vk_fb = framebuffer->winsys;
  int i;

  vk_fb->vertices_modes[vk_fb->n_vertices_modes++] = mode;
  g_assert(vk_fb->n_vertices_modes < G_N_ELEMENTS (vk_fb->vertices_modes));

  _cogl_framebuffer_vulkan_begin_render_pass (framebuffer);

  _cogl_flush_attributes_state (framebuffer, pipeline, flags,
                                attributes, n_attributes);

  _cogl_framebuffer_vulkan_flush_viewport_state (framebuffer);

  VK ( ctx, vkCmdDraw (vk_fb->cmd_buffer, n_vertices, 1, first_vertex, 0) );
  vk_fb->cmd_buffer_length++;

  vk_fb->n_vertices_modes--;
}

void
_cogl_framebuffer_vulkan_draw_indexed_attributes (CoglFramebuffer *framebuffer,
                                                  CoglPipeline *pipeline,
                                                  CoglVerticesMode mode,
                                                  int first_vertex,
                                                  int n_vertices,
                                                  CoglIndices *indices,
                                                  CoglAttribute **attributes,
                                                  int n_attributes,
                                                  CoglDrawFlags flags)
{
  CoglBuffer *indices_buffer = COGL_BUFFER (indices->buffer);
  CoglBufferVulkan *vk_buf = indices_buffer->winsys;
  CoglContext *ctx = framebuffer->context;
  CoglFramebufferVulkan *vk_fb = framebuffer->winsys;

  vk_fb->vertices_modes[vk_fb->n_vertices_modes++] = mode;
  g_assert(vk_fb->n_vertices_modes < G_N_ELEMENTS (vk_fb->vertices_modes));

  _cogl_framebuffer_vulkan_begin_render_pass (framebuffer);

  _cogl_flush_attributes_state (framebuffer, pipeline, flags,
                                attributes, n_attributes);

  _cogl_framebuffer_vulkan_flush_viewport_state (framebuffer);

  VK ( ctx, vkCmdBindIndexBuffer (vk_fb->cmd_buffer, vk_buf->buffer,
                                  indices->offset,
                                  _cogl_indices_type_to_vulkan_indices_type (indices->type)) );

  VK ( ctx, vkCmdDrawIndexed (vk_fb->cmd_buffer,
                              n_vertices, 1,
                              indices->offset, first_vertex,
                              1 /* TODO: Figure out why 1... */) );
  vk_fb->cmd_buffer_length++;

  g_ptr_array_add (vk_fb->attribute_buffers, cogl_object_ref (indices->buffer));

  vk_fb->n_vertices_modes--;
}

CoglBool
_cogl_framebuffer_vulkan_read_pixels_into_bitmap (CoglFramebuffer *framebuffer,
                                                  int x,
                                                  int y,
                                                  CoglReadPixelsFlags source,
                                                  CoglBitmap *bitmap,
                                                  CoglError **error)
{
  uint32_t buffer_size = bitmap->rowstride * bitmap->height;
  CoglContext *ctx = framebuffer->context;
  CoglContextVulkan *vk_ctx = ctx->winsys;
  CoglTexture *dst_texture = NULL, *src_texture = NULL;
  CoglFramebuffer *offscreen = NULL;
  CoglFramebufferVulkan *vk_fb = framebuffer->winsys, *vk_dst_fb;
  CoglPipeline *pipeline = NULL;
  CoglBool ret = FALSE;
  VkImage dst_image = VK_NULL_HANDLE;
  VkDeviceMemory dst_image_memory = VK_NULL_HANDLE;
  VkComponentMapping vk_src_component_mapping, vk_dst_component_mapping;
  VkImageCreateInfo image_create_info = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT,
    .imageType = VK_IMAGE_TYPE_2D,
    .format = _cogl_pixel_format_to_vulkan_format (ctx, bitmap->format, NULL,
                                                   &vk_dst_component_mapping),
    .extent = {
      .width = bitmap->width,
      .height = bitmap->height,
      .depth = 1,
    },
    .mipLevels = 1,
    .arrayLayers = 1,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    .tiling = VK_IMAGE_TILING_LINEAR,
    .usage = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
              VK_IMAGE_USAGE_TRANSFER_DST_BIT),
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  };
  VkMemoryRequirements reqs;

  VK_ERROR ( ctx,
             vkCreateImage (vk_ctx->device, &image_create_info,
                            NULL, &dst_image),
             error, COGL_DRIVER_ERROR, COGL_DRIVER_ERROR_INTERNAL );

  if (bitmap->buffer)
    {
      VK_TODO ();
      goto error;
    }
  else if (bitmap->shared_bmp)
    {
      VK_TODO ();
      goto error;
    }
  else
    {
      VkMemoryAllocateInfo allocate_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      };

      VK ( ctx, vkGetImageMemoryRequirements (vk_ctx->device,
                                              dst_image,
                                              &reqs) );

      allocate_info.allocationSize = reqs.size;
      allocate_info.memoryTypeIndex =
        _cogl_vulkan_context_get_memory_heap (ctx,
                                              reqs.memoryTypeBits);
      VK_ERROR ( ctx,
                 vkAllocateMemory (vk_ctx->device, &allocate_info, NULL,
                                   &dst_image_memory),
                 error, COGL_DRIVER_ERROR, COGL_DRIVER_ERROR_INTERNAL );

      VK_ERROR ( ctx,
                 vkBindImageMemory (vk_ctx->device, dst_image,
                                    dst_image_memory, 0),
                 error, COGL_DRIVER_ERROR, COGL_DRIVER_ERROR_INTERNAL );
    }

  _cogl_framebuffer_vulkan_ensure_command_buffer (framebuffer);

  /* End any pending drawing operation on the source framebuffer. */
  _cogl_journal_flush (framebuffer->journal);
  _cogl_framebuffer_vulkan_end_render_pass (framebuffer);

  _cogl_pixel_format_to_vulkan_format (ctx, framebuffer->internal_format, NULL,
                                       &vk_src_component_mapping);
  src_texture =
    COGL_TEXTURE (_cogl_texture_2d_vulkan_new_for_foreign (ctx,
                                                           framebuffer->width,
                                                           framebuffer->height,
                                                           vk_fb->color_image,
                                                           _cogl_vulkan_format_unorm (vk_fb->color_format),
                                                           vk_src_component_mapping,
                                                           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                                           (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                                                            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)));
  if (!cogl_texture_allocate (src_texture, error))
    goto error;

  cogl_framebuffer_finish (framebuffer);

  dst_texture =
    COGL_TEXTURE (_cogl_texture_2d_vulkan_new_for_foreign (ctx,
                                                           bitmap->width,
                                                           bitmap->height,
                                                           dst_image,
                                                           _cogl_vulkan_format_unorm (image_create_info.format),
                                                           vk_dst_component_mapping,
                                                           image_create_info.initialLayout,
                                                           0));

  if (!cogl_texture_allocate (dst_texture, error))
    goto error;

  offscreen = COGL_FRAMEBUFFER (cogl_offscreen_new_with_texture (dst_texture));
  cogl_framebuffer_orthographic (offscreen,
                                 0, 0, bitmap->width, bitmap->height,
                                 -1 /* near */, 1 /* far */);
  cogl_framebuffer_set_depth_write_enabled (offscreen, FALSE);

  if (!cogl_framebuffer_allocate (offscreen, error))
    goto error;

  vk_dst_fb = offscreen->winsys;

  pipeline = cogl_pipeline_new (offscreen->context);
  cogl_pipeline_set_layer_texture (pipeline, 0, src_texture);
  cogl_pipeline_set_layer_filters (pipeline, 0,
                                   COGL_PIPELINE_FILTER_NEAREST,
                                   COGL_PIPELINE_FILTER_NEAREST);
  cogl_pipeline_set_blend (pipeline, "RGBA = ADD(SRC_COLOR, 0)", NULL);

  cogl_framebuffer_draw_textured_rectangle (offscreen, pipeline,
                                            0, 0, bitmap->width, bitmap->height,
                                            (float) x / (float) framebuffer->width,
                                            (float) y / (float) framebuffer->height,
                                            (float) (x + bitmap->width) / (float) framebuffer->width,
                                            (float) (y + bitmap->height) / (float) framebuffer->height);

  _cogl_journal_flush (offscreen->journal);

  _cogl_framebuffer_vulkan_end_render_pass (offscreen);

  /* Move offscreen framebuffer to host. */
  _cogl_texture_vulkan_move_to (dst_texture,
                                COGL_TEXTURE_DOMAIN_HOST,
                                vk_dst_fb->cmd_buffer);

  /* Put the framebuffer back as an attachment. */
  _cogl_texture_vulkan_move_to (src_texture,
                                COGL_TEXTURE_DOMAIN_ATTACHMENT,
                                vk_dst_fb->cmd_buffer);

  cogl_framebuffer_finish (offscreen);

  /**/
  if (dst_image_memory != VK_NULL_HANDLE)
    {
      void *data;

      VK_ERROR (ctx,
                vkMapMemory (vk_ctx->device, dst_image_memory,
                             0, reqs.size, 0,
                             &data),
                error, COGL_BUFFER_ERROR, COGL_BUFFER_ERROR_MAP );

      // TODO: check rowstride
      memcpy (bitmap->data, data, buffer_size);

      VK (ctx, vkUnmapMemory (vk_ctx->device, dst_image_memory) );
    }

  ret = TRUE;

 error:
  if (pipeline)
    cogl_object_unref (pipeline);
  if (offscreen)
    cogl_object_unref (offscreen);
  if (dst_texture)
    cogl_object_unref (dst_texture);
  if (src_texture)
    cogl_object_unref (src_texture);
  if (dst_image)
    VK ( ctx, vkDestroyImage (vk_ctx->device, dst_image, NULL) );
  if (dst_image_memory)
    VK ( ctx, vkFreeMemory (vk_ctx->device, dst_image_memory, NULL) );

  return ret;
}

void
_cogl_offscreen_vulkan_free (CoglOffscreen *offscreen)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (offscreen);
  CoglContext *ctx = framebuffer->context;
  CoglContextVulkan *vk_ctx = ctx->winsys;
  CoglOffscreenVulkan *vk_off = framebuffer->winsys;

  if (vk_off == NULL)
    return;

  _cogl_framebuffer_vulkan_end (framebuffer, TRUE);

  _cogl_framebuffer_vulkan_deinit (framebuffer);

  if (vk_off->framebuffer != VK_NULL_HANDLE)
    VK (ctx,
        vkDestroyFramebuffer (vk_ctx->device, vk_off->framebuffer, NULL) );
  if (vk_off->image_view != VK_NULL_HANDLE)
    VK ( ctx,
         vkDestroyImageView (vk_ctx->device, vk_off->image_view, NULL) );

  g_slice_free (CoglOffscreenVulkan, framebuffer->winsys);
}

CoglBool
_cogl_offscreen_vulkan_allocate (CoglOffscreen *offscreen,
                                 CoglError **error)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (offscreen);
  CoglContext *ctx = framebuffer->context;
  CoglContextVulkan *vk_ctx = ctx->winsys;
  CoglOffscreenVulkan *vk_off = g_slice_new0 (CoglOffscreenVulkan);
  CoglFramebufferVulkan *vk_fb = (CoglFramebufferVulkan *) vk_off;
  VkFormat format = _cogl_texture_get_vulkan_format (offscreen->texture);
  VkImage image = _cogl_texture_get_vulkan_image (offscreen->texture);
  VkImageViewCreateInfo image_view_create_info = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .flags = 0,
    .image = image,
    .viewType = VK_IMAGE_VIEW_TYPE_2D,
    .format = format,
    .components =
      _cogl_texture_get_vulkan_component_mapping (offscreen->texture),
    .subresourceRange = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseMipLevel = 0,
      .levelCount = 1,
      .baseArrayLayer = 0,
      .layerCount = 1,
    },
  };

  VkResult result;

  framebuffer->winsys = vk_fb;

  if (!_cogl_framebuffer_vulkan_init (framebuffer, format, error))
    return FALSE;

  VK_ERROR ( ctx,
             vkCreateImageView (vk_ctx->device, &image_view_create_info, NULL,
                                &vk_off->image_view),
             error, COGL_FRAMEBUFFER_ERROR, COGL_FRAMEBUFFER_ERROR_ALLOCATE);

  result =
    _cogl_framebuffer_vulkan_create_framebuffer (framebuffer,
                                                 vk_off->image_view,
                                                 &vk_off->framebuffer);
  if (result != VK_SUCCESS)
    {
      _cogl_set_error (error, COGL_FRAMEBUFFER_ERROR,
                       COGL_FRAMEBUFFER_ERROR_ALLOCATE,
                       "%s: VK error (%d): %s\n",
                       G_STRLOC,
                       result,
                       _cogl_vulkan_error_to_string (result));
      goto error;
    }

  _cogl_framebuffer_vulkan_update_framebuffer (framebuffer, vk_off->framebuffer,
                                               image);

  _cogl_framebuffer_vulkan_ensure_command_buffer (framebuffer);

  /* TODO: maybe check the layout is right before any rendering? */
  _cogl_texture_vulkan_move_to (offscreen->texture,
                                COGL_TEXTURE_DOMAIN_ATTACHMENT,
                                vk_fb->cmd_buffer);

  return TRUE;

 error:
  _cogl_offscreen_vulkan_free (offscreen);
  return FALSE;
}
