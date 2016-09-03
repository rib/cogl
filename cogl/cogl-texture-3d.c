/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2010 Intel Corporation.
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
 * Authors:
 *  Neil Roberts   <neil@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-private.h"
#include "cogl-util.h"
#include "cogl-texture-private.h"
#include "cogl-texture-3d-private.h"
#include "cogl-texture-3d.h"
#include "cogl-texture-gl-private.h"
#include "cogl-texture-driver.h"
#include "cogl-context-private.h"
#include "cogl-object-private.h"
#include "cogl-journal-private.h"
#include "cogl-pipeline-private.h"
#include "cogl-pipeline-opengl-private.h"
#include "cogl-error-private.h"
#include "cogl-util-gl-private.h"
#include "cogl-gtype-private.h"

#include <string.h>
#include <math.h>

static void _cogl_texture_3d_free (CoglTexture3D *tex_3d);

COGL_TEXTURE_DEFINE (Texture3D, texture_3d);
COGL_GTYPE_DEFINE_CLASS (Texture3D, texture_3d,
                         COGL_GTYPE_IMPLEMENT_INTERFACE (texture));

static const CoglTextureVtable cogl_texture_3d_vtable;

static void
_cogl_texture_3d_free (CoglTexture3D *tex_3d)
{
  CoglContext *ctx = COGL_TEXTURE (tex_3d)->context;

  ctx->driver_vtable->texture_3d_free (tex_3d);

  /* Chain up */
  _cogl_texture_free (COGL_TEXTURE (tex_3d));
}

static void
_cogl_texture_3d_set_auto_mipmap (CoglTexture *tex,
                                  CoglBool value)
{
  CoglTexture3D *tex_3d = COGL_TEXTURE_3D (tex);

  tex_3d->auto_mipmap = value;
}

static CoglTexture3D *
_cogl_texture_3d_create_base (CoglContext *ctx,
                              int width,
                              int height,
                              int depth,
                              CoglPixelFormat internal_format,
                              CoglTextureLoader *loader)
{
  CoglTexture3D *tex_3d = g_new (CoglTexture3D, 1);
  CoglTexture *tex = COGL_TEXTURE (tex_3d);

  _cogl_texture_init (tex, ctx, width, height,
                      internal_format, loader, &cogl_texture_3d_vtable);

  tex_3d->depth = depth;
  tex_3d->mipmaps_dirty = TRUE;
  tex_3d->auto_mipmap = TRUE;

  ctx->driver_vtable->texture_3d_init (tex_3d);

  return _cogl_texture_3d_object_new (tex_3d);
}

CoglTexture3D *
cogl_texture_3d_new_with_size (CoglContext *ctx,
                               int width,
                               int height,
                               int depth)
{
  CoglTextureLoader *loader = _cogl_texture_create_loader ();
  loader->src_type = COGL_TEXTURE_SOURCE_TYPE_SIZED;
  loader->src.sized.width = width;
  loader->src.sized.height = height;
  loader->src.sized.depth = depth;

  return _cogl_texture_3d_create_base (ctx, width, height, depth,
                                       COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                                       loader);
}

CoglTexture3D *
cogl_texture_3d_new_from_bitmap (CoglBitmap *bmp,
                                 int height,
                                 int depth)
{
  CoglTextureLoader *loader;

  _COGL_RETURN_VAL_IF_FAIL (bmp, NULL);

  loader = _cogl_texture_create_loader ();
  loader->src_type = COGL_TEXTURE_SOURCE_TYPE_BITMAP;
  loader->src.bitmap.bitmap = cogl_object_ref (bmp);
  loader->src.bitmap.height = height;
  loader->src.bitmap.depth = depth;
  loader->src.bitmap.can_convert_in_place = FALSE; /* TODO add api for this */

  return _cogl_texture_3d_create_base (_cogl_bitmap_get_context (bmp),
                                       cogl_bitmap_get_width (bmp),
                                       height,
                                       depth,
                                       cogl_bitmap_get_format (bmp),
                                       loader);
}

CoglTexture3D *
cogl_texture_3d_new_from_data (CoglContext *context,
                               int width,
                               int height,
                               int depth,
                               CoglPixelFormat format,
                               int rowstride,
                               int image_stride,
                               const uint8_t *data,
                               CoglError **error)
{
  CoglBitmap *bitmap;
  CoglTexture3D *ret;

  _COGL_RETURN_VAL_IF_FAIL (data, NULL);
  _COGL_RETURN_VAL_IF_FAIL (format != COGL_PIXEL_FORMAT_ANY, NULL);

  /* Rowstride from width if not given */
  if (rowstride == 0)
    rowstride = width * _cogl_pixel_format_get_bytes_per_pixel (format);
  /* Image stride from height and rowstride if not given */
  if (image_stride == 0)
    image_stride = height * rowstride;

  if (image_stride < rowstride * height)
    return NULL;

  /* GL doesn't support uploading when the image_stride isn't a
     multiple of the rowstride. If this happens we'll just pack the
     image into a new bitmap. The documentation for this function
     recommends avoiding this situation. */
  if (image_stride % rowstride != 0)
    {
      uint8_t *bmp_data;
      int bmp_rowstride;
      int z, y;

      bitmap = _cogl_bitmap_new_with_malloc_buffer (context,
                                                    width,
                                                    depth * height,
                                                    format,
                                                    error);
      if (!bitmap)
        return NULL;

      bmp_data = _cogl_bitmap_map (bitmap,
                                   COGL_BUFFER_ACCESS_WRITE,
                                   COGL_BUFFER_MAP_HINT_DISCARD,
                                   error);

      if (bmp_data == NULL)
        {
          cogl_object_unref (bitmap);
          return NULL;
        }

      bmp_rowstride = cogl_bitmap_get_rowstride (bitmap);

      /* Copy all of the images in */
      for (z = 0; z < depth; z++)
        for (y = 0; y < height; y++)
          memcpy (bmp_data + (z * bmp_rowstride * height +
                              bmp_rowstride * y),
                  data + z * image_stride + rowstride * y,
                  bmp_rowstride);

      _cogl_bitmap_unmap (bitmap);
    }
  else
    bitmap = cogl_bitmap_new_for_data (context,
                                       width,
                                       image_stride / rowstride * depth,
                                       format,
                                       rowstride,
                                       (uint8_t *) data);

  ret = cogl_texture_3d_new_from_bitmap (bitmap,
                                         height,
                                         depth);

  cogl_object_unref (bitmap);

  if (ret &&
      !cogl_texture_allocate (COGL_TEXTURE (ret), error))
    {
      cogl_object_unref (ret);
      return NULL;
    }

  return ret;
}

#ifdef COGL_HAS_GTYPE_SUPPORT

CoglTexture3D *
cogl_texture_3d_new_from_bytes (CoglContext *context,
                                int width,
                                int height,
                                int depth,
                                CoglPixelFormat format,
                                int rowstride,
                                int image_stride,
                                GBytes *bytes,
                                CoglError **error)
{
  _COGL_RETURN_VAL_IF_FAIL (bytes != NULL, NULL);

  return cogl_texture_3d_new_from_data (context,
                                        width,
                                        height,
                                        depth,
                                        format,
                                        rowstride,
                                        image_stride,
                                        g_bytes_get_data (bytes, NULL),
                                        error);
}

#endif

static CoglBool
_cogl_texture_3d_allocate (CoglTexture *tex,
                           CoglError **error)
{
  CoglContext *ctx = tex->context;

  return ctx->driver_vtable->texture_3d_allocate (tex, error);
}

static int
_cogl_texture_3d_get_max_waste (CoglTexture *tex)
{
  return -1;
}

static CoglBool
_cogl_texture_3d_is_sliced (CoglTexture *tex)
{
  return FALSE;
}

static CoglBool
_cogl_texture_3d_can_hardware_repeat (CoglTexture *tex)
{
  return TRUE;
}

static void
_cogl_texture_3d_transform_coords_to_gl (CoglTexture *tex,
                                         float *s,
                                         float *t)
{
  /* The texture coordinates map directly so we don't need to do
     anything */
}

static CoglTransformResult
_cogl_texture_3d_transform_quad_coords_to_gl (CoglTexture *tex,
                                              float *coords)
{
  /* The texture coordinates map directly so we don't need to do
     anything other than check for repeats */

  CoglBool need_repeat = FALSE;
  int i;

  for (i = 0; i < 4; i++)
    if (coords[i] < 0.0f || coords[i] > 1.0f)
      need_repeat = TRUE;

  return (need_repeat ? COGL_TRANSFORM_HARDWARE_REPEAT
          : COGL_TRANSFORM_NO_REPEAT);
}

static CoglBool
_cogl_texture_3d_get_gl_texture (CoglTexture *tex,
                                 GLuint *out_gl_handle,
                                 GLenum *out_gl_target)
{
  CoglContext *ctx = tex->context;
  CoglTexture3D *tex_3d = COGL_TEXTURE_3D (tex);

  if (ctx->driver_vtable->texture_3d_get_gl_handle)
    {
      GLuint handle;

      if (out_gl_target)
        *out_gl_target = GL_TEXTURE_3D;

      handle = ctx->driver_vtable->texture_3d_get_gl_handle (tex_3d);

      if (out_gl_handle)
        *out_gl_handle = handle;

      return handle ? TRUE : FALSE;
    }
  else
    return FALSE;
}

static void
_cogl_texture_3d_flush_legacy_texobj_filters (CoglTexture *tex,
                                              GLenum min_filter,
                                              GLenum mag_filter)
{
  CoglContext *ctx = tex->context;

  if (!ctx->driver_vtable->texture_2d_flush_legacy_filters)
    return;

  ctx->driver_vtable->texture_3d_flush_legacy_filters (COGL_TEXTURE_3D (tex),
                                                       min_filter,
                                                       mag_filter);
}

static void
_cogl_texture_3d_flush_legacy_texobj_wrap_modes (CoglTexture *tex,
                                                 GLenum wrap_mode_s,
                                                 GLenum wrap_mode_t,
                                                 GLenum wrap_mode_p)
{
  CoglContext *ctx = tex->context;

  if (!ctx->driver_vtable->texture_3d_flush_legacy_wrap_modes)
    return;

  ctx->driver_vtable->texture_3d_flush_legacy_wrap_modes (COGL_TEXTURE_3D (tex),
                                                          wrap_mode_s,
                                                          wrap_mode_t,
                                                          wrap_mode_p);
}

static void
_cogl_texture_3d_pre_paint (CoglTexture *tex, CoglTexturePrePaintFlags flags)
{
  CoglTexture3D *tex_3d = COGL_TEXTURE_3D (tex);

  /* Only update if the mipmaps are dirty */
  if ((flags & COGL_TEXTURE_NEEDS_MIPMAP) &&
      tex_3d->auto_mipmap && tex_3d->mipmaps_dirty)
    {
      CoglContext *ctx = tex->context;

      ctx->driver_vtable->texture_3d_generate_mipmap (tex_3d);

      tex_3d->mipmaps_dirty = FALSE;
    }
}

static void
_cogl_texture_3d_ensure_non_quad_rendering (CoglTexture *tex)
{
  /* Nothing needs to be done */
}

static CoglBool
_cogl_texture_3d_set_region (CoglTexture *tex,
                             int src_x,
                             int src_y,
                             int dst_x,
                             int dst_y,
                             int dst_width,
                             int dst_height,
                             int level,
                             CoglBitmap *bmp,
                             CoglError **error)
{
  /* This function doesn't really make sense for 3D textures because
     it can't specify which image to upload to */
  _cogl_set_error (error,
                   COGL_SYSTEM_ERROR,
                   COGL_SYSTEM_ERROR_UNSUPPORTED,
                   "Setting a 2D region on a 3D texture isn't "
                   "currently supported");

  return FALSE;
}

static int
_cogl_texture_3d_get_data (CoglTexture *tex,
                           CoglPixelFormat format,
                           int rowstride,
                           uint8_t *data)
{
  /* FIXME: we could probably implement this by assuming the data is
     big enough to hold all of the images and that there is no stride
     between the images. However it would be better to have an API
     that can provide an image stride and this function probably isn't
     particularly useful anyway so for now it just reports failure */
  return 0;
}

static CoglPixelFormat
_cogl_texture_3d_get_format (CoglTexture *tex)
{
  return COGL_TEXTURE_3D (tex)->internal_format;
}

static GLenum
_cogl_texture_3d_get_gl_format (CoglTexture *tex)
{
  CoglContext *ctx = tex->context;

  if (!ctx->driver_vtable->texture_3d_get_gl_format)
    return 0;

  return ctx->driver_vtable->texture_3d_get_gl_format (COGL_TEXTURE_3D (tex));
}

static CoglTextureType
_cogl_texture_3d_get_type (CoglTexture *tex)
{
  return COGL_TEXTURE_TYPE_3D;
}

static const CoglTextureVtable
cogl_texture_3d_vtable =
  {
    TRUE, /* primitive */
    _cogl_texture_3d_allocate,
    _cogl_texture_3d_set_region,
    _cogl_texture_3d_get_data,
    NULL, /* foreach_sub_texture_in_region */
    _cogl_texture_3d_get_max_waste,
    _cogl_texture_3d_is_sliced,
    _cogl_texture_3d_can_hardware_repeat,
    _cogl_texture_3d_transform_coords_to_gl,
    _cogl_texture_3d_transform_quad_coords_to_gl,
    _cogl_texture_3d_get_gl_texture,
    _cogl_texture_3d_flush_legacy_texobj_filters,
    _cogl_texture_3d_pre_paint,
    _cogl_texture_3d_ensure_non_quad_rendering,
    _cogl_texture_3d_flush_legacy_texobj_wrap_modes,
    _cogl_texture_3d_get_format,
    _cogl_texture_3d_get_gl_format,
    _cogl_texture_3d_get_type,
    NULL, /* is_foreign */
    _cogl_texture_3d_set_auto_mipmap
  };
