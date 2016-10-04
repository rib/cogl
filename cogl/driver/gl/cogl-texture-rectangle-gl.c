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
 *
 * Authors:
 *  Lionel Landwerlin <lionel.g.landwerlin@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-private.h"
#include "cogl-util.h"
#include "cogl-texture-private.h"
#include "cogl-texture-rectangle-private.h"
#include "cogl-texture-rectangle-gl-private.h"
#include "cogl-texture-gl-private.h"
#include "cogl-texture-driver.h"
#include "cogl-context-private.h"
#include "cogl-object-private.h"
#include "cogl-pipeline-opengl-private.h"
#include "cogl-error-private.h"
#include "cogl-util-gl-private.h"

/* #include <string.h> */
/* #include <math.h> */

/* These aren't defined under GLES */
#ifndef GL_TEXTURE_RECTANGLE_ARB
#define GL_TEXTURE_RECTANGLE_ARB 0x84F5
#endif
#ifndef GL_CLAMP
#define GL_CLAMP                 0x2900
#endif
#ifndef GL_CLAMP_TO_BORDER
#define GL_CLAMP_TO_BORDER       0x812D
#endif

typedef struct _CoglTextureRectangleGL CoglTextureRectangleGL;

struct _CoglTextureRectangleGL
{
  /* The internal format of the GL texture represented as a GL enum */
  GLenum gl_format;
  /* The texture object number */
  GLuint gl_texture;
  GLenum gl_legacy_texobj_min_filter;
  GLenum gl_legacy_texobj_mag_filter;
  GLint gl_legacy_texobj_wrap_mode_s;
  GLint gl_legacy_texobj_wrap_mode_t;
};

void
_cogl_texture_rectangle_gl_free (CoglTextureRectangle *tex_rect)
{
  CoglTextureRectangleGL *tex_rect_gl = tex_rect->winsys;

  if (!tex_rect->is_foreign && tex_rect_gl->gl_texture)
    _cogl_delete_gl_texture (tex_rect_gl->gl_texture);

  g_slice_free (CoglTextureRectangleGL, tex_rect_gl);
}

void
_cogl_texture_rectangle_gl_init (CoglTextureRectangle *tex_rect)
{
  CoglTextureRectangleGL *tex_rect_gl = g_slice_new0 (CoglTextureRectangleGL);

  tex_rect_gl->gl_texture = 0;

  /* We default to GL_LINEAR for both filters */
  tex_rect_gl->gl_legacy_texobj_min_filter = GL_LINEAR;
  tex_rect_gl->gl_legacy_texobj_mag_filter = GL_LINEAR;

  /* Wrap mode not yet set */
  tex_rect_gl->gl_legacy_texobj_wrap_mode_s = GL_FALSE;
  tex_rect_gl->gl_legacy_texobj_wrap_mode_t = GL_FALSE;

  tex_rect->winsys = tex_rect_gl;
}

unsigned int
_cogl_texture_rectangle_gl_get_gl_handle (CoglTextureRectangle *tex_rect)
{
  CoglTextureRectangleGL *tex_rect_gl = tex_rect->winsys;

  return tex_rect_gl->gl_texture;
}

GLenum
_cogl_texture_rectangle_gl_get_gl_format (CoglTextureRectangle *tex_rect)
{
  CoglTextureRectangleGL *tex_rect_gl = tex_rect->winsys;

  return tex_rect_gl->gl_format;
}

static CoglBool
_cogl_texture_rectangle_can_create (CoglContext *ctx,
                                    unsigned int width,
                                    unsigned int height,
                                    CoglPixelFormat internal_format,
                                    CoglError **error)
{
  GLenum gl_intformat;
  GLenum gl_format;
  GLenum gl_type;

  if (!cogl_has_feature (ctx, COGL_FEATURE_ID_TEXTURE_RECTANGLE))
    {
      _cogl_set_error (error,
                       COGL_TEXTURE_ERROR,
                       COGL_TEXTURE_ERROR_TYPE,
                       "The CoglTextureRectangle feature isn't available");
      return FALSE;
    }

  ctx->driver_vtable->pixel_format_to_gl (ctx,
                                          internal_format,
                                          &gl_intformat,
                                          &gl_format,
                                          &gl_type);

  /* Check that the driver can create a texture with that size */
  if (!ctx->texture_driver->size_supported (ctx,
                                            GL_TEXTURE_RECTANGLE_ARB,
                                            gl_intformat,
                                            gl_format,
                                            gl_type,
                                            width,
                                            height))
    {
      _cogl_set_error (error,
                       COGL_TEXTURE_ERROR,
                       COGL_TEXTURE_ERROR_SIZE,
                       "The requested texture size + format is unsupported");
      return FALSE;
    }

  return TRUE;
}

static CoglBool
allocate_with_size (CoglTextureRectangle *tex_rect,
                    CoglTextureLoader *loader,
                    CoglError **error)
{
  CoglTextureRectangleGL *tex_rect_gl = tex_rect->winsys;
  CoglTexture *tex = COGL_TEXTURE (tex_rect);
  CoglContext *ctx = tex->context;
  CoglPixelFormat internal_format;
  int width = loader->src.sized.width;
  int height = loader->src.sized.height;
  GLenum gl_intformat;
  GLenum gl_format;
  GLenum gl_type;
  GLenum gl_error;
  GLenum gl_texture;

  internal_format =
    _cogl_texture_determine_internal_format (tex, COGL_PIXEL_FORMAT_ANY);

  if (!_cogl_texture_rectangle_can_create (ctx,
                                           width,
                                           height,
                                           internal_format,
                                           error))
    return FALSE;

  ctx->driver_vtable->pixel_format_to_gl (ctx,
                                          internal_format,
                                          &gl_intformat,
                                          &gl_format,
                                          &gl_type);

  gl_texture =
    ctx->texture_driver->gen (ctx,
                              GL_TEXTURE_RECTANGLE_ARB,
                              internal_format);
  _cogl_bind_gl_texture_transient (GL_TEXTURE_RECTANGLE_ARB,
                                   gl_texture,
                                   tex_rect->is_foreign);

  /* Clear any GL errors */
  while ((gl_error = ctx->glGetError ()) != GL_NO_ERROR)
    ;

  ctx->glTexImage2D (GL_TEXTURE_RECTANGLE_ARB, 0, gl_intformat,
                     width, height, 0, gl_format, gl_type, NULL);

  if (_cogl_gl_util_catch_out_of_memory (ctx, error))
    {
      GE( ctx, glDeleteTextures (1, &gl_texture) );
      return FALSE;
    }

  tex_rect->internal_format = internal_format;

  tex_rect_gl->gl_texture = gl_texture;
  tex_rect_gl->gl_format = gl_intformat;

  _cogl_texture_set_allocated (COGL_TEXTURE (tex_rect),
                               internal_format, width, height);

  return TRUE;
}

static CoglBool
allocate_from_bitmap (CoglTextureRectangle *tex_rect,
                      CoglTextureLoader *loader,
                      CoglError **error)
{
  CoglTextureRectangleGL *tex_rect_gl = tex_rect->winsys;
  CoglTexture *tex = COGL_TEXTURE (tex_rect);
  CoglContext *ctx = tex->context;
  CoglPixelFormat internal_format;
  CoglBitmap *bmp = loader->src.bitmap.bitmap;
  int width = cogl_bitmap_get_width (bmp);
  int height = cogl_bitmap_get_height (bmp);
  CoglBool can_convert_in_place = loader->src.bitmap.can_convert_in_place;
  CoglBitmap *upload_bmp;
  GLenum gl_intformat;
  GLenum gl_format;
  GLenum gl_type;

  internal_format =
    _cogl_texture_determine_internal_format (tex, cogl_bitmap_get_format (bmp));

  if (!_cogl_texture_rectangle_can_create (ctx,
                                           width,
                                           height,
                                           internal_format,
                                           error))
    return FALSE;

  upload_bmp = _cogl_bitmap_convert_for_upload (bmp,
                                                internal_format,
                                                can_convert_in_place,
                                                error);
  if (upload_bmp == NULL)
    return FALSE;

  ctx->driver_vtable->pixel_format_to_gl (ctx,
                                          cogl_bitmap_get_format (upload_bmp),
                                          NULL, /* internal format */
                                          &gl_format,
                                          &gl_type);
  ctx->driver_vtable->pixel_format_to_gl (ctx,
                                          internal_format,
                                          &gl_intformat,
                                          NULL,
                                          NULL);

  tex_rect_gl->gl_texture =
    ctx->texture_driver->gen (ctx,
                              GL_TEXTURE_RECTANGLE_ARB,
                              internal_format);
  if (!ctx->texture_driver->upload_to_gl (ctx,
                                          GL_TEXTURE_RECTANGLE_ARB,
                                          tex_rect_gl->gl_texture,
                                          FALSE,
                                          upload_bmp,
                                          gl_intformat,
                                          gl_format,
                                          gl_type,
                                          error))
    {
      cogl_object_unref (upload_bmp);
      return FALSE;
    }

  tex_rect_gl->gl_format = gl_intformat;
  tex_rect->internal_format = internal_format;

  cogl_object_unref (upload_bmp);

  _cogl_texture_set_allocated (COGL_TEXTURE (tex_rect),
                               internal_format, width, height);

  return TRUE;
}

static CoglBool
allocate_from_gl_foreign (CoglTextureRectangle *tex_rect,
                          CoglTextureLoader *loader,
                          CoglError **error)
{
  CoglTextureRectangleGL *tex_rect_gl = tex_rect->winsys;
  CoglTexture *tex = COGL_TEXTURE (tex_rect);
  CoglContext *ctx = tex->context;
  CoglPixelFormat format = loader->src.gl_foreign.format;
  GLenum gl_error = 0;
  GLint gl_compressed = GL_FALSE;
  GLenum gl_int_format = 0;

  if (!ctx->texture_driver->allows_foreign_gl_target (ctx,
                                                      GL_TEXTURE_RECTANGLE_ARB))
    {
      _cogl_set_error (error,
                       COGL_SYSTEM_ERROR,
                       COGL_SYSTEM_ERROR_UNSUPPORTED,
                       "Foreign GL_TEXTURE_RECTANGLE textures are not "
                       "supported by your system");
      return FALSE;
    }

  /* Make sure binding succeeds */
  while ((gl_error = ctx->glGetError ()) != GL_NO_ERROR)
    ;

  _cogl_bind_gl_texture_transient (GL_TEXTURE_RECTANGLE_ARB,
                                   loader->src.gl_foreign.gl_handle, TRUE);
  if (ctx->glGetError () != GL_NO_ERROR)
    {
      _cogl_set_error (error,
                       COGL_SYSTEM_ERROR,
                       COGL_SYSTEM_ERROR_UNSUPPORTED,
                       "Failed to bind foreign GL_TEXTURE_RECTANGLE texture");
      return FALSE;
    }

  /* Obtain texture parameters */

#ifdef HAVE_COGL_GL
  if (_cogl_has_private_feature
      (ctx, COGL_PRIVATE_FEATURE_QUERY_TEXTURE_PARAMETERS))
    {
      GLint val;

      GE( ctx, glGetTexLevelParameteriv (GL_TEXTURE_RECTANGLE_ARB, 0,
                                         GL_TEXTURE_COMPRESSED,
                                         &gl_compressed) );

      GE( ctx, glGetTexLevelParameteriv (GL_TEXTURE_RECTANGLE_ARB, 0,
                                         GL_TEXTURE_INTERNAL_FORMAT,
                                         &val) );

      gl_int_format = val;

      /* If we can query GL for the actual pixel format then we'll ignore
         the passed in format and use that. */
      if (!ctx->driver_vtable->pixel_format_from_gl_internal (ctx,
                                                              gl_int_format,
                                                              &format))
        {
          _cogl_set_error (error,
                           COGL_SYSTEM_ERROR,
                           COGL_SYSTEM_ERROR_UNSUPPORTED,
                           "Unsupported internal format for foreign texture");
          return FALSE;
        }
    }
  else
#endif
    {
      /* Otherwise we'll assume we can derive the GL format from the
         passed in format */
      ctx->driver_vtable->pixel_format_to_gl (ctx,
                                              format,
                                              &gl_int_format,
                                              NULL,
                                              NULL);
    }

  /* Compressed texture images not supported */
  if (gl_compressed == GL_TRUE)
    {
      _cogl_set_error (error,
                       COGL_SYSTEM_ERROR,
                       COGL_SYSTEM_ERROR_UNSUPPORTED,
                       "Compressed foreign textures aren't currently supported");
      return FALSE;
    }

  /* Setup bitmap info */
  tex_rect->is_foreign = TRUE;

  tex_rect_gl->gl_texture = loader->src.gl_foreign.gl_handle;
  tex_rect_gl->gl_format = gl_int_format;

  /* Unknown filter */
  tex_rect_gl->gl_legacy_texobj_min_filter = GL_FALSE;
  tex_rect_gl->gl_legacy_texobj_mag_filter = GL_FALSE;

  tex_rect->internal_format = format;

  _cogl_texture_set_allocated (COGL_TEXTURE (tex_rect),
                               format,
                               loader->src.gl_foreign.width,
                               loader->src.gl_foreign.height);

  return TRUE;
}

CoglBool
_cogl_texture_rectangle_gl_allocate (CoglTexture *tex,
                                     CoglError **error)
{
  CoglTextureRectangle *tex_rect = COGL_TEXTURE_RECTANGLE (tex);
  CoglTextureLoader *loader = tex->loader;

  _COGL_RETURN_VAL_IF_FAIL (loader, FALSE);

  switch (loader->src_type)
    {
    case COGL_TEXTURE_SOURCE_TYPE_SIZED:
      return allocate_with_size (tex_rect, loader, error);
    case COGL_TEXTURE_SOURCE_TYPE_BITMAP:
      return allocate_from_bitmap (tex_rect, loader, error);
    case COGL_TEXTURE_SOURCE_TYPE_GL_FOREIGN:
      return allocate_from_gl_foreign (tex_rect, loader, error);
    default:
      break;
    }

  g_return_val_if_reached (FALSE);
}

void
_cogl_texture_rectangle_gl_flush_legacy_texobj_filters (CoglTextureRectangle *tex_rect,
                                                        GLenum min_filter,
                                                        GLenum mag_filter)
{
  CoglTextureRectangleGL *tex_rect_gl = tex_rect->winsys;
  CoglContext *ctx = COGL_TEXTURE (tex_rect)->context;

  if (min_filter == tex_rect_gl->gl_legacy_texobj_min_filter
      && mag_filter == tex_rect_gl->gl_legacy_texobj_mag_filter)
    return;

  /* Rectangle textures don't support mipmapping */
  g_assert (min_filter == GL_LINEAR || min_filter == GL_NEAREST);

  /* Store new values */
  tex_rect_gl->gl_legacy_texobj_min_filter = min_filter;
  tex_rect_gl->gl_legacy_texobj_mag_filter = mag_filter;

  /* Apply new filters to the texture */
  _cogl_bind_gl_texture_transient (GL_TEXTURE_RECTANGLE_ARB,
                                   tex_rect_gl->gl_texture,
                                   tex_rect->is_foreign);
  GE( ctx, glTexParameteri (GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER,
                            mag_filter) );
  GE( ctx, glTexParameteri (GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER,
                            min_filter) );
}

static CoglBool
can_use_wrap_mode (GLenum wrap_mode)
{
  return (wrap_mode == GL_CLAMP ||
          wrap_mode == GL_CLAMP_TO_EDGE ||
          wrap_mode == GL_CLAMP_TO_BORDER);
}

void
_cogl_texture_rectangle_gl_flush_legacy_texobj_wrap_modes (CoglTextureRectangle *tex_rect,
                                                           GLenum wrap_mode_s,
                                                           GLenum wrap_mode_t,
                                                           GLenum wrap_mode_p)
{
  CoglTextureRectangleGL *tex_rect_gl = tex_rect->winsys;
  CoglContext *ctx = COGL_TEXTURE (tex_rect)->context;

  /* Only set the wrap mode if it's different from the current value
     to avoid too many GL calls. Texture rectangle doesn't make use of
     the r coordinate so we can ignore its wrap mode */
  if (tex_rect_gl->gl_legacy_texobj_wrap_mode_s != wrap_mode_s ||
      tex_rect_gl->gl_legacy_texobj_wrap_mode_t != wrap_mode_t)
    {
      g_assert (can_use_wrap_mode (wrap_mode_s));
      g_assert (can_use_wrap_mode (wrap_mode_t));

      _cogl_bind_gl_texture_transient (GL_TEXTURE_RECTANGLE_ARB,
                                       tex_rect_gl->gl_texture,
                                       tex_rect->is_foreign);
      GE( ctx, glTexParameteri (GL_TEXTURE_RECTANGLE_ARB,
                                GL_TEXTURE_WRAP_S, wrap_mode_s) );
      GE( ctx, glTexParameteri (GL_TEXTURE_RECTANGLE_ARB,
                                GL_TEXTURE_WRAP_T, wrap_mode_t) );

      tex_rect_gl->gl_legacy_texobj_wrap_mode_s = wrap_mode_s;
      tex_rect_gl->gl_legacy_texobj_wrap_mode_t = wrap_mode_t;
    }
}

void
_cogl_texture_rectangle_gl_get_data (CoglTextureRectangle *tex_rect,
                                     CoglPixelFormat format,
                                     int rowstride,
                                     uint8_t *data)
{
  CoglTextureRectangleGL *tex_rect_gl = tex_rect->winsys;
  CoglTexture *tex = COGL_TEXTURE (tex_rect);
  CoglContext *ctx = tex->context;
  int bpp;
  GLenum gl_format;
  GLenum gl_type;

  bpp = _cogl_pixel_format_get_bytes_per_pixel (format);

  ctx->driver_vtable->pixel_format_to_gl (ctx,
                                          format,
                                          NULL, /* internal format */
                                          &gl_format,
                                          &gl_type);

  ctx->texture_driver->prep_gl_for_pixels_download (ctx,
                                                    rowstride,
                                                    tex->width,
                                                    bpp);

  _cogl_bind_gl_texture_transient (GL_TEXTURE_RECTANGLE_ARB,
                                   tex_rect_gl->gl_texture,
                                   tex_rect->is_foreign);

  ctx->texture_driver->gl_get_tex_image (ctx,
                                         GL_TEXTURE_RECTANGLE_ARB,
                                         gl_format,
                                         gl_type,
                                         data);
}
