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
 *
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
#include "cogl-texture-rectangle-private.h"
#include "cogl-texture-driver.h"
#include "cogl-context-private.h"
#include "cogl-object-private.h"
#include "cogl-journal-private.h"
#include "cogl-pipeline-opengl-private.h"
#include "cogl-error-private.h"
#include "cogl-util-gl-private.h"
#include "cogl-gtype-private.h"

static void _cogl_texture_rectangle_free (CoglTextureRectangle *tex_rect);

COGL_TEXTURE_DEFINE (TextureRectangle, texture_rectangle);
COGL_GTYPE_DEFINE_CLASS (TextureRectangle, texture_rectangle,
                         COGL_GTYPE_IMPLEMENT_INTERFACE (texture));

static const CoglTextureVtable cogl_texture_rectangle_vtable;

static void
_cogl_texture_rectangle_free (CoglTextureRectangle *tex_rect)
{
  CoglContext *ctx = COGL_TEXTURE (tex_rect)->context;

  ctx->driver_vtable->texture_rectangle_free (tex_rect);

  /* Chain up */
  _cogl_texture_free (COGL_TEXTURE (tex_rect));
}

static void
_cogl_texture_rectangle_set_auto_mipmap (CoglTexture *tex,
                                         CoglBool value)
{
  /* Rectangle textures currently never support mipmapping so there's
     no point in doing anything here */
}

static CoglTextureRectangle *
_cogl_texture_rectangle_create_base (CoglContext *ctx,
                                     int width,
                                     int height,
                                     CoglPixelFormat internal_format,
                                     CoglTextureLoader *loader)
{
  CoglTextureRectangle *tex_rect = g_new (CoglTextureRectangle, 1);
  CoglTexture *tex = COGL_TEXTURE (tex_rect);

  _cogl_texture_init (tex, ctx, width, height,
                      internal_format, loader,
                      &cogl_texture_rectangle_vtable);

  tex_rect->is_foreign = FALSE;

  ctx->driver_vtable->texture_rectangle_init (tex_rect);

  return _cogl_texture_rectangle_object_new (tex_rect);
}

CoglTextureRectangle *
cogl_texture_rectangle_new_with_size (CoglContext *ctx,
                                      int width,
                                      int height)
{
  CoglTextureLoader *loader = _cogl_texture_create_loader ();
  loader->src_type = COGL_TEXTURE_SOURCE_TYPE_SIZED;
  loader->src.sized.width = width;
  loader->src.sized.height = height;

  return _cogl_texture_rectangle_create_base (ctx, width, height,
                                              COGL_PIXEL_FORMAT_RGBA_8888_PRE,
                                              loader);
}

static CoglBool
_cogl_texture_rectangle_allocate (CoglTexture *tex,
                                  CoglError **error)
{
  CoglContext *ctx = tex->context;

  return ctx->driver_vtable->texture_rectangle_allocate (tex, error);
}

CoglTextureRectangle *
cogl_texture_rectangle_new_from_bitmap (CoglBitmap *bmp)
{
  CoglTextureLoader *loader;

  _COGL_RETURN_VAL_IF_FAIL (cogl_is_bitmap (bmp), NULL);

  loader = _cogl_texture_create_loader ();
  loader->src_type = COGL_TEXTURE_SOURCE_TYPE_BITMAP;
  loader->src.bitmap.bitmap = cogl_object_ref (bmp);
  loader->src.bitmap.can_convert_in_place = FALSE; /* TODO add api for this */

  return _cogl_texture_rectangle_create_base (_cogl_bitmap_get_context (bmp),
                                              cogl_bitmap_get_width (bmp),
                                              cogl_bitmap_get_height (bmp),
                                              cogl_bitmap_get_format (bmp),
                                              loader);
}

CoglTextureRectangle *
cogl_texture_rectangle_new_from_foreign (CoglContext *ctx,
                                         unsigned int gl_handle,
                                         int width,
                                         int height,
                                         CoglPixelFormat format)
{
  CoglTextureLoader *loader;

  /* NOTE: width, height and internal format are not queriable in
   * GLES, hence such a function prototype. Also in the case of full
   * opengl the user may be creating a Cogl texture for a
   * texture_from_pixmap object where glTexImage2D may not have been
   * called and the texture_from_pixmap spec doesn't clarify that it
   * is reliable to query back the size from OpenGL.
   */

  /* Assert that it is a valid GL texture object */
  _COGL_RETURN_VAL_IF_FAIL (ctx->glIsTexture (gl_handle), NULL);

  /* Validate width and height */
  _COGL_RETURN_VAL_IF_FAIL (width > 0 && height > 0, NULL);

  loader = _cogl_texture_create_loader ();
  loader->src_type = COGL_TEXTURE_SOURCE_TYPE_GL_FOREIGN;
  loader->src.gl_foreign.gl_handle = gl_handle;
  loader->src.gl_foreign.width = width;
  loader->src.gl_foreign.height = height;
  loader->src.gl_foreign.format = format;

  return _cogl_texture_rectangle_create_base (ctx, width, height,
                                              format, loader);
}

static int
_cogl_texture_rectangle_get_max_waste (CoglTexture *tex)
{
  return -1;
}

static CoglBool
_cogl_texture_rectangle_is_sliced (CoglTexture *tex)
{
  return FALSE;
}

static CoglBool
_cogl_texture_rectangle_can_hardware_repeat (CoglTexture *tex)
{
  return FALSE;
}

static void
_cogl_texture_rectangle_transform_coords_to_gl (CoglTexture *tex,
                                                float *s,
                                                float *t)
{
  *s *= tex->width;
  *t *= tex->height;
}

static CoglTransformResult
_cogl_texture_rectangle_transform_quad_coords_to_gl (CoglTexture *tex,
                                                     float *coords)
{
  CoglBool need_repeat = FALSE;
  int i;

  for (i = 0; i < 4; i++)
    {
      if (coords[i] < 0.0f || coords[i] > 1.0f)
        need_repeat = TRUE;
      coords[i] *= (i & 1) ? tex->height : tex->width;
    }

  return (need_repeat ? COGL_TRANSFORM_SOFTWARE_REPEAT
          : COGL_TRANSFORM_NO_REPEAT);
}

static CoglBool
_cogl_texture_rectangle_get_gl_texture (CoglTexture *tex,
                                        GLuint *out_gl_handle,
                                        GLenum *out_gl_target)
{
  CoglContext *ctx = tex->context;
  CoglTextureRectangle *tex_rect = COGL_TEXTURE_RECTANGLE (tex);

  if (ctx->driver_vtable->texture_rectangle_get_gl_handle)
    {
      GLuint handle;

      if (out_gl_target)
        *out_gl_target = GL_TEXTURE_RECTANGLE_ARB;

      handle = ctx->driver_vtable->texture_rectangle_get_gl_handle (tex_rect);

      if (out_gl_handle)
        *out_gl_handle = handle;

      return handle ? TRUE : FALSE;
    }
  else
    return FALSE;
}

static void
_cogl_texture_rectangle_flush_legacy_texobj_filters (CoglTexture *tex,
                                                     GLenum min_filter,
                                                     GLenum mag_filter)
{
  CoglContext *ctx = tex->context;

  if (!ctx->driver_vtable->texture_rectangle_flush_legacy_filters)
    return;

  ctx->driver_vtable->texture_rectangle_flush_legacy_filters (COGL_TEXTURE_RECTANGLE (tex),
                                                              min_filter,
                                                              mag_filter);
}

static void
_cogl_texture_rectangle_flush_legacy_texobj_wrap_modes (CoglTexture *tex,
                                                        GLenum wrap_mode_s,
                                                        GLenum wrap_mode_t,
                                                        GLenum wrap_mode_p)
{
  CoglContext *ctx = tex->context;

  if (!ctx->driver_vtable->texture_rectangle_flush_legacy_wrap_modes)
    return;

  ctx->driver_vtable->texture_rectangle_flush_legacy_wrap_modes (COGL_TEXTURE_RECTANGLE (tex),
                                                                 wrap_mode_s,
                                                                 wrap_mode_t,
                                                                 wrap_mode_p);
}

static void
_cogl_texture_rectangle_pre_paint (CoglTexture *tex,
                                   CoglTexturePrePaintFlags flags)
{
  /* Rectangle textures don't support mipmaps */
  g_assert ((flags & COGL_TEXTURE_NEEDS_MIPMAP) == 0);
}

static void
_cogl_texture_rectangle_ensure_non_quad_rendering (CoglTexture *tex)
{
  /* Nothing needs to be done */
}

static CoglBool
_cogl_texture_rectangle_set_region (CoglTexture *tex,
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
  CoglBitmap *upload_bmp;
  GLenum gl_format;
  GLenum gl_type;
  CoglContext *ctx = tex->context;
  CoglBool status;

  upload_bmp =
    _cogl_bitmap_convert_for_upload (bmp,
                                     _cogl_texture_get_format (tex),
                                     FALSE, /* can't convert in place */
                                     error);
  if (upload_bmp == NULL)
    return FALSE;

  ctx->driver_vtable->pixel_format_to_gl (ctx,
                                          cogl_bitmap_get_format (upload_bmp),
                                          NULL, /* internal format */
                                          &gl_format,
                                          &gl_type);

  /* Send data to GL */
  status =
    ctx->texture_driver->upload_subregion_to_gl (ctx,
                                                 tex,
                                                 FALSE,
                                                 src_x, src_y,
                                                 dst_x, dst_y,
                                                 dst_width, dst_height,
                                                 level,
                                                 upload_bmp,
                                                 gl_format,
                                                 gl_type,
                                                 error);

  cogl_object_unref (upload_bmp);

  return status;
}

static CoglBool
_cogl_texture_rectangle_get_data (CoglTexture *tex,
                                  CoglPixelFormat format,
                                  int rowstride,
                                  uint8_t *data)
{
  CoglContext *ctx = tex->context;

  if (ctx->driver_vtable->texture_2d_get_data)
    {
      CoglTextureRectangle *tex_rect = COGL_TEXTURE_RECTANGLE (tex);
      ctx->driver_vtable->texture_rectangle_get_data (tex_rect, format,
                                                      rowstride, data);
      return TRUE;
    }
  else
    return FALSE;
}

static CoglPixelFormat
_cogl_texture_rectangle_get_format (CoglTexture *tex)
{
  return COGL_TEXTURE_RECTANGLE (tex)->internal_format;
}

static GLenum
_cogl_texture_rectangle_get_gl_format (CoglTexture *tex)
{
  CoglContext *ctx = tex->context;

  if (!ctx->driver_vtable->texture_rectangle_get_gl_format)
    return 0;

  return ctx->driver_vtable->texture_rectangle_get_gl_format (COGL_TEXTURE_RECTANGLE (tex));
}

static CoglBool
_cogl_texture_rectangle_is_foreign (CoglTexture *tex)
{
  return COGL_TEXTURE_RECTANGLE (tex)->is_foreign;
}

static CoglTextureType
_cogl_texture_rectangle_get_type (CoglTexture *tex)
{
  return COGL_TEXTURE_TYPE_RECTANGLE;
}

static const CoglTextureVtable
cogl_texture_rectangle_vtable =
  {
    TRUE, /* primitive */
    _cogl_texture_rectangle_allocate,
    _cogl_texture_rectangle_set_region,
    _cogl_texture_rectangle_get_data,
    NULL, /* foreach_sub_texture_in_region */
    _cogl_texture_rectangle_get_max_waste,
    _cogl_texture_rectangle_is_sliced,
    _cogl_texture_rectangle_can_hardware_repeat,
    _cogl_texture_rectangle_transform_coords_to_gl,
    _cogl_texture_rectangle_transform_quad_coords_to_gl,
    _cogl_texture_rectangle_get_gl_texture,
    _cogl_texture_rectangle_flush_legacy_texobj_filters,
    _cogl_texture_rectangle_pre_paint,
    _cogl_texture_rectangle_ensure_non_quad_rendering,
    _cogl_texture_rectangle_flush_legacy_texobj_wrap_modes,
    _cogl_texture_rectangle_get_format,
    _cogl_texture_rectangle_get_gl_format,
    _cogl_texture_rectangle_get_type,
    _cogl_texture_rectangle_is_foreign,
    _cogl_texture_rectangle_set_auto_mipmap
  };
