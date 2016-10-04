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

#include <config.h>

#include <string.h>

#include "cogl-blit.h"
#include "cogl-buffer-vulkan-private.h"
#include "cogl-context-private.h"
#include "cogl-driver-vulkan-private.h"
#include "cogl-error-private.h"
#include "cogl-framebuffer-vulkan-private.h"
#include "cogl-private.h"
#include "cogl-renderer-private.h"
#include "cogl-texture-private.h"
#include "cogl-texture-2d-vulkan-private.h"
#include "cogl-texture-2d-private.h"
#include "cogl-util-vulkan-private.h"

typedef struct _CoglTexture2DVulkan CoglTexture2DVulkan;

struct _CoglTexture2DVulkan
{
  VkImage image;
  VkComponentMapping component_mapping;
  VkFormat format;
  VkImageTiling tiling;

  VkDeviceMemory memory;
  uint32_t memory_size;

  VkImageView image_view;

  VkImageLayout image_layout;
  VkAccessFlags access_mask;

  CoglBool has_mipmap;
};

void
_cogl_texture_2d_vulkan_free (CoglTexture2D *tex_2d)
{
  CoglContext *ctx = tex_2d->_parent.context;
  CoglContextVulkan *vk_ctx = ctx->winsys;
  CoglTexture2DVulkan *tex_2d_vk = tex_2d->winsys;

  if (tex_2d_vk->image_view != VK_NULL_HANDLE)
    VK ( ctx,
         vkDestroyImageView (vk_ctx->device, tex_2d_vk->image_view, NULL) );
  if (!tex_2d->is_foreign && tex_2d_vk->image != VK_NULL_HANDLE)
    VK ( ctx,
         vkDestroyImage (vk_ctx->device, tex_2d_vk->image, NULL) );
  if (tex_2d_vk->memory != VK_NULL_HANDLE)
    VK ( ctx,
         vkFreeMemory (vk_ctx->device, tex_2d_vk->memory, NULL) );

  g_slice_free (CoglTexture2DVulkan, tex_2d_vk);
}

CoglBool
_cogl_texture_2d_vulkan_can_create (CoglContext *ctx,
                                    int width,
                                    int height,
                                    CoglPixelFormat internal_format)
{
  CoglRenderer *renderer = ctx->display->renderer;
  CoglRendererVulkan *vk_renderer = renderer->winsys;

  if (_cogl_pixel_format_to_vulkan_format_for_sampling (ctx,
                                                        internal_format,
                                                        NULL,
                                                        NULL) == VK_FORMAT_UNDEFINED)
    return FALSE;

  if (width >= vk_renderer->physical_device_properties.limits.maxFramebufferWidth ||
      height >= vk_renderer->physical_device_properties.limits.maxFramebufferHeight)
    return FALSE;

  return TRUE;
}

void
_cogl_texture_2d_vulkan_init (CoglTexture2D *tex_2d)
{
  CoglTexture2DVulkan *tex_2d_vk = g_slice_new0 (CoglTexture2DVulkan);

  tex_2d_vk->image = VK_NULL_HANDLE;
  tex_2d_vk->image_view = VK_NULL_HANDLE;
  tex_2d_vk->memory = VK_NULL_HANDLE;

  tex_2d_vk->image_layout = VK_IMAGE_LAYOUT_UNDEFINED;
  tex_2d_vk->access_mask = 0;

  tex_2d->winsys = tex_2d_vk;
}

static CoglBool
create_image (CoglTexture2D *tex_2d,
              VkImageUsageFlags usage,
              VkImageTiling tiling,
              int width, int height,
              int mip_levels,
              CoglError **error)
{
  CoglTexture2DVulkan *tex_2d_vk = tex_2d->winsys;
  CoglTexture *tex = COGL_TEXTURE (tex_2d);
  CoglContext *ctx = tex->context;
  CoglContextVulkan *vk_ctx = ctx->winsys;
  VkImageCreateInfo image_create_info = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT,
    .imageType = VK_IMAGE_TYPE_2D,
    .format = tex_2d_vk->format,
    .extent = {
      .width = width,
      .height = height,
      .depth = 1
    },
    .mipLevels = mip_levels,
    .arrayLayers = 1,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    .tiling = tiling,
    .usage = (VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
              VK_IMAGE_USAGE_TRANSFER_DST_BIT |
              VK_IMAGE_USAGE_SAMPLED_BIT |
              VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT),
    .initialLayout = tex_2d_vk->image_layout,
  };

  tex_2d_vk->tiling = tiling;

  VK_RET_VAL_ERROR ( ctx,
                     vkCreateImage (vk_ctx->device, &image_create_info, NULL,
                                    &tex_2d_vk->image),
                     FALSE, error,
                     COGL_TEXTURE_ERROR, COGL_TEXTURE_ERROR_BAD_PARAMETER );

  return TRUE;
}

static CoglBool
allocate_image_memory (CoglTexture2D *tex_2d, CoglError **error)
{
  CoglTexture2DVulkan *tex_2d_vk = tex_2d->winsys;
  CoglTexture *tex = COGL_TEXTURE (tex_2d);
  CoglContext *ctx = tex->context;
  CoglContextVulkan *vk_ctx = ctx->winsys;
  VkMemoryRequirements reqs;
  VkMemoryAllocateInfo allocate_info = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
  };

  VK ( ctx, vkGetImageMemoryRequirements (vk_ctx->device,
                                          tex_2d_vk->image,
                                          &reqs) );

  allocate_info.allocationSize = tex_2d_vk->memory_size = reqs.size;
  allocate_info.memoryTypeIndex =
    _cogl_vulkan_context_get_memory_heap (ctx, reqs.memoryTypeBits);
  VK_RET_VAL_ERROR ( ctx,
                     vkAllocateMemory (vk_ctx->device, &allocate_info, NULL,
                                       &tex_2d_vk->memory),
                     FALSE, error,
                     COGL_TEXTURE_ERROR, COGL_TEXTURE_ERROR_BAD_PARAMETER );

  VK_RET_VAL_ERROR ( ctx,
                     vkBindImageMemory (vk_ctx->device, tex_2d_vk->image,
                                        tex_2d_vk->memory, 0),
                     FALSE, error,
                     COGL_TEXTURE_ERROR, COGL_TEXTURE_ERROR_BAD_PARAMETER );

  return TRUE;
}

static CoglBool
create_image_view (CoglTexture2D *tex_2d, CoglError **error)
{
  CoglTexture2DVulkan *tex_2d_vk = tex_2d->winsys;
  CoglTexture *tex = COGL_TEXTURE (tex_2d);
  CoglContext *ctx = tex->context;
  CoglContextVulkan *vk_ctx = ctx->winsys;
  VkImageViewCreateInfo image_view_create_info = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .flags = 0,
    .image = tex_2d_vk->image,
    .viewType = VK_IMAGE_VIEW_TYPE_2D,
    .format = tex_2d_vk->format,
    .components = tex_2d_vk->component_mapping,
    .subresourceRange = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseMipLevel = 0,
      .levelCount = 1,
      .baseArrayLayer = 0,
      .layerCount = 1,
    },
  };

  VK_RET_VAL_ERROR ( ctx,
                     vkCreateImageView (vk_ctx->device, &image_view_create_info, NULL,
                                        &tex_2d_vk->image_view),
                     FALSE, error,
                     COGL_TEXTURE_ERROR, COGL_TEXTURE_ERROR_BAD_PARAMETER);

  return TRUE;
}

static CoglBool
allocate_with_size (CoglTexture2D *tex_2d,
                    CoglTextureLoader *loader,
                    CoglError **error)
{
  CoglTexture2DVulkan *tex_2d_vk = tex_2d->winsys;
  CoglTexture *tex = COGL_TEXTURE (tex_2d);
  CoglPixelFormat internal_format =
    _cogl_texture_determine_internal_format (COGL_TEXTURE (tex_2d),
                                             COGL_PIXEL_FORMAT_ANY);
  int width = loader->src.sized.width, height = loader->src.sized.height;
  VkImageUsageFlags usage = (VK_IMAGE_USAGE_SAMPLED_BIT |
                             VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
  VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;

  tex_2d_vk->format =
    _cogl_pixel_format_to_vulkan_format_for_sampling (tex->context,
                                                      internal_format, NULL,
                                                      &tex_2d_vk->component_mapping);

  if (tex_2d_vk->format == VK_FORMAT_UNDEFINED)
    {
      _cogl_set_error (error, COGL_TEXTURE_ERROR,
                       COGL_TEXTURE_ERROR_BAD_PARAMETER,
                       "Failed to create texture 2d due to format constraints");
      return FALSE;
    }

  if (!create_image (tex_2d, usage, tiling, width, height,
                     1 + floor (log2 (MAX (width, height))), error))
    return FALSE;

  if (!allocate_image_memory (tex_2d, error))
    return FALSE;

  if (!create_image_view (tex_2d, error))
    return FALSE;

  tex_2d->internal_format = internal_format;
  _cogl_texture_set_allocated (tex, internal_format, width, height);

  return TRUE;
}

static CoglBool
load_bitmap_data_to_texture (CoglTexture2D *tex_2d,
                             int src_x,
                             int src_y,
                             int width,
                             int height,
                             CoglBitmap *bitmap,
                             int dst_x,
                             int dst_y,
                             CoglError **error)
{
  CoglTexture2DVulkan *tex_2d_vk = tex_2d->winsys;
  CoglTexture *tex = COGL_TEXTURE (tex_2d);
  CoglContext *ctx = tex->context;
  CoglContextVulkan *vk_ctx = ctx->winsys;
  CoglPixelFormat internal_format = bitmap->format;
  int format_bpp = _cogl_pixel_format_get_bytes_per_pixel (internal_format);
  void *src_data, *dst_data = NULL;
  CoglBool ret = FALSE;
  VkCommandBuffer cmd_buffer = VK_NULL_HANDLE;

  if (bitmap->buffer)
    src_data = cogl_buffer_map (bitmap->buffer, COGL_BUFFER_ACCESS_READ, 0);
  else
    src_data = bitmap->data;

  if (tex_2d_vk->image_layout != COGL_TEXTURE_DOMAIN_HOST)
    {
      if (!_cogl_vulkan_context_create_command_buffer (ctx, &cmd_buffer, error))
        goto error;

      _cogl_texture_vulkan_move_to (tex,
                                    COGL_TEXTURE_DOMAIN_HOST,
                                    cmd_buffer);

      if (!_cogl_vulkan_context_submit_command_buffer (ctx, cmd_buffer, error))
        goto error;
    }

  VK_ERROR ( ctx,
             vkMapMemory (vk_ctx->device,
                          tex_2d_vk->memory, 0,
                          tex_2d_vk->memory_size, 0,
                          &dst_data),
             error, COGL_TEXTURE_ERROR, COGL_TEXTURE_ERROR_BAD_PARAMETER );

  if (src_x == 0 && src_y == 0 &&
      dst_x == 0 && dst_y == 0 &&
      width == tex->width && height == tex->height &&
      bitmap->rowstride == width * format_bpp)
    {
      memcpy (dst_data, src_data, width * height * format_bpp);
    }
  else
    {
      int i, src_rowstride = bitmap->rowstride,
        dst_rowstride = tex->width * format_bpp;

      for (i = 0; i < height; i++) {
        memcpy ((uint8_t *) dst_data + (dst_y + i) * dst_rowstride + dst_x * format_bpp,
                (uint8_t *) src_data + (src_y + i) * src_rowstride + src_x * format_bpp,
                width * format_bpp);
      }
    }

  ret = TRUE;

 error:

  if (bitmap->buffer)
    cogl_buffer_unmap (bitmap->buffer);

  if (dst_data != NULL)
    VK ( ctx, vkUnmapMemory (vk_ctx->device, tex_2d_vk->memory) );

  if (cmd_buffer != VK_NULL_HANDLE)
    VK ( ctx,
         vkFreeCommandBuffers (vk_ctx->device, vk_ctx->cmd_pool,
                               1, &cmd_buffer) );

  return ret;
}

static CoglBool
load_bitmap_buffer_to_texture (CoglTexture2D *tex_2d,
                               CoglBitmap *bitmap,
                               int dst_x, int dst_y,
                               CoglError **error)
{
  CoglTexture2DVulkan *tex_2d_vk = tex_2d->winsys;
  CoglTexture *tex = COGL_TEXTURE (tex_2d);
  CoglContext *ctx = tex->context;
  CoglContextVulkan *vk_ctx = ctx->winsys;
  CoglBufferVulkan *vk_buf = bitmap->buffer->winsys;
  VkBufferImageCopy image_copy = {
    .bufferOffset = 0,
    .bufferRowLength = bitmap->width,
    .bufferImageHeight = bitmap->height,
    .imageSubresource = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .mipLevel = 0,
      .baseArrayLayer = 0,
      .layerCount = 1,
    },
    .imageOffset = { dst_x, dst_y, 0, },
    .imageExtent = { bitmap->width, bitmap->height, 1, },
  };
  VkCommandBuffer cmd_buffer = VK_NULL_HANDLE;
  CoglBool ret = FALSE;

  if (!_cogl_vulkan_context_create_command_buffer (ctx, &cmd_buffer, error))
    goto error;

  _cogl_buffer_vulkan_move_to_device (bitmap->buffer, cmd_buffer);

  _cogl_texture_vulkan_move_to (tex,
                                COGL_TEXTURE_DOMAIN_TRANSFER_DESTINATION,
                                cmd_buffer);

  VK ( ctx,
       vkCmdCopyBufferToImage (cmd_buffer,
                               vk_buf->buffer,
                               tex_2d_vk->image,
                               tex_2d_vk->image_layout,
                               1,
                               &image_copy) );

  _cogl_texture_vulkan_move_to (tex,
                                COGL_TEXTURE_DOMAIN_SAMPLING,
                                cmd_buffer);

  if (!_cogl_vulkan_context_submit_command_buffer (ctx, cmd_buffer, error))
    goto error;

  ret = TRUE;

 error:
  if (cmd_buffer != VK_NULL_HANDLE)
    VK ( ctx,
         vkFreeCommandBuffers (vk_ctx->device, vk_ctx->cmd_pool,
                               1, &cmd_buffer) );

  return ret;
}

static CoglBool
load_texture_to_texture (CoglTexture2D *tex_2d,
                         int src_x,
                         int src_y,
                         int width,
                         int height,
                         CoglTexture *src,
                         int dst_x,
                         int dst_y,
                         CoglError **error)
{
  CoglTexture2DVulkan *tex_2d_vk = tex_2d->winsys;
  CoglTexture *tex = COGL_TEXTURE (tex_2d);
  CoglBool ret = FALSE;
  CoglBlitData blit_data;

  if (!cogl_texture_allocate (tex, error))
    return FALSE;

  if (!cogl_texture_allocate (src, error))
    return FALSE;

  _cogl_blit_begin (&blit_data, tex, src);
  _cogl_blit (&blit_data, src_x, src_y, dst_x, dst_y, width, height);
  _cogl_blit_end (&blit_data);

  return TRUE;
}

static CoglBool
allocate_from_bitmap (CoglTexture2D *tex_2d,
                      CoglTextureLoader *loader,
                      CoglError **error)
{
  CoglTexture2DVulkan *tex_2d_vk = tex_2d->winsys;
  CoglTexture *tex = COGL_TEXTURE (tex_2d);
  CoglBitmap *bitmap = loader->src.bitmap.bitmap;
  CoglPixelFormat internal_format;
  CoglBool ret = FALSE;
  int width = bitmap->width, height = bitmap->height;
  VkImageUsageFlags usage;

  internal_format =
    _cogl_texture_determine_internal_format (tex,
                                             cogl_bitmap_get_format (bitmap));

  tex_2d_vk->format =
    _cogl_pixel_format_to_vulkan_format_for_sampling (tex->context,
                                                      internal_format, NULL,
                                                      &tex_2d_vk->component_mapping);
  if (tex_2d_vk->format == VK_FORMAT_UNDEFINED)
    internal_format = COGL_PIXEL_FORMAT_RGBA_8888;

  /* Override default tiling.

     TODO: Consider always using optimal tiling and blit to an intermediate
     linear buffer.
   */
  if (bitmap->shared_bmp || bitmap->buffer)
    {
      usage = (VK_IMAGE_USAGE_TRANSFER_DST_BIT |
               VK_IMAGE_USAGE_SAMPLED_BIT |
               VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    }
  else
    {
      tex_2d_vk->image_layout = VK_IMAGE_LAYOUT_PREINITIALIZED;
      tex_2d_vk->access_mask = VK_ACCESS_HOST_WRITE_BIT;
      usage = (VK_IMAGE_USAGE_SAMPLED_BIT |
               VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    }

  bitmap =
    _cogl_bitmap_convert_for_upload (bitmap,
                                     internal_format,
                                     loader->src.bitmap.can_convert_in_place,
                                     error);
  if (bitmap == NULL)
    goto error;

  tex_2d_vk->format =
    _cogl_pixel_format_to_vulkan_format_for_sampling (tex->context,
                                                      bitmap->format, NULL,
                                                      &tex_2d_vk->component_mapping);
  if (tex_2d_vk->format == VK_FORMAT_UNDEFINED)
    {
      _cogl_set_error (error, COGL_TEXTURE_ERROR,
                       COGL_TEXTURE_ERROR_BAD_PARAMETER,
                       "Failed to create texture 2d due to format constraints");
      goto error;
    }

  if (!create_image (tex_2d, usage, VK_IMAGE_TILING_LINEAR,
                     width, height, 1, error))
    goto error;

  if (!allocate_image_memory (tex_2d, error))
    goto error;

  if (bitmap->shared_bmp)
    {
      VK_TODO();
      _cogl_set_error (error, COGL_TEXTURE_ERROR,
                       COGL_TEXTURE_ERROR_BAD_PARAMETER,
                       "Unsupported shared bitmap load to texture");
      goto error;
    }
  else if (bitmap->buffer)
    {
      if (!load_bitmap_buffer_to_texture (tex_2d, bitmap, 0, 0, error))
        goto error;
    }
  else
    {
      if (!load_bitmap_data_to_texture (tex_2d,
                                        0, 0,
                                        bitmap->width, bitmap->height,
                                        bitmap, 0, 0, error))
        goto error;
    }

  if (!create_image_view (tex_2d, error))
    goto error;

  tex_2d->internal_format = bitmap->format;
  _cogl_texture_set_allocated (tex, bitmap->format, width, height);

  ret = TRUE;

 error:
  if (bitmap)
    cogl_object_unref (bitmap);

  return ret;
}

static CoglBool
allocate_from_foreign_vulkan (CoglTexture2D *tex_2d,
                              CoglTextureLoader *loader,
                              CoglError **error)
{
  CoglTexture2DVulkan *tex_2d_vk = tex_2d->winsys;
  CoglTexture *tex = COGL_TEXTURE (tex_2d);
  int width = loader->src.vulkan_foreign.width,
    height = loader->src.vulkan_foreign.height;

  tex_2d->is_foreign = TRUE;

  tex_2d_vk->format = loader->src.vulkan_foreign.format;
  tex_2d_vk->image = loader->src.vulkan_foreign.image;
  tex_2d_vk->component_mapping = loader->src.vulkan_foreign.component_mapping;
  tex_2d_vk->image_layout = loader->src.vulkan_foreign.image_layout;
  tex_2d_vk->access_mask = loader->src.vulkan_foreign.access_mask;
  tex_2d->internal_format =
    _cogl_vulkan_format_to_pixel_format (loader->src.vulkan_foreign.format);

  if (!create_image_view (tex_2d, error))
      return FALSE;

  _cogl_texture_set_allocated (tex, tex_2d->internal_format, width, height);

  return TRUE;
}

CoglBool
_cogl_texture_2d_vulkan_allocate (CoglTexture *tex,
                                  CoglError **error)
{
  CoglTexture2D *tex_2d = COGL_TEXTURE_2D (tex);
  CoglTextureLoader *loader = tex->loader;

  _COGL_RETURN_VAL_IF_FAIL (loader, FALSE);

  switch (loader->src_type)
    {
    case COGL_TEXTURE_SOURCE_TYPE_SIZED:
      return allocate_with_size (tex_2d, loader, error);
    case COGL_TEXTURE_SOURCE_TYPE_BITMAP:
      return allocate_from_bitmap (tex_2d, loader, error);
    case COGL_TEXTURE_SOURCE_TYPE_VULKAN_FOREIGN:
      return allocate_from_foreign_vulkan (tex_2d, loader, error);
    default:
      break;
    }

  g_return_val_if_reached (FALSE);
}

void
_cogl_texture_2d_vulkan_copy_from_framebuffer (CoglTexture2D *tex_2d,
                                               int src_x,
                                               int src_y,
                                               int width,
                                               int height,
                                               CoglFramebuffer *src_fb,
                                               int dst_x,
                                               int dst_y,
                                               int level)
{
  CoglTexture2DVulkan *tex_2d_vk = tex_2d->winsys;
  CoglTexture *tex = COGL_TEXTURE (tex_2d);
  CoglContext *ctx = tex->context;
  CoglContextVulkan *vk_ctx = ctx->winsys;
  CoglFramebufferVulkan *vk_fb = src_fb->winsys;
  VkImageCopy image_copy = {
    .srcSubresource = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .mipLevel = 0,
      .baseArrayLayer = 0,
      .layerCount = 1,
    },
    .srcOffset = { src_x, src_y, 0, },
    .dstSubresource = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .mipLevel = level,
      .baseArrayLayer = 0,
      .layerCount = 1,
    },
    .dstOffset = { dst_x, dst_y, 0, },
    .extent = { width, height, 1, },
  };
  VkCommandBuffer cmd_buffer = VK_NULL_HANDLE;
  CoglError *error = NULL;

  if (level != 0 && !tex_2d_vk->has_mipmap)
    _cogl_texture_2d_vulkan_generate_mipmap (tex_2d);

  cogl_framebuffer_finish (src_fb);

  if (!_cogl_vulkan_context_create_command_buffer (ctx, &cmd_buffer, &error))
    goto error;

  _cogl_texture_vulkan_move_to (tex,
                                COGL_TEXTURE_DOMAIN_TRANSFER_DESTINATION,
                                cmd_buffer);

  VK ( ctx,
       vkCmdCopyImage (cmd_buffer,
                       vk_fb->color_image,
                       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                       tex_2d_vk->image,
                       tex_2d_vk->image_layout,
                       1, &image_copy) );

  _cogl_texture_vulkan_move_to (tex, COGL_TEXTURE_DOMAIN_SAMPLING, cmd_buffer);

  if (!_cogl_vulkan_context_submit_command_buffer (ctx, cmd_buffer, &error))
    goto error;

 error:
  if (error)
    {
      g_warning ("Copy from framebuffer to texture failed : %s",
                 error->message);
      cogl_error_free (error);
    }

  if (cmd_buffer != VK_NULL_HANDLE)
    VK ( ctx,
         vkFreeCommandBuffers (vk_ctx->device, vk_ctx->cmd_pool,
                               1, &cmd_buffer) );
}

void
_cogl_texture_2d_vulkan_get_gl_info (CoglTexture2D *tex_2d,
                                     CoglTextureGLInfo *info)
{
  CoglTexture2DVulkan *tex_2d_vk = tex_2d->winsys;
  uint64_t value = (uint64_t) tex_2d_vk->image;
  uint32_t *pvalue = (uint32_t *) &value;

  info->format = 0;
  info->handle = pvalue[0] ^ pvalue[1];
}

void
_cogl_texture_2d_vulkan_generate_mipmap (CoglTexture2D *tex_2d)
{
  VK_TODO();
}

CoglBool
_cogl_texture_2d_vulkan_copy_from_bitmap (CoglTexture2D *tex_2d,
                                          int src_x,
                                          int src_y,
                                          int width,
                                          int height,
                                          CoglBitmap *bmp,
                                          int dst_x,
                                          int dst_y,
                                          int level,
                                          CoglError **error)
{
  CoglTexture2DVulkan *tex_2d_vk = tex_2d->winsys;
  CoglTexture *tex = COGL_TEXTURE (tex_2d);
  CoglBlitData blit_data;
  CoglTexture *src;
  CoglBool ret;

  if (bmp->shared_bmp)
    {
      /* We need to go deeper. */
      return _cogl_texture_2d_vulkan_copy_from_bitmap (tex_2d, src_x, src_y,
                                                       width, height,
                                                       bmp->shared_bmp,
                                                       dst_x, dst_y,
                                                       level, error);
    }
  else if (bmp->buffer)
    {
      if (src_x == 0 && src_y == 0 && bmp->width == width && bmp->height == height)
        return load_bitmap_buffer_to_texture (tex_2d, bmp, dst_x, dst_y, error);
      else if (tex_2d_vk->tiling == VK_IMAGE_TILING_LINEAR)
        return load_bitmap_data_to_texture (tex_2d, src_x, src_y, width, height,
                                            bmp, dst_x, dst_y, error);
    }
  else if (bmp->format == tex_2d->internal_format &&
           tex_2d_vk->tiling == VK_IMAGE_TILING_LINEAR)
    {
      return load_bitmap_data_to_texture (tex_2d, src_x, src_y, width, height,
                                          bmp, dst_x, dst_y, error);
    }

  /* Slow 2-pass upload with format conversion on the GPU. */
  src = COGL_TEXTURE (cogl_texture_2d_new_from_bitmap (bmp));
  ret = load_texture_to_texture (tex_2d, src_x, src_y,
                                 width, height,
                                 src, dst_x, dst_y, error);
  cogl_object_unref (src);

  return ret;
}

CoglBool
_cogl_texture_2d_vulkan_ready_for_sampling (CoglTexture2D *tex_2d)
{
  CoglTexture2DVulkan *tex_2d_vk = tex_2d->winsys;

  return (tex_2d_vk->access_mask & VK_ACCESS_SHADER_READ_BIT) != 0 &&
    tex_2d_vk->image_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

static void
_domain_to_vulkan_layout_and_access_mask (CoglTextureDomain domain,
                                          VkImageLayout *layout,
                                          VkAccessFlags *access_mask,
                                          VkPipelineStageFlagBits *stage)
{
  switch (domain)
    {
    case COGL_TEXTURE_DOMAIN_ATTACHMENT:
      *layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      *access_mask = (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
      *stage = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
      break;
    case COGL_TEXTURE_DOMAIN_HOST:
      *layout = VK_IMAGE_LAYOUT_GENERAL;
      *access_mask = VK_ACCESS_HOST_READ_BIT;
      *stage = VK_PIPELINE_STAGE_HOST_BIT;
      break;
    case COGL_TEXTURE_DOMAIN_SAMPLING:
      *layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      *access_mask = VK_ACCESS_SHADER_READ_BIT;
      *stage = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
      break;
    case COGL_TEXTURE_DOMAIN_TRANSFER_DESTINATION:
      *layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      *access_mask = VK_ACCESS_TRANSFER_WRITE_BIT;
      *stage = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
      break;
    case COGL_TEXTURE_DOMAIN_TRANSFER_SOURCE:
      *layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
      *access_mask = VK_ACCESS_TRANSFER_READ_BIT;
      *stage = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
      break;
    default:
      g_assert_not_reached();
    }
}

void
_cogl_texture_2d_vulkan_get_vulkan_info (CoglTexture2D *tex_2d,
                                         CoglTextureVulkanInfo *info)
{
  CoglTexture2DVulkan *tex_2d_vk = tex_2d->winsys;

  info->format = tex_2d_vk->format;
  info->image = tex_2d_vk->image;
  info->image_view = tex_2d_vk->image_view;
  info->image_layout = tex_2d_vk->image_layout;
  info->component_mapping = tex_2d_vk->component_mapping;
}

void
_cogl_texture_2d_vulkan_vulkan_move_to (CoglTexture2D *tex_2d,
                                        CoglTextureDomain domain,
                                        VkCommandBuffer cmd_buffer)
{
  CoglTexture2DVulkan *tex_2d_vk = tex_2d->winsys;
  CoglContext *ctx = tex_2d->_parent.context;
  VkImageLayout new_layout;
  VkAccessFlags new_access_mask;
  VkPipelineStageFlags dst_stage;
  VkImageMemoryBarrier image_barrier = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    .srcAccessMask = tex_2d_vk->access_mask,
    .dstAccessMask = 0,
    .oldLayout = tex_2d_vk->image_layout,
    .newLayout = 0,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .image = tex_2d_vk->image,
    .subresourceRange = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseMipLevel = 0,
      .levelCount = 1,
      .baseArrayLayer = 0,
      .layerCount = 1,
    },
  };

  _domain_to_vulkan_layout_and_access_mask (domain,
                                            &new_layout,
                                            &new_access_mask,
                                            &dst_stage);

  if (tex_2d_vk->image_layout == new_layout)
    return;

  image_barrier.newLayout = new_layout;
  image_barrier.dstAccessMask = new_access_mask;

  tex_2d_vk->image_layout = new_layout;
  tex_2d_vk->access_mask = new_access_mask;

  VK ( ctx, vkCmdPipelineBarrier (cmd_buffer,
                                  VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
                                  dst_stage,
                                  0,
                                  0, NULL,
                                  0, NULL,
                                  1, &image_barrier) );
}

CoglTexture2D *
_cogl_texture_2d_vulkan_new_for_foreign (CoglContext *ctx,
                                         int width,
                                         int height,
                                         VkImage image,
                                         VkFormat format,
                                         VkComponentMapping component_mapping,
                                         VkImageLayout image_layout,
                                         VkAccessFlags access_mask)
{
  CoglTextureLoader *loader;

  _COGL_RETURN_VAL_IF_FAIL (image != VK_NULL_HANDLE, NULL);
  _COGL_RETURN_VAL_IF_FAIL (width > 0 && height > 0, NULL);

  loader = _cogl_texture_create_loader ();
  loader->src_type = COGL_TEXTURE_SOURCE_TYPE_VULKAN_FOREIGN;
  loader->src.vulkan_foreign.width = width;
  loader->src.vulkan_foreign.height = height;
  loader->src.vulkan_foreign.image = image;
  loader->src.vulkan_foreign.format = format;
  loader->src.vulkan_foreign.component_mapping = component_mapping;
  loader->src.vulkan_foreign.image_layout = image_layout;
  loader->src.vulkan_foreign.access_mask = access_mask;

  return _cogl_texture_2d_create_base (ctx, width, height, format, loader);
}
