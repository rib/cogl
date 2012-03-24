/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2011 Collabora Ltd.
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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *  Tomeu Vizoso <tomeu.vizoso@collabora.com>
 *  Robert Bragg <robert@linux.intel.com>
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

static void _cogl_gles2_context_free (CoglGLES2Context *gles2_context);

COGL_OBJECT_DEFINE (GLES2Context, gles2_context);

static CoglGLES2Context *current_gles2_context;

GQuark
_cogl_gles2_context_error_quark (void)
{
  return g_quark_from_static_string ("cogl-gles2-context-error-quark");
}

/* We wrap glBindFramebuffer so that when framebuffer 0 is bound
 * we can instead bind the write_framebuffer passed to
 * cogl_push_gles2_context().
 */
static void
gl_bind_framebuffer_wrapper (GLenum target, GLuint framebuffer)
{
  CoglGLES2Context *gles2_ctx = current_gles2_context;

  gles2_ctx->current_fbo_handle = framebuffer;

  if (framebuffer == 0)
    {
      CoglGLES2Framebuffer *write = gles2_ctx->write_buffer;
      framebuffer = write->gl_framebuffer.fbo_handle;
    }

  gles2_ctx->context->glBindFramebuffer (target, framebuffer);
}

/* We wrap glReadPixels so when framebuffer 0 is bound then we can
 * read from the read_framebuffer passed to cogl_push_gles2_context().
 */
static void
gl_read_pixels_wrapper (GLint x,
                        GLint y,
                        GLsizei width,
                        GLsizei height,
                        GLenum format,
                        GLenum type,
                        GLvoid *pixels)
{
  CoglGLES2Context *gles2_ctx = current_gles2_context;

  if (gles2_ctx->current_fbo_handle == 0)
    {
      CoglGLES2Framebuffer *read = gles2_ctx->read_buffer;
      CoglGLES2Framebuffer *write = gles2_ctx->write_buffer;

      gles2_ctx->context->glBindFramebuffer (GL_FRAMEBUFFER,
                                             read->gl_framebuffer.fbo_handle);
      gles2_ctx->context->glReadPixels (x, y,
                                        width, height,
                                        format, type, pixels);
      gles2_ctx->context->glBindFramebuffer (GL_FRAMEBUFFER,
                                             write->gl_framebuffer.fbo_handle);
    }
  else
    gles2_ctx->context->glReadPixels (x, y,
                                      width, height, format, type, pixels);
}

static void
_cogl_gles2_context_free (CoglGLES2Context *gles2_context)
{
  CoglContext *ctx = gles2_context->context;
  const CoglWinsysVtable *winsys;
  GList *l;

  winsys = ctx->display->renderer->winsys_vtable;
  winsys->destroy_gles2_context (gles2_context);

  for (l = gles2_context->foreign_framebuffers; l; l = l->next)
    {
      CoglGLES2Framebuffer *gles2_framebuffer = l->data;

      cogl_object_unref (gles2_framebuffer->original);
      g_slice_free (CoglGLES2Framebuffer, gles2_framebuffer);
    }
  g_list_free (gles2_context->foreign_framebuffers);

  g_object_unref (gles2_context->context);

  g_free (gles2_context->vtable);

  g_free (gles2_context);
}

CoglGLES2Context *
cogl_gles2_context_new (CoglContext *ctx, GError **error)
{
  CoglGLES2Context *gles2_ctx;
  const CoglWinsysVtable *winsys;

  if (!cogl_has_feature (ctx, COGL_FEATURE_ID_GLES2_CONTEXT))
    {
      g_set_error (error, COGL_GLES2_CONTEXT_ERROR,
                   COGL_GLES2_CONTEXT_ERROR_UNSUPPORTED,
                   "Backend doesn't support creating GLES2 contexts");

      return NULL;
    }

  gles2_ctx = g_malloc0 (sizeof (CoglGLES2Context));

  cogl_object_ref (ctx);
  gles2_ctx->context = ctx;

  winsys = ctx->display->renderer->winsys_vtable;
  gles2_ctx->winsys = winsys->context_create_gles2_context (ctx, error);
  if (gles2_ctx->winsys == NULL)
    {
      cogl_object_unref (gles2_ctx->context);
      g_free (gles2_ctx);
      return NULL;
    }

  gles2_ctx->vtable = g_malloc0 (sizeof (CoglGLES2Vtable));
#define COGL_EXT_BEGIN(name, \
                       min_gl_major, min_gl_minor, \
                       gles_availability, \
                       extension_suffixes, extension_names)

#define COGL_EXT_FUNCTION(ret, name, args) \
  gles2_ctx->vtable->name = ctx->name;

#define COGL_EXT_END()

#include "gl-prototypes/cogl-gles2-functions.h"

#undef COGL_EXT_BEGIN
#undef COGL_EXT_FUNCTION
#undef COGL_EXT_END

  gles2_ctx->vtable->glBindFramebuffer = gl_bind_framebuffer_wrapper;
  gles2_ctx->vtable->glReadPixels = gl_read_pixels_wrapper;

  return _cogl_gles2_context_object_new (gles2_ctx);
}

const CoglGLES2Vtable *
cogl_gles2_context_get_vtable (CoglGLES2Context *gles2_ctx)
{
  return gles2_ctx->vtable;
}

/* When drawing to a CoglFramebuffer from separate context we have
 * to be able to allocate ancillary buffers for that context...
 */
static CoglGLES2Framebuffer *
_cogl_gles2_framebuffer_allocate (CoglFramebuffer *framebuffer,
                                  CoglGLES2Context *gles2_context,
                                  GError **error)
{
  CoglOffscreen *offscreen = COGL_OFFSCREEN (framebuffer);
  const CoglWinsysVtable *winsys;
  GList *l;
  GError *internal_error = NULL;
  CoglGLES2Framebuffer *gles2_framebuffer;

  _COGL_RETURN_VAL_IF_FAIL (cogl_is_offscreen (framebuffer), FALSE);

  if (!framebuffer->allocated &&
      !cogl_framebuffer_allocate (framebuffer, error))
    {
      return NULL;
    }

  for (l = gles2_context->foreign_framebuffers; l; l = l->next)
    {
      CoglGLES2Framebuffer *gles2_framebuffer = l->data;
      if (gles2_framebuffer->original == framebuffer)
        return gles2_framebuffer;
    }

  winsys = _cogl_framebuffer_get_winsys (framebuffer);
  winsys->save_context (framebuffer->context);
  if (!winsys->set_gles2_context (gles2_context, &internal_error))
    {
      winsys->restore_context (framebuffer->context);

      g_error_free (internal_error);
      g_set_error (error, COGL_FRAMEBUFFER_ERROR,
                   COGL_FRAMEBUFFER_ERROR_ALLOCATE,
                   "Failed to bind gles2 context to create framebuffer");
      return NULL;
    }

  gles2_framebuffer = g_slice_new0 (CoglGLES2Framebuffer);
  if (!_cogl_framebuffer_try_creating_gl_fbo (gles2_context->context,
                                          offscreen->texture,
                                          offscreen->texture_level,
                                          offscreen->texture_level_width,
                                          offscreen->texture_level_height,
                                          &COGL_FRAMEBUFFER (offscreen)->config,
                                          offscreen->allocation_flags,
                                          &gles2_framebuffer->gl_framebuffer))
    {
      winsys->restore_context (framebuffer->context);

      g_slice_free (CoglGLES2Framebuffer, gles2_framebuffer);

      g_set_error (error, COGL_FRAMEBUFFER_ERROR,
                   COGL_FRAMEBUFFER_ERROR_ALLOCATE,
                   "Failed to create an OpenGL framebuffer object");
      return NULL;
    }

  winsys->restore_context (framebuffer->context);

  gles2_framebuffer->original = cogl_object_ref (framebuffer);

  gles2_context->foreign_framebuffers =
    g_list_prepend (gles2_context->foreign_framebuffers,
                    gles2_framebuffer);

  return gles2_framebuffer;
}

gboolean
cogl_push_gles2_context (CoglContext *ctx,
                         CoglGLES2Context *gles2_ctx,
                         CoglFramebuffer *read_buffer,
                         CoglFramebuffer *write_buffer,
                         GError **error)
{
  const CoglWinsysVtable *winsys = ctx->display->renderer->winsys_vtable;

  _COGL_RETURN_VAL_IF_FAIL (gles2_ctx != NULL, FALSE);
  _COGL_RETURN_VAL_IF_FAIL (cogl_is_offscreen (read_buffer), FALSE);
  _COGL_RETURN_VAL_IF_FAIL (cogl_is_offscreen (write_buffer), FALSE);

  if (ctx->gles2_context_stack->length == 0)
    {
      _cogl_journal_flush (read_buffer->journal, read_buffer);
      if (write_buffer != read_buffer)
        _cogl_journal_flush (write_buffer->journal, write_buffer);
      winsys->save_context (ctx);
    }
  else
    gles2_ctx->vtable->glFlush ();

  winsys->set_gles2_context (gles2_ctx, error);

  g_queue_push_tail (ctx->gles2_context_stack, gles2_ctx);

  gles2_ctx->read_buffer = _cogl_gles2_framebuffer_allocate (read_buffer,
                                                             gles2_ctx,
                                                             error);
  if (!gles2_ctx->read_buffer)
    {
      winsys->restore_context (ctx);
      return FALSE;
    }

  gles2_ctx->write_buffer = _cogl_gles2_framebuffer_allocate (write_buffer,
                                                              gles2_ctx,
                                                              error);
  if (!gles2_ctx->write_buffer)
    {
      winsys->restore_context (ctx);
      return FALSE;
    }

  current_gles2_context = gles2_ctx;
  return TRUE;
}

CoglGLES2Vtable *
cogl_gles2_get_current_vtable (void)
{
  return current_gles2_context ? current_gles2_context->vtable : NULL;
}

void
cogl_pop_gles2_context (CoglContext *ctx)
{
  CoglGLES2Context *gles2_ctx;
  const CoglWinsysVtable *winsys = ctx->display->renderer->winsys_vtable;

  _COGL_RETURN_IF_FAIL (ctx->gles2_context_stack->length > 0);

  g_queue_pop_tail (ctx->gles2_context_stack);

  gles2_ctx = g_queue_peek_tail (ctx->gles2_context_stack);

  if (gles2_ctx)
    {
      winsys->set_gles2_context (gles2_ctx, NULL);
      current_gles2_context = NULL;
    }
  else
    {
      winsys->restore_context (ctx);
      current_gles2_context = gles2_ctx;
    }
}

CoglTexture2D *
cogl_gles2_texture_2d_new_from_handle (CoglContext *ctx,
                                       CoglGLES2Context *gles2_ctx,
                                       unsigned int handle,
                                       int width,
                                       int height,
                                       CoglPixelFormat internal_format,
                                       GError **error)
{
  return cogl_texture_2d_new_from_foreign (ctx,
                                           handle,
                                           width,
                                           height,
                                           internal_format,
                                           error);
}

gboolean
cogl_gles2_texture_get_handle (CoglTexture *texture,
                               unsigned int *handle,
                               unsigned int *target)
{
  return cogl_texture_get_gl_texture (texture, handle, target);
}
