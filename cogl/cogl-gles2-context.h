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

#if !defined(__COGL_H_INSIDE__) && !defined(CLUTTER_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_GLES2_CONTEXT_H__
#define __COGL_GLES2_CONTEXT_H__

#include <cogl/cogl-defines.h>
#include <cogl/cogl-context.h>

G_BEGIN_DECLS

typedef struct _CoglGLES2Context CoglGLES2Context;
typedef struct _CoglGLES2Vtable CoglGLES2Vtable;

struct _CoglGLES2Vtable
{
#define COGL_EXT_BEGIN(name, \
                       min_gl_major, min_gl_minor, \
                       gles_availability, \
                       extension_suffixes, extension_names)

#define COGL_EXT_FUNCTION(ret, name, args) \
  ret (* name) args;

#define COGL_EXT_END()

#include <cogl/cogl-ext-functions.h>

#undef COGL_EXT_BEGIN
#undef COGL_EXT_FUNCTION
#undef COGL_EXT_END
};

CoglGLES2Context *cogl_gles2_context_new (CoglContext *ctx, GError **error);

CoglGLES2Vtable *cogl_gles2_context_get_vtable (CoglGLES2Context *gles2_ctx);

G_END_DECLS

#endif /* __COGL_GLES2_CONTEXT_H__ */

