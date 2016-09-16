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

#include "cogl-context-private.h"
#include "cogl-driver-vulkan-private.h"
#include "cogl-renderer-private.h"
#include "cogl-texture-2d-vulkan-private.h"
#include "cogl-texture-driver.h"
#include "cogl-util-vulkan-private.h"

static GLuint
_cogl_texture_driver_gen (CoglContext *ctx,
                          GLenum gl_target,
                          CoglPixelFormat internal_format)
{
  return 0;
}

static void
_cogl_texture_driver_prep_gl_for_pixels_upload (CoglContext *ctx,
                                                int pixels_rowstride,
                                                int pixels_bpp)
{
}

static void
_cogl_texture_driver_prep_gl_for_pixels_download (CoglContext *ctx,
                                                  int image_width,
                                                  int pixels_rowstride,
                                                  int pixels_bpp)
{
}

static CoglBool
_cogl_texture_driver_upload_subregion_to_gl (CoglContext *ctx,
                                             CoglTexture *texture,
                                             CoglBool is_foreign,
                                             int src_x,
                                             int src_y,
                                             int dst_x,
                                             int dst_y,
                                             int width,
                                             int height,
                                             int level,
                                             CoglBitmap  *source_bmp,
				             GLuint source_gl_format,
				             GLuint source_gl_type,
                                             CoglError **error)
{
  VK_TODO();
  return FALSE;
}

static CoglBool
_cogl_texture_driver_upload_to_gl (CoglContext *ctx,
                                   GLenum gl_target,
                                   GLuint gl_handle,
                                   CoglBool is_foreign,
                                   CoglBitmap *source_bmp,
                                   GLint internal_gl_format,
                                   GLuint source_gl_format,
                                   GLuint source_gl_type,
                                   CoglError **error)
{
  VK_TODO();
  return FALSE;
}

static CoglBool
_cogl_texture_driver_upload_to_gl_3d (CoglContext *ctx,
                                      GLenum gl_target,
                                      GLuint gl_handle,
                                      CoglBool is_foreign,
                                      GLint height,
                                      GLint depth,
                                      CoglBitmap *source_bmp,
                                      GLint internal_gl_format,
                                      GLuint source_gl_format,
                                      GLuint source_gl_type,
                                      CoglError **error)
{
  VK_TODO();
  return FALSE;
}

static CoglBool
_cogl_texture_driver_gl_get_tex_image (CoglContext *ctx,
                                       GLenum gl_target,
                                       GLenum dest_gl_format,
                                       GLenum dest_gl_type,
                                       uint8_t *dest)
{
  return FALSE;
}

static CoglBool
_cogl_texture_driver_size_supported_3d (CoglContext *ctx,
                                        GLenum gl_target,
                                        GLenum gl_format,
                                        GLenum gl_type,
                                        int width,
                                        int height,
                                        int depth)
{
  CoglRenderer *renderer = ctx->display->renderer;
  CoglRendererVulkan *vk_renderer = renderer->winsys;
  VkPhysicalDeviceLimits *limits =
    &vk_renderer->physical_device_properties.limits;
  CoglPixelFormat format = gl_format;

  if (width > limits->maxImageDimension3D ||
      height > limits->maxImageDimension3D ||
      depth > limits->maxImageDimension3D)
    return FALSE;

  return TRUE;
}

static CoglBool
_cogl_texture_driver_size_supported (CoglContext *ctx,
                                     GLenum gl_target,
                                     GLenum gl_intformat,
                                     GLenum gl_format,
                                     GLenum gl_type,
                                     int width,
                                     int height)
{
  CoglRenderer *renderer = ctx->display->renderer;
  CoglRendererVulkan *vk_renderer = renderer->winsys;
  VkPhysicalDeviceLimits *limits =
    &vk_renderer->physical_device_properties.limits;
  CoglPixelFormat format = gl_format;
  VkFormat vk_format =
    _cogl_pixel_format_to_vulkan_format_for_sampling (ctx, format, NULL, NULL);

  if (vk_format == VK_FORMAT_UNDEFINED)
    return FALSE;

  if (width > limits->maxImageDimension2D ||
      height > limits->maxImageDimension2D)
    return FALSE;

  return TRUE;
}

static void
_cogl_texture_driver_try_setting_gl_border_color
                                       (CoglContext *ctx,
                                        GLuint gl_target,
                                        const GLfloat *transparent_color)
{
}

static CoglBool
_cogl_texture_driver_allows_foreign_gl_target (CoglContext *ctx,
                                               GLenum gl_target)
{
  return FALSE;
}

static CoglPixelFormat
_cogl_texture_driver_find_best_gl_get_data_format
                                            (CoglContext *context,
                                             CoglPixelFormat format,
                                             GLenum *closest_gl_format,
                                             GLenum *closest_gl_type)
{
  VkFormat vk_format =
    _cogl_pixel_format_to_vulkan_format_for_sampling (context, format,
                                                      NULL, NULL);

  if (vk_format == VK_FORMAT_UNDEFINED)
    return COGL_PIXEL_FORMAT_RGBA_8888;

  return format;
}

static void
_cogl_texture_driver_pre_paint (CoglContext *ctx,
                                CoglTexture *texture)
{
  if (cogl_is_texture_2d (texture) &&
      !_cogl_texture_2d_vulkan_ready_for_sampling (COGL_TEXTURE_2D (texture)))
    {
      CoglContextVulkan *vk_ctx = ctx->winsys;
      CoglError *error = NULL;
      VkCommandBuffer cmd_buffer = VK_NULL_HANDLE;

      if (!_cogl_vulkan_context_create_command_buffer (ctx, &cmd_buffer,
                                                       &error))
        goto error;

      _cogl_texture_vulkan_move_to (texture,
                                    COGL_TEXTURE_DOMAIN_SAMPLING,
                                    cmd_buffer);

      _cogl_vulkan_context_submit_command_buffer (ctx, cmd_buffer, &error);

    error:
      if (error)
        {
          g_warning ("Failed to prepare texture for sampling : %s",
                     error->message);
          cogl_error_free (error);
        }
      if (cmd_buffer)
        VK ( ctx, vkFreeCommandBuffers (vk_ctx->device, vk_ctx->cmd_pool,
                                        1, &cmd_buffer) );
    }
}

const CoglTextureDriver
_cogl_texture_driver_vulkan =
  {
    _cogl_texture_driver_gen,
    _cogl_texture_driver_prep_gl_for_pixels_upload,
    _cogl_texture_driver_upload_subregion_to_gl,
    _cogl_texture_driver_upload_to_gl,
    _cogl_texture_driver_upload_to_gl_3d,
    _cogl_texture_driver_prep_gl_for_pixels_download,
    _cogl_texture_driver_gl_get_tex_image,
    _cogl_texture_driver_size_supported,
    _cogl_texture_driver_size_supported_3d,
    _cogl_texture_driver_try_setting_gl_border_color,
    _cogl_texture_driver_allows_foreign_gl_target,
    _cogl_texture_driver_find_best_gl_get_data_format,
    _cogl_texture_driver_pre_paint
  };
