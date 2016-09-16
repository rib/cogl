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

#include <string.h>

#include "cogl-context-private.h"
#include "cogl-driver-vulkan-private.h"
#include "cogl-buffer-vulkan-private.h"
#include "cogl-error-private.h"
#include "cogl-util-vulkan-private.h"

static VkBufferUsageFlags
_cogl_buffer_usage_to_vulkan_buffer_usage (CoglBufferUsageHint usage)
{
  switch (usage)
    {
    case COGL_BUFFER_USAGE_HINT_TEXTURE:
      return (VK_BUFFER_USAGE_TRANSFER_DST_BIT |
              VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    case COGL_BUFFER_USAGE_HINT_ATTRIBUTE_BUFFER:
      return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    case COGL_BUFFER_USAGE_HINT_INDEX_BUFFER:
      return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    case COGL_BUFFER_USAGE_HINT_UNIFORM_BUFFER:
      return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    default:
      g_assert_not_reached();
    }
}

void
_cogl_buffer_vulkan_create (CoglBuffer *buffer)
{
  CoglContext *ctx = buffer->context;
  CoglContextVulkan *vk_ctx = ctx->winsys;
  CoglBufferVulkan *vk_buffer = g_slice_new0 (CoglBufferVulkan);
  VkMemoryRequirements mem_reqs;

  buffer->winsys = vk_buffer;

  VK_RET ( ctx,
           vkCreateBuffer (vk_ctx->device, &(VkBufferCreateInfo) {
               .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
               .size = buffer->size,
               .usage = _cogl_buffer_usage_to_vulkan_buffer_usage (buffer->usage_hint),
               .flags = VK_SHARING_MODE_EXCLUSIVE,
             },
             NULL,
             &vk_buffer->buffer) );

  VK ( ctx,
       vkGetBufferMemoryRequirements (vk_ctx->device,
                                      vk_buffer->buffer, &mem_reqs) );

  mem_reqs.memoryTypeBits |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
  /* vk_buffer->memory_need_flush = */
  /*   (mem_reqs.memoryTypeBits & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0; */

  VK_RET ( ctx,
           vkAllocateMemory (vk_ctx->device, &(VkMemoryAllocateInfo) {
               .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
               .allocationSize = mem_reqs.size,
               .memoryTypeIndex =
                 _cogl_vulkan_context_get_memory_heap (buffer->context,
                                                       mem_reqs.memoryTypeBits),
             },
             NULL,
             &vk_buffer->memory) );

  VK_RET ( ctx,
           vkBindBufferMemory (vk_ctx->device,
                               vk_buffer->buffer,
                               vk_buffer->memory, 0) );
}

void
_cogl_buffer_vulkan_destroy (CoglBuffer *buffer)
{
  CoglContext *ctx = buffer->context;
  CoglContextVulkan *vk_ctx = ctx->winsys;
  CoglBufferVulkan *vk_buffer = buffer->winsys;

  if (vk_buffer->buffer != VK_NULL_HANDLE)
    VK( ctx, vkDestroyBuffer (vk_ctx->device, vk_buffer->buffer, NULL) );
  if (vk_buffer->memory != VK_NULL_HANDLE)
    VK( ctx, vkFreeMemory (vk_ctx->device, vk_buffer->memory, NULL) );

  g_slice_free (CoglBufferVulkan, vk_buffer);
}

void *
_cogl_buffer_vulkan_map_range (CoglBuffer *buffer,
                               size_t offset,
                               size_t size,
                               CoglBufferAccess access,
                               CoglBufferMapHint hints,
                               CoglError **error)
{
  CoglContext *ctx = buffer->context;
  CoglContextVulkan *vk_ctx = ctx->winsys;
  CoglBufferVulkan *vk_buffer = buffer->winsys;
  void *data;

  if (vk_buffer->buffer == VK_NULL_HANDLE)
    {
      _cogl_set_error (error, COGL_BUFFER_ERROR,
                       COGL_BUFFER_ERROR_MAP,
                       "Buffer not allocated");
      return NULL;
    }

  VK_RET_VAL_ERROR ( ctx,
                     vkMapMemory (vk_ctx->device,
                                  vk_buffer->memory,
                                  offset,
                                  size,
                                  0,
                                  &data),
                     NULL,
                     error, COGL_BUFFER_ERROR, COGL_BUFFER_ERROR_MAP );

  buffer->flags |= COGL_BUFFER_FLAG_MAPPED;

  return data;
}

void
_cogl_buffer_vulkan_unmap (CoglBuffer *buffer)
{
  CoglContext *ctx = buffer->context;
  CoglContextVulkan *vk_ctx = ctx->winsys;
  CoglBufferVulkan *vk_buffer = buffer->winsys;

  if (vk_buffer->memory_need_flush)
    {
      vk_buffer->memory_need_flush = FALSE;

      VK_RET ( ctx,
               vkFlushMappedMemoryRanges (vk_ctx->device, 1, &(VkMappedMemoryRange) {
                   .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                   .memory = vk_buffer->memory,
                   .offset = 0,
                   .size = buffer->size,
                 }) );
    }

  VK (ctx, vkUnmapMemory (vk_ctx->device, vk_buffer->memory) );

  buffer->flags &= ~COGL_BUFFER_FLAG_MAPPED;
}

CoglBool
_cogl_buffer_vulkan_set_data (CoglBuffer *buffer,
                              unsigned int offset,
                              const void *data,
                              unsigned int size,
                              CoglError **error)
{
  void *data_map;

  if (buffer->flags & COGL_BUFFER_FLAG_MAPPED)
    {
      _cogl_set_error (error, COGL_BUFFER_ERROR,
                       COGL_BUFFER_ERROR_MAP,
                       "Cannot set data while the buffer is mapped");
      return FALSE;
    }

  data_map =
    _cogl_buffer_vulkan_map_range (buffer, offset, size,
                                   COGL_BUFFER_ACCESS_WRITE,
                                   COGL_BUFFER_MAP_HINT_DISCARD_RANGE,
                                   error);
  if (!data_map)
    return FALSE;

  memcpy ((uint8_t *) data_map + offset, data, size);

  _cogl_buffer_vulkan_unmap (buffer);

  return TRUE;
}

CoglBool
_cogl_buffer_vulkan_flush_mapped_memory (CoglBuffer *buffer,
                                         CoglError **error)
{
  CoglBufferVulkan *vk_buf = buffer->winsys;
  CoglContext *ctx = buffer->context;
  CoglContextVulkan *vk_ctx = ctx->winsys;
  VkMappedMemoryRange range = {
    .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
    .memory = vk_buf->memory,
    .offset = 0,
    .size = buffer->size,
  };

  VK_ERROR ( ctx,
             vkFlushMappedMemoryRanges (vk_ctx->device,
                                        1,
                                        &range),
             error, COGL_BUFFER_ERROR, COGL_BUFFER_ERROR_MAP /* TODO : new error */ );

  return TRUE;

 error:
  return FALSE;
}

CoglBool
_cogl_buffer_vulkan_invalidate_mapped_memory (CoglBuffer *buffer,
                                              CoglError **error)
{
  CoglBufferVulkan *vk_buf = buffer->winsys;
  CoglContext *ctx = buffer->context;
  CoglContextVulkan *vk_ctx = ctx->winsys;
  VkMappedMemoryRange range = {
    .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
    .memory = vk_buf->memory,
    .offset = 0,
    .size = buffer->size,
  };

  VK_ERROR ( ctx, vkInvalidateMappedMemoryRanges (vk_ctx->device,
                                                  1,
                                                  &range),
             error, COGL_BUFFER_ERROR, COGL_BUFFER_ERROR_MAP /* TODO: new error */ );

  return TRUE;

 error:
  return FALSE;
}

void
_cogl_buffer_vulkan_move_to_device (CoglBuffer *buffer,
                                    VkCommandBuffer cmd_buffer)
{
  CoglBufferVulkan *vk_buf = buffer->winsys;
  CoglContext *ctx = buffer->context;
  VkBufferMemoryBarrier buffer_barrier = {
    .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
    .srcAccessMask = vk_buf->access_mask,
    .dstAccessMask = (VK_ACCESS_TRANSFER_READ_BIT |
                      VK_ACCESS_TRANSFER_WRITE_BIT),
    .srcQueueFamilyIndex = 0,
    .dstQueueFamilyIndex = 0,
    .buffer = vk_buf->buffer,
    .offset = 0,
    .size = buffer->size,
  };

  if (vk_buf->access_mask == buffer_barrier.dstAccessMask)
    return;

  vk_buf->access_mask = buffer_barrier.dstAccessMask;

  VK ( ctx,
       vkCmdPipelineBarrier (cmd_buffer,
                             VK_PIPELINE_STAGE_HOST_BIT,
                             VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
                             0,
                             0, NULL,
                             1, &buffer_barrier,
                             0, NULL) );
}

void
_cogl_buffer_vulkan_move_to_host (CoglBuffer *buffer,
                                  VkCommandBuffer cmd_buffer)
{
  CoglBufferVulkan *vk_buf = buffer->winsys;
  CoglContext *ctx = buffer->context;
  VkBufferMemoryBarrier buffer_barrier = {
    .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
    .srcAccessMask = vk_buf->access_mask,
    .dstAccessMask = (VK_ACCESS_HOST_READ_BIT |
                      VK_ACCESS_HOST_WRITE_BIT),
    .srcQueueFamilyIndex = 0,
    .dstQueueFamilyIndex = 0,
    .buffer = vk_buf->buffer,
    .offset = 0,
    .size = buffer->size,
  };

  if (vk_buf->access_mask == buffer_barrier.dstAccessMask)
    return;

  vk_buf->access_mask = buffer_barrier.dstAccessMask;

  VK ( ctx,
       vkCmdPipelineBarrier (cmd_buffer,
                             VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
                             VK_PIPELINE_STAGE_HOST_BIT,
                             0,
                             0, NULL,
                             1, &buffer_barrier,
                             0, NULL) );
}
