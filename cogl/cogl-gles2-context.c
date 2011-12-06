/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2011 Collabora Ltd.
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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *  Tomeu Vizoso <tomeu.vizoso@collabora.com>
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-gles2-context.h"
#include "cogl-gles2-context-private.h"

#include "cogl-context-private.h"
#include "cogl-display-private.h"
#include "cogl-framebuffer-private.h"
#include "cogl-onscreen-template-private.h"
#include "cogl-renderer-private.h"
#include "cogl-swap-chain-private.h"
#include "cogl-texture-2d-private.h"
#include "winsys/cogl-winsys-egl-private.h"

GQuark
_cogl_gles2_context_error_quark (void)
{
  return g_quark_from_static_string ("cogl-gles2-context-error-quark");
}

#define COGL_GLES2_CONTEXT_ERROR (_cogl_gles2_context_error_quark ())

typedef enum { /*< prefix=COGL_GLES2_CONTEXT_ERROR >*/
  COGL_GLES2_CONTEXT_ERROR_NEW,
  COGL_GLES2_CONTEXT_ERROR_PUSH_CONTEXT,
} CoglGLES2ContextError;

CoglGLES2Context *
cogl_gles2_context_new (CoglContext *ctx, GError **error)
{
  CoglGLES2Context *gles2_context;
  const CoglWinsysVtable *winsys;

  if (!cogl_has_feature (ctx, COGL_FEATURE_ID_GLES2_CONTEXT))
    {
      g_set_error (error, COGL_GLES2_CONTEXT_ERROR,
                   COGL_GLES2_CONTEXT_ERROR_NEW,
                   "Backend doesn't support creating GLES2 contexts");

      return NULL;
    }

  gles2_context = g_malloc (sizeof (CoglGLES2Context));

  cogl_object_ref (ctx);
  gles2_context->context = ctx;

  winsys = ctx->display->renderer->winsys_vtable;
  gles2_context->winsys = winsys->create_gles2_context (ctx, error);
  if (gles2_context->winsys == NULL)
    {
      g_free (gles2_context);
      return NULL;
    }

  return gles2_context;
}

CoglGLES2Vtable *
cogl_gles2_context_get_vtable (CoglGLES2Context *gles2_ctx)
{
  CoglContext *ctx = gles2_ctx->context;
  CoglGLES2Vtable *vtable = g_malloc (sizeof (CoglGLES2Vtable));

#define COGL_EXT_BEGIN(name, \
                       min_gl_major, min_gl_minor, \
                       gles_availability, \
                       extension_suffixes, extension_names)

#define COGL_EXT_FUNCTION(ret, name, args) \
  vtable->name = ctx->name;

#define COGL_EXT_END()

#include "cogl-ext-functions.h"

#undef COGL_EXT_BEGIN
#undef COGL_EXT_FUNCTION
#undef COGL_EXT_END

  return vtable;
}
