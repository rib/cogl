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
#include "cogl-texture-3d-private.h"
#include "cogl-texture-gl-private.h"
#include "cogl-texture-driver.h"
#include "cogl-context-private.h"
#include "cogl-object-private.h"
#include "cogl-pipeline-opengl-private.h"
#include "cogl-error-private.h"
#include "cogl-util-gl-private.h"

#include <string.h>

/* These might not be defined on GLES */
#ifndef GL_TEXTURE_3D
#define GL_TEXTURE_3D                           0x806F
#endif
#ifndef GL_TEXTURE_WRAP_R
#define GL_TEXTURE_WRAP_R                       0x8072
#endif

typedef struct _CoglTexture3DGL CoglTexture3DGL;

struct _CoglTexture3DGL {
  /* The internal format of the GL texture represented as a GL enum */
  GLenum gl_format;
  /* The texture object number */
  GLuint gl_texture;
  GLenum gl_legacy_texobj_min_filter;
  GLenum gl_legacy_texobj_mag_filter;
  GLint gl_legacy_texobj_wrap_mode_s;
  GLint gl_legacy_texobj_wrap_mode_t;
  GLint gl_legacy_texobj_wrap_mode_p;
  CoglTexturePixel first_pixel;
};

void
_cogl_texture_3d_gl_free (CoglTexture3D *tex_3d)
{
  CoglTexture3DGL *tex_3d_gl = tex_3d->winsys;

  if (tex_3d_gl->gl_texture)
    _cogl_delete_gl_texture (tex_3d_gl->gl_texture);

  g_slice_free (CoglTexture3DGL, tex_3d_gl);
}

void
_cogl_texture_3d_gl_init (CoglTexture3D *tex_3d)
{
  CoglTexture3DGL *tex_3d_gl = g_slice_new0 (CoglTexture3DGL);

  tex_3d_gl->gl_texture = 0;

  /* We default to GL_LINEAR for both filters */
  tex_3d_gl->gl_legacy_texobj_min_filter = GL_LINEAR;
  tex_3d_gl->gl_legacy_texobj_mag_filter = GL_LINEAR;

  /* Wrap mode not yet set */
  tex_3d_gl->gl_legacy_texobj_wrap_mode_s = GL_FALSE;
  tex_3d_gl->gl_legacy_texobj_wrap_mode_t = GL_FALSE;
  tex_3d_gl->gl_legacy_texobj_wrap_mode_p = GL_FALSE;

  tex_3d->winsys = tex_3d_gl;
}

GLenum
_cogl_texture_3d_gl_get_gl_format (CoglTexture3D *tex_3d)
{
  CoglTexture3DGL *tex_3d_gl = tex_3d->winsys;

  return tex_3d_gl->gl_format;
}

unsigned int
_cogl_texture_3d_gl_get_gl_handle (CoglTexture3D *tex_3d)
{
  CoglTexture3DGL *tex_3d_gl = tex_3d->winsys;

  return tex_3d_gl->gl_texture;
}

static CoglBool
_cogl_texture_3d_can_create (CoglContext *ctx,
                             int width,
                             int height,
                             int depth,
                             CoglPixelFormat internal_format,
                             CoglError **error)
{
  GLenum gl_intformat;
  GLenum gl_type;

  /* This should only happen on GLES */
  if (!cogl_has_feature (ctx, COGL_FEATURE_ID_TEXTURE_3D))
    {
      _cogl_set_error (error,
                       COGL_SYSTEM_ERROR,
                       COGL_SYSTEM_ERROR_UNSUPPORTED,
                       "3D textures are not supported by the GPU");
      return FALSE;
    }

  /* If NPOT textures aren't supported then the size must be a power
     of two */
  if (!cogl_has_feature (ctx, COGL_FEATURE_ID_TEXTURE_NPOT) &&
      (!_cogl_util_is_pot (width) ||
       !_cogl_util_is_pot (height) ||
       !_cogl_util_is_pot (depth)))
    {
      _cogl_set_error (error,
                       COGL_SYSTEM_ERROR,
                       COGL_SYSTEM_ERROR_UNSUPPORTED,
                                              "A non-power-of-two size was requested but this is not "
                       "supported by the GPU");
      return FALSE;
    }

  ctx->driver_vtable->pixel_format_to_gl (ctx,
                                          internal_format,
                                          &gl_intformat,
                                          NULL,
                                          &gl_type);

  /* Check that the driver can create a texture with that size */
  if (!ctx->texture_driver->size_supported_3d (ctx,
                                               GL_TEXTURE_3D,
                                               gl_intformat,
                                               gl_type,
                                               width,
                                               height,
                                               depth))
    {
      _cogl_set_error (error,
                       COGL_SYSTEM_ERROR,
                       COGL_SYSTEM_ERROR_UNSUPPORTED,
                       "The requested dimensions are not supported by the GPU");
      return FALSE;
    }

  return TRUE;
}

static CoglBool
allocate_with_size (CoglTexture3D *tex_3d,
                    CoglTextureLoader *loader,
                    CoglError **error)
{
  CoglTexture3DGL *tex_3d_gl = tex_3d->winsys;
  CoglTexture *tex = COGL_TEXTURE (tex_3d);
  CoglContext *ctx = tex->context;
  CoglPixelFormat internal_format;
  int width = loader->src.sized.width;
  int height = loader->src.sized.height;
  int depth = loader->src.sized.depth;
  GLenum gl_intformat;
  GLenum gl_format;
  GLenum gl_type;
  GLenum gl_error;
  GLenum gl_texture;

  internal_format =
    _cogl_texture_determine_internal_format (tex, COGL_PIXEL_FORMAT_ANY);

  if (!_cogl_texture_3d_can_create (ctx,
                                    width,
                                    height,
                                    depth,
                                    internal_format,
                                    error))
    return FALSE;

  ctx->driver_vtable->pixel_format_to_gl (ctx,
                                          internal_format,
                                          &gl_intformat,
                                          &gl_format,
                                          &gl_type);

  gl_texture =
    ctx->texture_driver->gen (ctx, GL_TEXTURE_3D, internal_format);
  _cogl_bind_gl_texture_transient (GL_TEXTURE_3D,
                                   gl_texture,
                                   FALSE);
  /* Clear any GL errors */
  while ((gl_error = ctx->glGetError ()) != GL_NO_ERROR)
    ;

  ctx->glTexImage3D (GL_TEXTURE_3D, 0, gl_intformat,
                     width, height, depth,
                     0, gl_format, gl_type, NULL);

  if (_cogl_gl_util_catch_out_of_memory (ctx, error))
    {
      GE( ctx, glDeleteTextures (1, &gl_texture) );
      return FALSE;
    }

  tex_3d_gl->gl_texture = gl_texture;
  tex_3d_gl->gl_format = gl_intformat;

  tex_3d->depth = depth;

  tex_3d->internal_format = internal_format;

  _cogl_texture_set_allocated (tex, internal_format, width, height);

  return TRUE;
}

static CoglBool
allocate_from_bitmap (CoglTexture3D *tex_3d,
                      CoglTextureLoader *loader,
                      CoglError **error)
{
  CoglTexture3DGL *tex_3d_gl = tex_3d->winsys;
  CoglTexture *tex = COGL_TEXTURE (tex_3d);
  CoglContext *ctx = tex->context;
  CoglPixelFormat internal_format;
  CoglBitmap *bmp = loader->src.bitmap.bitmap;
  int bmp_width = cogl_bitmap_get_width (bmp);
  int height = loader->src.bitmap.height;
  int depth = loader->src.bitmap.depth;
  CoglPixelFormat bmp_format = cogl_bitmap_get_format (bmp);
  CoglBool can_convert_in_place = loader->src.bitmap.can_convert_in_place;
  CoglBitmap *upload_bmp;
  CoglPixelFormat upload_format;
  GLenum gl_intformat;
  GLenum gl_format;
  GLenum gl_type;

  internal_format = _cogl_texture_determine_internal_format (tex, bmp_format);

  if (!_cogl_texture_3d_can_create (ctx,
                                    bmp_width, height, depth,
                                    internal_format,
                                    error))
    return FALSE;

  upload_bmp = _cogl_bitmap_convert_for_upload (bmp,
                                                internal_format,
                                                can_convert_in_place,
                                                error);
  if (upload_bmp == NULL)
    return FALSE;

  upload_format = cogl_bitmap_get_format (upload_bmp);

  ctx->driver_vtable->pixel_format_to_gl (ctx,
                                          upload_format,
                                          NULL, /* internal format */
                                          &gl_format,
                                          &gl_type);
  ctx->driver_vtable->pixel_format_to_gl (ctx,
                                          internal_format,
                                          &gl_intformat,
                                          NULL,
                                          NULL);

  /* Keep a copy of the first pixel so that if glGenerateMipmap isn't
     supported we can fallback to using GL_GENERATE_MIPMAP */
  if (!cogl_has_feature (ctx, COGL_FEATURE_ID_OFFSCREEN))
    {
      CoglError *ignore = NULL;
      uint8_t *data = _cogl_bitmap_map (upload_bmp,
                                        COGL_BUFFER_ACCESS_READ, 0,
                                        &ignore);

      tex_3d_gl->first_pixel.gl_format = gl_format;
      tex_3d_gl->first_pixel.gl_type = gl_type;

      if (data)
        {
          memcpy (tex_3d_gl->first_pixel.data, data,
                  _cogl_pixel_format_get_bytes_per_pixel (upload_format));
          _cogl_bitmap_unmap (upload_bmp);
        }
      else
        {
          g_warning ("Failed to read first pixel of bitmap for "
                     "glGenerateMipmap fallback");
          cogl_error_free (ignore);
          memset (tex_3d_gl->first_pixel.data, 0,
                  _cogl_pixel_format_get_bytes_per_pixel (upload_format));
        }
    }

  tex_3d_gl->gl_texture =
    ctx->texture_driver->gen (ctx, GL_TEXTURE_3D, internal_format);

  if (!ctx->texture_driver->upload_to_gl_3d (ctx,
                                             GL_TEXTURE_3D,
                                             tex_3d_gl->gl_texture,
                                             FALSE, /* is_foreign */
                                             height,
                                             depth,
                                             upload_bmp,
                                             gl_intformat,
                                             gl_format,
                                             gl_type,
                                             error))
    {
      cogl_object_unref (upload_bmp);
      return FALSE;
    }

  tex_3d_gl->gl_format = gl_intformat;

  cogl_object_unref (upload_bmp);

  tex_3d->depth = loader->src.bitmap.depth;

  tex_3d->internal_format = internal_format;

  _cogl_texture_set_allocated (tex, internal_format,
                               bmp_width, loader->src.bitmap.height);

  return TRUE;
}

CoglBool
_cogl_texture_3d_gl_allocate (CoglTexture *tex,
                              CoglError **error)
{
  CoglTexture3D *tex_3d = COGL_TEXTURE_3D (tex);
  CoglTextureLoader *loader = tex->loader;

  _COGL_RETURN_VAL_IF_FAIL (loader, FALSE);

  switch (loader->src_type)
    {
    case COGL_TEXTURE_SOURCE_TYPE_SIZED:
      return allocate_with_size (tex_3d, loader, error);
    case COGL_TEXTURE_SOURCE_TYPE_BITMAP:
      return allocate_from_bitmap (tex_3d, loader, error);
    default:
      break;
    }

  g_return_val_if_reached (FALSE);
}

void
_cogl_texture_3d_gl_generate_mipmap (CoglTexture3D *tex_3d)
{
  CoglTexture3DGL *tex_3d_gl = tex_3d->winsys;
  CoglContext *ctx = COGL_TEXTURE (tex_3d)->context;

  /* glGenerateMipmap is defined in the FBO extension. If it's not available
     we'll fallback to temporarily enabling GL_GENERATE_MIPMAP and
     reuploading the first pixel */
  if (cogl_has_feature (ctx, COGL_FEATURE_ID_OFFSCREEN))
    _cogl_texture_gl_generate_mipmaps (COGL_TEXTURE (tex_3d));
#if defined (HAVE_COGL_GL) || defined (HAVE_COGL_GLES)
  else if (_cogl_has_private_feature (ctx, COGL_PRIVATE_FEATURE_GL_FIXED))
    {
      _cogl_bind_gl_texture_transient (GL_TEXTURE_3D,
                                       tex_3d_gl->gl_texture,
                                       FALSE);

      GE( ctx, glTexParameteri (GL_TEXTURE_3D,
                                GL_GENERATE_MIPMAP,
                                GL_TRUE) );
      GE( ctx, glTexSubImage3D (GL_TEXTURE_3D,
                                0, /* level */
                                0, /* xoffset */
                                0, /* yoffset */
                                0, /* zoffset */
                                1, /* width */
                                1, /* height */
                                1, /* depth */
                                tex_3d_gl->first_pixel.gl_format,
                                tex_3d_gl->first_pixel.gl_type,
                                tex_3d_gl->first_pixel.data) );
      GE( ctx, glTexParameteri (GL_TEXTURE_3D,
                                GL_GENERATE_MIPMAP,
                                GL_FALSE) );
    }
#endif
}

void
_cogl_texture_3d_gl_flush_legacy_texobj_filters (CoglTexture3D *tex_3d,
                                                 GLenum min_filter,
                                                 GLenum mag_filter)
{
  CoglTexture3DGL *tex_3d_gl = tex_3d->winsys;
  CoglContext *ctx = COGL_TEXTURE (tex_3d)->context;

  if (min_filter == tex_3d_gl->gl_legacy_texobj_min_filter
      && mag_filter == tex_3d_gl->gl_legacy_texobj_mag_filter)
    return;

  /* Store new values */
  tex_3d_gl->gl_legacy_texobj_min_filter = min_filter;
  tex_3d_gl->gl_legacy_texobj_mag_filter = mag_filter;

  /* Apply new filters to the texture */
  _cogl_bind_gl_texture_transient (GL_TEXTURE_3D,
                                   tex_3d_gl->gl_texture,
                                   FALSE);
  GE( ctx, glTexParameteri (GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, mag_filter) );
  GE( ctx, glTexParameteri (GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, min_filter) );
}

void
_cogl_texture_3d_gl_flush_legacy_texobj_wrap_modes (CoglTexture3D *tex_3d,
                                                    GLenum wrap_mode_s,
                                                    GLenum wrap_mode_t,
                                                    GLenum wrap_mode_p)
{
  CoglTexture3DGL *tex_3d_gl = tex_3d->winsys;
  CoglContext *ctx = COGL_TEXTURE (tex_3d)->context;

  /* Only set the wrap mode if it's different from the current value
     to avoid too many GL calls. */
  if (tex_3d_gl->gl_legacy_texobj_wrap_mode_s != wrap_mode_s ||
      tex_3d_gl->gl_legacy_texobj_wrap_mode_t != wrap_mode_t ||
      tex_3d_gl->gl_legacy_texobj_wrap_mode_p != wrap_mode_p)
    {
      _cogl_bind_gl_texture_transient (GL_TEXTURE_3D,
                                       tex_3d_gl->gl_texture,
                                       FALSE);
      GE( ctx, glTexParameteri (GL_TEXTURE_3D,
                                GL_TEXTURE_WRAP_S,
                                wrap_mode_s) );
      GE( ctx, glTexParameteri (GL_TEXTURE_3D,
                                GL_TEXTURE_WRAP_T,
                                wrap_mode_t) );
      GE( ctx, glTexParameteri (GL_TEXTURE_3D,
                                GL_TEXTURE_WRAP_R,
                                wrap_mode_p) );

      tex_3d_gl->gl_legacy_texobj_wrap_mode_s = wrap_mode_s;
      tex_3d_gl->gl_legacy_texobj_wrap_mode_t = wrap_mode_t;
      tex_3d_gl->gl_legacy_texobj_wrap_mode_p = wrap_mode_p;
    }
}
