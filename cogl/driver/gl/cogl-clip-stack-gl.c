/*
 * Cogl
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2007,2008,2009,2010,2011,2012 Intel Corporation.
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
 *  Robert Bragg   <robert@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-context-private.h"
#include "cogl-util-gl-private.h"
#include "cogl-primitives-private.h"
#include "cogl-pipeline-opengl-private.h"
#include "cogl-clip-stack-gl-private.h"
#include "cogl-primitive-private.h"

static void
add_stencil_clip_rectangle (CoglFramebuffer *framebuffer,
                            CoglMatrixEntry *modelview_entry,
                            float x_1,
                            float y_1,
                            float x_2,
                            float y_2,
                            CoglBool first)
{
  CoglMatrixStack *projection_stack =
    _cogl_framebuffer_get_projection_stack (framebuffer);
  CoglContext *ctx = cogl_framebuffer_get_context (framebuffer);

  /* NB: This can be called while flushing the journal so we need
   * to be very conservative with what state we change.
   */

  _cogl_context_set_current_projection_entry (ctx,
                                              projection_stack->last_entry);
  _cogl_context_set_current_modelview_entry (ctx, modelview_entry);

  if (first)
    {
      GE( ctx, glEnable (GL_STENCIL_TEST) );

      /* Initially disallow everything */
      GE( ctx, glClearStencil (0) );
      GE( ctx, glClear (GL_STENCIL_BUFFER_BIT) );

      /* Punch out a hole to allow the rectangle */
      GE( ctx, glStencilFunc (GL_NEVER, 0x1, 0x1) );
      GE( ctx, glStencilOp (GL_REPLACE, GL_REPLACE, GL_REPLACE) );

      _cogl_rectangle_immediate (framebuffer,
                                 ctx->stencil_pipeline,
                                 x_1, y_1, x_2, y_2);
    }
  else
    {
      /* Add one to every pixel of the stencil buffer in the
	 rectangle */
      GE( ctx, glStencilFunc (GL_NEVER, 0x1, 0x3) );
      GE( ctx, glStencilOp (GL_INCR, GL_INCR, GL_INCR) );
      _cogl_rectangle_immediate (framebuffer,
                                 ctx->stencil_pipeline,
                                 x_1, y_1, x_2, y_2);

      /* Subtract one from all pixels in the stencil buffer so that
	 only pixels where both the original stencil buffer and the
	 rectangle are set will be valid */
      GE( ctx, glStencilOp (GL_DECR, GL_DECR, GL_DECR) );

      _cogl_context_set_current_projection_entry (ctx, &ctx->identity_entry);
      _cogl_context_set_current_modelview_entry (ctx, &ctx->identity_entry);

      _cogl_rectangle_immediate (framebuffer,
                                 ctx->stencil_pipeline,
                                 -1.0, -1.0, 1.0, 1.0);
    }

  /* Restore the stencil mode */
  GE( ctx, glStencilFunc (GL_EQUAL, 0x1, 0x1) );
  GE( ctx, glStencilOp (GL_KEEP, GL_KEEP, GL_KEEP) );
}

typedef void (*SilhouettePaintCallback) (CoglFramebuffer *framebuffer,
                                         CoglPipeline *pipeline,
                                         void *user_data);

static void
add_stencil_clip_silhouette (CoglFramebuffer *framebuffer,
                             SilhouettePaintCallback silhouette_callback,
                             CoglMatrixEntry *modelview_entry,
                             float bounds_x1,
                             float bounds_y1,
                             float bounds_x2,
                             float bounds_y2,
                             CoglBool merge,
                             CoglBool need_clear,
                             void *user_data)
{
  CoglMatrixStack *projection_stack =
    _cogl_framebuffer_get_projection_stack (framebuffer);
  CoglContext *ctx = cogl_framebuffer_get_context (framebuffer);

  /* NB: This can be called while flushing the journal so we need
   * to be very conservative with what state we change.
   */

  _cogl_context_set_current_projection_entry (ctx,
                                              projection_stack->last_entry);
  _cogl_context_set_current_modelview_entry (ctx, modelview_entry);

  _cogl_pipeline_flush_gl_state (ctx, ctx->stencil_pipeline,
                                 framebuffer, FALSE, FALSE);

  GE( ctx, glEnable (GL_STENCIL_TEST) );

  GE( ctx, glColorMask (FALSE, FALSE, FALSE, FALSE) );
  GE( ctx, glDepthMask (FALSE) );

  if (merge)
    {
      GE (ctx, glStencilMask (2));
      GE (ctx, glStencilFunc (GL_LEQUAL, 0x2, 0x6));
    }
  else
    {
      /* If we're not using the stencil buffer for clipping then we
         don't need to clear the whole stencil buffer, just the area
         that will be drawn */
      if (need_clear)
        /* If this is being called from the clip stack code then it
           will have set up a scissor for the minimum bounding box of
           all of the clips. That box will likely mean that this
           _cogl_clear won't need to clear the entire
           buffer. _cogl_framebuffer_clear_without_flush4f is used instead
           of cogl_clear because it won't try to flush the journal */
        _cogl_framebuffer_clear_without_flush4f (framebuffer,
                                                 COGL_BUFFER_BIT_STENCIL,
                                                 0, 0, 0, 0);
      else
        {
          /* Just clear the bounding box */
          GE( ctx, glStencilMask (~(GLuint) 0) );
          GE( ctx, glStencilOp (GL_ZERO, GL_ZERO, GL_ZERO) );
          _cogl_rectangle_immediate (framebuffer,
                                     ctx->stencil_pipeline,
                                     bounds_x1, bounds_y1,
                                     bounds_x2, bounds_y2);
        }
      GE (ctx, glStencilMask (1));
      GE (ctx, glStencilFunc (GL_LEQUAL, 0x1, 0x3));
    }

  GE (ctx, glStencilOp (GL_INVERT, GL_INVERT, GL_INVERT));

  silhouette_callback (framebuffer, ctx->stencil_pipeline, user_data);

  if (merge)
    {
      /* Now we have the new stencil buffer in bit 1 and the old
         stencil buffer in bit 0 so we need to intersect them */
      GE (ctx, glStencilMask (3));
      GE (ctx, glStencilFunc (GL_NEVER, 0x2, 0x3));
      GE (ctx, glStencilOp (GL_DECR, GL_DECR, GL_DECR));
      /* Decrement all of the bits twice so that only pixels where the
         value is 3 will remain */

      _cogl_context_set_current_projection_entry (ctx, &ctx->identity_entry);
      _cogl_context_set_current_modelview_entry (ctx, &ctx->identity_entry);

      _cogl_rectangle_immediate (framebuffer, ctx->stencil_pipeline,
                                 -1.0, -1.0, 1.0, 1.0);
      _cogl_rectangle_immediate (framebuffer, ctx->stencil_pipeline,
                                 -1.0, -1.0, 1.0, 1.0);
    }

  GE (ctx, glStencilMask (~(GLuint) 0));
  GE (ctx, glDepthMask (TRUE));
  GE (ctx, glColorMask (TRUE, TRUE, TRUE, TRUE));

  GE (ctx, glStencilFunc (GL_EQUAL, 0x1, 0x1));
  GE (ctx, glStencilOp (GL_KEEP, GL_KEEP, GL_KEEP));
}

static void
paint_primitive_silhouette (CoglFramebuffer *framebuffer,
                            CoglPipeline *pipeline,
                            void *user_data)
{
  _cogl_primitive_draw (user_data,
                        framebuffer,
                        pipeline,
                        COGL_DRAW_SKIP_JOURNAL_FLUSH |
                        COGL_DRAW_SKIP_PIPELINE_VALIDATION |
                        COGL_DRAW_SKIP_FRAMEBUFFER_FLUSH);
}

static void
add_stencil_clip_primitive (CoglFramebuffer *framebuffer,
                            CoglMatrixEntry *modelview_entry,
                            CoglPrimitive *primitive,
                            float bounds_x1,
                            float bounds_y1,
                            float bounds_x2,
                            float bounds_y2,
                            CoglBool merge,
                            CoglBool need_clear)
{
  add_stencil_clip_silhouette (framebuffer,
                               paint_primitive_silhouette,
                               modelview_entry,
                               bounds_x1,
                               bounds_y1,
                               bounds_x2,
                               bounds_y2,
                               merge,
                               need_clear,
                               primitive);
}

void
_cogl_clip_stack_gl_flush (CoglClipStack *stack,
                           CoglFramebuffer *framebuffer)
{
  CoglContext *ctx = framebuffer->context;
  CoglBool using_stencil_buffer = FALSE;
  int scissor_x0;
  int scissor_y0;
  int scissor_x1;
  int scissor_y1;
  CoglClipStack *entry;
  int scissor_y_start;

  /* If we have already flushed this state then we don't need to do
     anything */
  if (ctx->current_clip_stack_valid)
    {
      if (ctx->current_clip_stack == stack &&
          (ctx->needs_viewport_scissor_workaround == FALSE ||
           (framebuffer->viewport_age ==
            framebuffer->viewport_age_for_scissor_workaround &&
            ctx->viewport_scissor_workaround_framebuffer ==
            framebuffer)))
        return;

      _cogl_clip_stack_unref (ctx->current_clip_stack);
    }

  ctx->current_clip_stack_valid = TRUE;
  ctx->current_clip_stack = _cogl_clip_stack_ref (stack);

  GE( ctx, glDisable (GL_STENCIL_TEST) );

  /* If the stack is empty then there's nothing else to do
   *
   * See comment below about ctx->needs_viewport_scissor_workaround
   */
  if (stack == NULL && !ctx->needs_viewport_scissor_workaround)
    {
      COGL_NOTE (CLIPPING, "Flushed empty clip stack");

      GE (ctx, glDisable (GL_SCISSOR_TEST));
      return;
    }

  /* Calculate the scissor rect first so that if we eventually have to
     clear the stencil buffer then the clear will be clipped to the
     intersection of all of the bounding boxes. This saves having to
     clear the whole stencil buffer */
  _cogl_clip_stack_get_bounds (stack,
                               &scissor_x0, &scissor_y0,
                               &scissor_x1, &scissor_y1);

  /* XXX: ONGOING BUG: Intel viewport scissor
   *
   * Intel gen6 drivers don't correctly handle offset viewports, since
   * primitives aren't clipped within the bounds of the viewport.  To
   * workaround this we push our own clip for the viewport that will
   * use scissoring to ensure we clip as expected.
   *
   * TODO: file a bug upstream!
   */
  if (ctx->needs_viewport_scissor_workaround)
    {
      _cogl_util_scissor_intersect (framebuffer->viewport_x,
                                    framebuffer->viewport_y,
                                    framebuffer->viewport_x +
                                      framebuffer->viewport_width,
                                    framebuffer->viewport_y +
                                      framebuffer->viewport_height,
                                    &scissor_x0, &scissor_y0,
                                    &scissor_x1, &scissor_y1);
      framebuffer->viewport_age_for_scissor_workaround =
        framebuffer->viewport_age;
      ctx->viewport_scissor_workaround_framebuffer =
        framebuffer;
    }

  /* Enable scissoring as soon as possible */
  if (scissor_x0 >= scissor_x1 || scissor_y0 >= scissor_y1)
    scissor_x0 = scissor_y0 = scissor_x1 = scissor_y1 = scissor_y_start = 0;
  else
    {
      /* We store the entry coordinates in Cogl coordinate space
       * but OpenGL requires the window origin to be the bottom
       * left so we may need to convert the incoming coordinates.
       *
       * NB: Cogl forces all offscreen rendering to be done upside
       * down so in this case no conversion is needed.
       */

      if (cogl_is_offscreen (framebuffer))
        scissor_y_start = scissor_y0;
      else
        {
          int framebuffer_height =
            cogl_framebuffer_get_height (framebuffer);

          scissor_y_start = framebuffer_height - scissor_y1;
        }
    }

  COGL_NOTE (CLIPPING, "Flushing scissor to (%i, %i, %i, %i)",
             scissor_x0, scissor_y0,
             scissor_x1, scissor_y1);

  GE (ctx, glEnable (GL_SCISSOR_TEST));
  GE (ctx, glScissor (scissor_x0, scissor_y_start,
                      scissor_x1 - scissor_x0,
                      scissor_y1 - scissor_y0));

  /* Add all of the entries. This will end up adding them in the
     reverse order that they were specified but as all of the clips
     are intersecting it should work out the same regardless of the
     order */
  for (entry = stack; entry; entry = entry->parent)
    {
      switch (entry->type)
        {
        case COGL_CLIP_STACK_PRIMITIVE:
            {
              CoglClipStackPrimitive *primitive_entry =
                (CoglClipStackPrimitive *) entry;

              COGL_NOTE (CLIPPING, "Adding stencil clip for primitive");

              add_stencil_clip_primitive (framebuffer,
                                          primitive_entry->matrix_entry,
                                          primitive_entry->primitive,
                                          primitive_entry->bounds_x1,
                                          primitive_entry->bounds_y1,
                                          primitive_entry->bounds_x2,
                                          primitive_entry->bounds_y2,
                                          using_stencil_buffer,
                                          TRUE);

              using_stencil_buffer = TRUE;
              break;
            }
        case COGL_CLIP_STACK_RECT:
            {
              CoglClipStackRect *rect = (CoglClipStackRect *) entry;

              /* We don't need to do anything extra if the clip for this
                 rectangle was entirely described by its scissor bounds */
              if (!rect->can_be_scissor)
                {
                  COGL_NOTE (CLIPPING, "Adding stencil clip for rectangle");

                  add_stencil_clip_rectangle (framebuffer,
                                              rect->matrix_entry,
                                              rect->x0,
                                              rect->y0,
                                              rect->x1,
                                              rect->y1,
                                              !using_stencil_buffer);
                  using_stencil_buffer = TRUE;
                }
              break;
            }
        case COGL_CLIP_STACK_WINDOW_RECT:
          break;
          /* We don't need to do anything for window space rectangles because
           * their functionality is entirely implemented by the entry bounding
           * box */
        }
    }
}
