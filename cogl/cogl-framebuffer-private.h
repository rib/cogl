/*
 * Cogl
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2007,2008,2009 Intel Corporation.
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

#ifndef __COGL_FRAMEBUFFER_PRIVATE_H
#define __COGL_FRAMEBUFFER_PRIVATE_H

#include "cogl-object-private.h"
#include "cogl-matrix-stack-private.h"
#include "cogl-journal-private.h"
#include "cogl-winsys-private.h"
#include "cogl-attribute-private.h"
#include "cogl-offscreen.h"
#include "cogl-gl-header.h"
#include "cogl-clip-stack.h"

#ifdef COGL_HAS_XLIB_SUPPORT
#include <X11/Xlib.h>
#endif

#ifdef COGL_HAS_GLX_SUPPORT
#include <GL/glx.h>
#include <GL/glxext.h>
#endif

typedef enum _CoglFramebufferType {
  COGL_FRAMEBUFFER_TYPE_ONSCREEN,
  COGL_FRAMEBUFFER_TYPE_OFFSCREEN
} CoglFramebufferType;

typedef struct
{
  CoglBool has_alpha;
  CoglBool need_stencil;
  int samples_per_pixel;
  CoglBool swap_throttled;
  CoglBool depth_texture_enabled;
} CoglFramebufferConfig;

/* Flags to pass to _cogl_offscreen_new_with_texture_full */
typedef enum
{
  COGL_OFFSCREEN_DISABLE_DEPTH_AND_STENCIL = 1
} CoglOffscreenFlags;

/* XXX: The order of these indices determines the order they are
 * flushed.
 *
 * Flushing clip state may trash the modelview and projection matrices
 * so we must do it before flushing the matrices.
 */
typedef enum _CoglFramebufferStateIndex
{
  COGL_FRAMEBUFFER_STATE_INDEX_BIND               = 0,
  COGL_FRAMEBUFFER_STATE_INDEX_VIEWPORT           = 1,
  COGL_FRAMEBUFFER_STATE_INDEX_CLIP               = 2,
  COGL_FRAMEBUFFER_STATE_INDEX_DITHER             = 3,
  COGL_FRAMEBUFFER_STATE_INDEX_MODELVIEW          = 4,
  COGL_FRAMEBUFFER_STATE_INDEX_PROJECTION         = 5,
  COGL_FRAMEBUFFER_STATE_INDEX_COLOR_MASK         = 6,
  COGL_FRAMEBUFFER_STATE_INDEX_FRONT_FACE_WINDING = 7,
  COGL_FRAMEBUFFER_STATE_INDEX_DEPTH_WRITE        = 8,
  COGL_FRAMEBUFFER_STATE_INDEX_MAX                = 9
} CoglFramebufferStateIndex;

typedef enum _CoglFramebufferState
{
  COGL_FRAMEBUFFER_STATE_BIND               = 1<<0,
  COGL_FRAMEBUFFER_STATE_VIEWPORT           = 1<<1,
  COGL_FRAMEBUFFER_STATE_CLIP               = 1<<2,
  COGL_FRAMEBUFFER_STATE_DITHER             = 1<<3,
  COGL_FRAMEBUFFER_STATE_MODELVIEW          = 1<<4,
  COGL_FRAMEBUFFER_STATE_PROJECTION         = 1<<5,
  COGL_FRAMEBUFFER_STATE_COLOR_MASK         = 1<<6,
  COGL_FRAMEBUFFER_STATE_FRONT_FACE_WINDING = 1<<7,
  COGL_FRAMEBUFFER_STATE_DEPTH_WRITE        = 1<<8
} CoglFramebufferState;

#define COGL_FRAMEBUFFER_STATE_ALL ((1<<COGL_FRAMEBUFFER_STATE_INDEX_MAX) - 1)

/* Private flags that can internally be added to CoglReadPixelsFlags */
typedef enum
{
  /* If this is set then the data will not be flipped to compensate
     for GL's upside-down coordinate system but instead will be left
     in whatever order GL gives us (which will depend on whether the
     framebuffer is offscreen or not) */
  COGL_READ_PIXELS_NO_FLIP = 1L << 30
} CoglPrivateReadPixelsFlags;

typedef struct
{
  int red;
  int blue;
  int green;
  int alpha;
  int depth;
  int stencil;
} CoglFramebufferBits;

struct _CoglFramebuffer
{
  CoglObject          _parent;
  CoglContext        *context;
  CoglFramebufferType  type;

  /* The user configuration before allocation... */
  CoglFramebufferConfig config;

  int                 width;
  int                 height;
  /* Format of the pixels in the framebuffer (including the expected
     premult state) */
  CoglPixelFormat     internal_format;
  CoglBool            allocated;

  CoglMatrixStack    *modelview_stack;
  CoglMatrixStack    *projection_stack;
  float               viewport_x;
  float               viewport_y;
  float               viewport_width;
  float               viewport_height;
  int                 viewport_age;
  int                 viewport_age_for_scissor_workaround;

  CoglClipStack      *clip_stack;

  CoglBool            dither_enabled;
  CoglBool            depth_writing_enabled;
  CoglColorMask       color_mask;

  /* We journal the textured rectangles we want to submit to OpenGL so
   * we have an oppertunity to batch them together into less draw
   * calls. */
  CoglJournal        *journal;

  /* The scene of a given framebuffer may depend on images in other
   * framebuffers... */
  UList              *deps;

  /* As part of an optimization for reading-back single pixels from a
   * framebuffer in some simple cases where the geometry is still
   * available in the journal we need to track the bounds of the last
   * region cleared, its color and we need to track when something
   * does in fact draw to that region so it is no longer clear.
   */
  float               clear_color_red;
  float               clear_color_green;
  float               clear_color_blue;
  float               clear_color_alpha;
  int                 clear_clip_x0;
  int                 clear_clip_y0;
  int                 clear_clip_x1;
  int                 clear_clip_y1;
  CoglBool            clear_clip_dirty;

  /* Whether something has been drawn to the buffer since the last
   * swap buffers or swap region. */
  CoglBool            mid_scene;

  /* driver specific */
  CoglBool            dirty_bitmasks;
  CoglFramebufferBits bits;

  int                 samples_per_pixel;
};

typedef enum {
  COGL_OFFSCREEN_ALLOCATE_FLAG_DEPTH_STENCIL    = 1L<<0,
  COGL_OFFSCREEN_ALLOCATE_FLAG_DEPTH            = 1L<<1,
  COGL_OFFSCREEN_ALLOCATE_FLAG_STENCIL          = 1L<<2
} CoglOffscreenAllocateFlags;

typedef struct _CoglGLFramebuffer
{
  GLuint fbo_handle;
  UList *renderbuffers;
  int samples_per_pixel;
} CoglGLFramebuffer;

struct _CoglOffscreen
{
  CoglFramebuffer  _parent;

  CoglGLFramebuffer gl_framebuffer;

  CoglTexture    *texture;
  int             texture_level;

  CoglTexture *depth_texture;

  CoglOffscreenAllocateFlags allocation_flags;

  /* FIXME: _cogl_offscreen_new_with_texture_full should be made to use
   * fb->config to configure if we want a depth or stencil buffer so
   * we can get rid of these flags */
  CoglOffscreenFlags create_flags;
};

void
_cogl_framebuffer_init (CoglFramebuffer *framebuffer,
                        CoglContext *ctx,
                        CoglFramebufferType type,
                        int width,
                        int height);

/* XXX: For a public api we might instead want a way to explicitly
 * set the _premult status of a framebuffer or what components we
 * care about instead of exposing the CoglPixelFormat
 * internal_format.
 *
 * The current use case for this api is where we create an offscreen
 * framebuffer for a shared atlas texture that has a format of
 * RGBA_8888 disregarding the premultiplied alpha status for
 * individual atlased textures or whether the alpha component is being
 * discarded. We want to overried the internal_format that will be
 * derived from the texture.
 */
void
_cogl_framebuffer_set_internal_format (CoglFramebuffer *framebuffer,
                                       CoglPixelFormat internal_format);

void _cogl_framebuffer_free (CoglFramebuffer *framebuffer);

const CoglWinsysVtable *
_cogl_framebuffer_get_winsys (CoglFramebuffer *framebuffer);

void
_cogl_framebuffer_clear_without_flush4f (CoglFramebuffer *framebuffer,
                                         CoglBufferBit buffers,
                                         float red,
                                         float green,
                                         float blue,
                                         float alpha);

void
_cogl_framebuffer_mark_clear_clip_dirty (CoglFramebuffer *framebuffer);

void
_cogl_framebuffer_mark_mid_scene (CoglFramebuffer *framebuffer);

/*
 * _cogl_framebuffer_get_clip_stack:
 * @framebuffer: A #CoglFramebuffer
 *
 * Gets a pointer to the current clip stack. This can be used to later
 * return to the same clip stack state with
 * _cogl_framebuffer_set_clip_stack(). A reference is not taken on the
 * stack so if you want to keep it you should call
 * _cogl_clip_stack_ref().
 *
 * Return value: a pointer to the @framebuffer clip stack.
 */
CoglClipStack *
_cogl_framebuffer_get_clip_stack (CoglFramebuffer *framebuffer);

/*
 * _cogl_framebuffer_set_clip_stack:
 * @framebuffer: A #CoglFramebuffer
 * @stack: a pointer to the replacement clip stack
 *
 * Replaces the @framebuffer clip stack with @stack.
 */
void
_cogl_framebuffer_set_clip_stack (CoglFramebuffer *framebuffer,
                                  CoglClipStack *stack);

CoglMatrixStack *
_cogl_framebuffer_get_modelview_stack (CoglFramebuffer *framebuffer);

CoglMatrixStack *
_cogl_framebuffer_get_projection_stack (CoglFramebuffer *framebuffer);

void
_cogl_framebuffer_add_dependency (CoglFramebuffer *framebuffer,
                                  CoglFramebuffer *dependency);

void
_cogl_framebuffer_remove_all_dependencies (CoglFramebuffer *framebuffer);

void
_cogl_framebuffer_flush_journal (CoglFramebuffer *framebuffer);

void
_cogl_framebuffer_flush_dependency_journals (CoglFramebuffer *framebuffer);

void
_cogl_framebuffer_flush_state (CoglFramebuffer *draw_buffer,
                               CoglFramebuffer *read_buffer,
                               CoglFramebufferState state);

/*
 * _cogl_offscreen_new_with_texture_full:
 * @texture: A #CoglTexture pointer
 * @create_flags: Flags specifying how to create the FBO
 * @level: The mipmap level within the texture to target
 *
 * Creates a new offscreen buffer which will target the given
 * texture. By default the buffer will have a depth and stencil
 * buffer. This can be disabled by passing
 * %COGL_OFFSCREEN_DISABLE_DEPTH_AND_STENCIL in @create_flags.
 *
 * Return value: the new CoglOffscreen object.
 */
CoglOffscreen *
_cogl_offscreen_new_with_texture_full (CoglTexture *texture,
                                       CoglOffscreenFlags create_flags,
                                       int level);

/*
 * _cogl_push_framebuffers:
 * @draw_buffer: A pointer to the buffer used for drawing
 * @read_buffer: A pointer to the buffer used for reading back pixels
 *
 * Redirects drawing and reading to the specified framebuffers as in
 * cogl_push_framebuffer() except that it allows the draw and read
 * buffer to be different. The buffers are pushed as a pair so that
 * they can later both be restored with a single call to
 * cogl_pop_framebuffer().
 */
void
_cogl_push_framebuffers (CoglFramebuffer *draw_buffer,
                         CoglFramebuffer *read_buffer);

/*
 * _cogl_blit_framebuffer:
 * @src: The source #CoglFramebuffer
 * @dest: The destination #CoglFramebuffer
 * @src_x: Source x position
 * @src_y: Source y position
 * @dst_x: Destination x position
 * @dst_y: Destination y position
 * @width: Width of region to copy
 * @height: Height of region to copy
 *
 * This blits a region of the color buffer of the current draw buffer
 * to the current read buffer. The draw and read buffers can be set up
 * using _cogl_push_framebuffers(). This function should only be
 * called if the COGL_PRIVATE_FEATURE_OFFSCREEN_BLIT feature is
 * advertised. The two buffers must both be offscreen and have the
 * same format.
 *
 * Note that this function differs a lot from the glBlitFramebuffer
 * function provided by the GL_EXT_framebuffer_blit extension. Notably
 * it doesn't support having different sizes for the source and
 * destination rectangle. This isn't supported by the corresponding
 * GL_ANGLE_framebuffer_blit extension on GLES2.0 and it doesn't seem
 * like a particularly useful feature. If the application wanted to
 * scale the results it may make more sense to draw a primitive
 * instead.
 *
 * We can only really support blitting between two offscreen buffers
 * for this function on GLES2.0. This is because we effectively render
 * upside down to offscreen buffers to maintain Cogl's representation
 * of the texture coordinate system where 0,0 is the top left of the
 * texture. If we were to blit from an offscreen to an onscreen buffer
 * then we would need to mirror the blit along the x-axis but the GLES
 * extension does not support this.
 *
 * The GL function is documented to be affected by the scissor. This
 * function therefore ensure that an empty clip stack is flushed
 * before performing the blit which means the scissor is effectively
 * ignored.
 *
 * The function also doesn't support specifying the buffers to copy
 * and instead only the color buffer is copied. When copying the depth
 * or stencil buffers the extension on GLES2.0 only supports copying
 * the full buffer which would be awkward to document with this
 * API. If we wanted to support that feature it may be better to have
 * a separate function to copy the entire buffer for a given mask.
 */
void
_cogl_blit_framebuffer (CoglFramebuffer *src,
                        CoglFramebuffer *dest,
                        int src_x,
                        int src_y,
                        int dst_x,
                        int dst_y,
                        int width,
                        int height);

void
_cogl_framebuffer_push_projection (CoglFramebuffer *framebuffer);

void
_cogl_framebuffer_pop_projection (CoglFramebuffer *framebuffer);

void
_cogl_framebuffer_save_clip_stack (CoglFramebuffer *framebuffer);

void
_cogl_framebuffer_restore_clip_stack (CoglFramebuffer *framebuffer);

void
_cogl_framebuffer_unref (CoglFramebuffer *framebuffer);

/* This can be called directly by the CoglJournal to draw attributes
 * skipping the implicit journal flush, the framebuffer flush and
 * pipeline validation. */
void
_cogl_framebuffer_draw_attributes (CoglFramebuffer *framebuffer,
                                   CoglPipeline *pipeline,
                                   CoglVerticesMode mode,
                                   int first_vertex,
                                   int n_vertices,
                                   CoglAttribute **attributes,
                                   int n_attributes,
                                   CoglDrawFlags flags);

void
_cogl_framebuffer_draw_indexed_attributes (CoglFramebuffer *framebuffer,
                                           CoglPipeline *pipeline,
                                           CoglVerticesMode mode,
                                           int first_vertex,
                                           int n_vertices,
                                           CoglIndices *indices,
                                           CoglAttribute **attributes,
                                           int n_attributes,
                                           CoglDrawFlags flags);

gboolean
_cogl_framebuffer_try_creating_gl_fbo (CoglContext *ctx,
                                       CoglTexture *texture,
                                       int texture_level,
                                       int texture_level_width,
                                       int texture_level_height,
                                       CoglTexture *depth_texture,
                                       CoglFramebufferConfig *config,
                                       CoglOffscreenAllocateFlags flags,
                                       CoglGLFramebuffer *gl_framebuffer);

unsigned long
_cogl_framebuffer_compare (CoglFramebuffer *a,
                           CoglFramebuffer *b,
                           unsigned long state);

static inline CoglMatrixEntry *
_cogl_framebuffer_get_modelview_entry (CoglFramebuffer *framebuffer)
{
  CoglMatrixStack *modelview_stack =
    _cogl_framebuffer_get_modelview_stack (framebuffer);
  return modelview_stack->last_entry;
}

static inline CoglMatrixEntry *
_cogl_framebuffer_get_projection_entry (CoglFramebuffer *framebuffer)
{
  CoglMatrixStack *projection_stack =
    _cogl_framebuffer_get_projection_stack (framebuffer);
  return projection_stack->last_entry;
}

/*
 * _cogl_framebuffer_flush:
 * @framebuffer: A #CoglFramebuffer
 *
 * This function should only need to be called in exceptional circumstances.
 *
 * As an optimization Cogl drawing functions may batch up primitives
 * internally, so if you are trying to use native drawing apis
 * directly to a Cogl onscreen framebuffer or raw OpenGL outside of
 * Cogl you stand a better chance of being successful if you ask Cogl
 * to flush any batched drawing before issuing your own drawing
 * commands.
 *
 * This api only ensure that the underlying driver is issued all the
 * commands necessary to draw the batched primitives. It provides no
 * guarantees about when the driver will complete the rendering.
 *
 * This provides no guarantees about native graphics state or OpenGL
 * state upon returning and to avoid confusing Cogl you should aim to
 * save and restore any changes you make before resuming use of Cogl.
 *
 * Note: If you make OpenGL state changes with the intention of
 * affecting Cogl drawing primitives you stand a high chance of
 * conflicting with Cogl internals which can change so this is not
 * supported.
 *
 * XXX: We considered making this api public, but actually for the
 * direct use of OpenGL usecase this api isn't really enough since
 * it would leave GL in a completely undefined state which would
 * basically make it inpractical for applications to use. To really
 * support mixing raw GL with Cogl we'd probabably need to guarantee
 * that we reset all GL state to defaults.
 */
void
_cogl_framebuffer_flush (CoglFramebuffer *framebuffer);

/*
 * _cogl_framebuffer_get_stencil_bits:
 * @framebuffer: a pointer to a #CoglFramebuffer
 *
 * Retrieves the number of stencil bits of @framebuffer
 *
 * Return value: the number of bits
 *
 * Since: 2.0
 * Stability: unstable
 */
int
_cogl_framebuffer_get_stencil_bits (CoglFramebuffer *framebuffer);

#endif /* __COGL_FRAMEBUFFER_PRIVATE_H */
