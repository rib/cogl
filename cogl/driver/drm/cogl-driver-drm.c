/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2012 Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "cogl-private.h"
#include "cogl-context-private.h"
#include "cogl-feature-private.h"
#include "cogl-renderer-private.h"
#include "cogl-error-private.h"
#include "cogl-framebuffer-drm-private.h"
#include "cogl-texture-2d-drm-private.h"
#include "cogl-attribute-drm-private.h"
#include "cogl-clip-stack-drm-private.h"

static CoglBool
_cogl_driver_update_features (CoglContext *ctx,
                              CoglError **error)
{
  /* _cogl_gpu_info_init (ctx, &ctx->gpu); */

  ctx->private_feature_flags = 0;

  return TRUE;
}

const CoglDriverVtable
_cogl_driver_drm =
  {
    NULL, /* pixel_format_from_gl_internal */
    NULL, /* pixel_format_to_gl */
    _cogl_driver_update_features,
    _cogl_offscreen_drm_allocate,
    _cogl_offscreen_drm_free,
    _cogl_framebuffer_drm_flush_state,
    _cogl_framebuffer_drm_clear,
    _cogl_framebuffer_drm_query_bits,
    _cogl_framebuffer_drm_finish,
    _cogl_framebuffer_drm_discard_buffers,
    _cogl_framebuffer_drm_draw_attributes,
    _cogl_framebuffer_drm_draw_indexed_attributes,
    _cogl_framebuffer_drm_read_pixels_into_bitmap,
    _cogl_texture_2d_drm_free,
    _cogl_texture_2d_drm_can_create,
    _cogl_texture_2d_drm_init,
    _cogl_texture_2d_drm_allocate,
    _cogl_texture_2d_drm_new_from_bitmap,
#if defined (COGL_HAS_EGL_SUPPORT) && defined (EGL_KHR_image_base)
    _cogl_egl_texture_2d_drm_new_from_image,
#endif
    _cogl_texture_2d_drm_copy_from_framebuffer,
    _cogl_texture_2d_drm_get_gl_handle,
    _cogl_texture_2d_drm_generate_mipmap,
    _cogl_texture_2d_drm_copy_from_bitmap,
    NULL, /* texture_2d_get_data */
    _cogl_drm_flush_attributes_state,
    _cogl_clip_stack_drm_flush,
  };
