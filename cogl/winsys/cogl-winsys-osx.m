/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2011 Intel Corporation.
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
 * Authors:
 *   Neil Roberts <neil@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-renderer-private.h"
#include "cogl-display-private.h"
#include "cogl-framebuffer-private.h"
#include "cogl-swap-chain-private.h"
#include "cogl-onscreen-template-private.h"
#include "cogl-context-private.h"
#include "cogl-onscreen-private.h"

#import <AppKit/AppKit.h>

/* might need this */
#if 0
@class NSOpenGLPixelFormat, NSOpenGLContext;
#endif

typedef struct _CoglRendererOSX
{
  int stub;
} CoglRendererOSX;

typedef struct _CoglDisplayOSX
{
  NSOpenGLPixelFormat *ns_pixel_format;
  NSOpenGLView *ns_dummy_view;
  NSWindow *ns_dummy_window;
  NSOpenGLContext *ns_context;
} CoglDisplayOSX;

typedef struct _CoglOnscreenOSX
{
  NSOpenGLPixelFormat *ns_pixel_format;
  NSOpenGLView *ns_view;
  NSWindow *ns_window;
} CoglOnscreenOSX;

#if 0
@interface CoglOSXOpenGLView : NSOpenGLView
{
  CoglOnscreen *onscreen;
}
- (id) initWithContext: (CoglContext *) context: (int) width: (int) height;
//- (void) drawRect: (NSRect) bounds;
@end
#endif

static CoglFuncPtr
_cogl_winsys_renderer_get_proc_address (CoglRenderer *renderer,
                                        const char *name)
{
  static GModule *module = NULL;

  /* this should find the right function if the program is linked against a
   * library providing it */
  if (G_UNLIKELY (module == NULL))
    module = g_module_open (NULL, 0);

  if (module)
    {
      void *symbol;

      if (g_module_symbol (module, name, &symbol))
        return symbol;
    }

  return NULL;
}

static void
_cogl_winsys_renderer_disconnect (CoglRenderer *renderer)
{
  g_slice_free (CoglRendererOSX, renderer->winsys);
}

static gboolean
_cogl_winsys_renderer_connect (CoglRenderer *renderer,
                               GError **error)
{
  NSAutoreleasePool *autorelease_pool = [[NSAutoreleasePool alloc] init];
  NSApplication *myApplication;

  /* XXX: Hack to trick gnustep into initializing its backend including the
   * GSDisplayServer which we require to access GLX. */
  if (!NSApp)
    myApplication = [NSApplication sharedApplication];

  [autorelease_pool release];

  if (renderer->driver != COGL_DRIVER_GL)
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_INIT,
                   "The OSX winsys only supports the GL driver");
      return FALSE;
    }

  renderer->winsys = g_slice_new0 (CoglRendererOSX);
  return TRUE;
}

static void
_cogl_winsys_display_destroy (CoglDisplay *display)
{
  CoglDisplayOSX *ns_display = display->winsys;

  _COGL_RETURN_IF_FAIL (ns_display != NULL);

  if (ns_display->ns_context)
    [ns_display->ns_context release];

  if (ns_display->ns_pixel_format)
    [ns_display->ns_pixel_format release];

  g_slice_free (CoglDisplayOSX, display->winsys);
  display->winsys = NULL;
}

#define MAX_PIXEL_FORMAT_ATTRIBS 30

static NSOpenGLPixelFormat *
ns_pixel_format_from_framebuffer_config (CoglFramebufferConfig *config)
{
  NSOpenGLPixelFormatAttribute attrs[MAX_PIXEL_FORMAT_ATTRIBS];
  int i = 0;

  attrs[i++] = NSOpenGLPFAMinimumPolicy;

  if (config->swap_chain->length >= 1)
    attrs[i++] = NSOpenGLPFADoubleBuffer;

  if (config->need_stencil)
    {
      attrs[i++] = NSOpenGLPFAStencilSize;
      attrs[i++] = 8;
    }

  if (config->swap_chain->has_alpha)
    {
      attrs[i++] = NSOpenGLPFAAlphaSize;
      attrs[i++] = 1;
    }

  attrs[i++] = NSOpenGLPFADepthSize;
  attrs[i++] = 24;

  if (config->samples_per_pixel)
    {
      attrs[i++] = NSOpenGLPFAMultisample;
      attrs[i++] = NSOpenGLPFASampleBuffers;
      attrs[i++] = 1;
      attrs[i++] = NSOpenGLPFASamples;
      attrs[i++] = config->samples_per_pixel;
    }

  attrs[i++] = 0;

  g_assert (i < MAX_PIXEL_FORMAT_ATTRIBS);

  return [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
}

static gboolean
create_context (CoglDisplay *display,
                GError **error)
{
  CoglDisplayOSX *ns_display = display->winsys;
  CoglFramebufferConfig *config;
#ifndef USING_GNUSTEP
  const long sw = 1;
#endif
  gboolean status = TRUE;
  NSRect rect;

  NSAutoreleasePool *autorelease_pool = [[NSAutoreleasePool alloc] init];

  config = &display->onscreen_template->config;
  ns_display->ns_pixel_format =
    ns_pixel_format_from_framebuffer_config (config);
  if (!ns_display->ns_pixel_format)
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_CONTEXT,
                   "Unable to find suitable GL pixel format");
      status = FALSE;
      goto done;
    }

  config = &display->onscreen_template->config;
  ns_display->ns_context =
    [[NSOpenGLContext alloc] initWithFormat: ns_display->ns_pixel_format
                               shareContext: nil];
  if (!ns_display->ns_context)
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_CONTEXT,
                   "Unable to create suitable GL context");
      status = FALSE;
      goto done;
    }

#ifndef USING_GNUSTEP
  /* Enable vblank sync - http://developer.apple.com/qa/qa2007/qa1521.html */
  [ns_display->ns_context setValues:&sw forParameter: NSOpenGLCPSwapInterval];
#endif

  rect = NSMakeRect (0, 0, 1, 1);

  ns_display->ns_dummy_view = [[NSOpenGLView alloc]
                initWithFrame: rect
                  pixelFormat: ns_display->ns_pixel_format];
  if (!ns_display->ns_dummy_view)
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_ONSCREEN,
                   "Unable to create suitable NSView");
      status = FALSE;
      goto done;
    }

  ns_display->ns_dummy_window = [[NSWindow alloc]
            initWithContentRect: [ns_display->ns_dummy_view frame]
                      styleMask: NSTitledWindowMask | NSClosableWindowMask | NSResizableWindowMask
                        backing: NSBackingStoreBuffered
                          defer: NO];
  if (!ns_display->ns_dummy_window)
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_ONSCREEN,
                   "Unable to create suitable NSWindow");
      status = FALSE;
      goto done;
    }
  [ns_display->ns_dummy_window setContentView:ns_display->ns_dummy_view];
  [ns_display->ns_dummy_window useOptimizedDrawing:YES];

  [ns_display->ns_dummy_view setOpenGLContext:ns_display->ns_context];

  [ns_display->ns_context setView: ns_display->ns_dummy_view];
  [ns_display->ns_context makeCurrentContext];

done:
  [autorelease_pool release];
  return status;
}

static gboolean
_cogl_winsys_display_setup (CoglDisplay *display,
                            GError **error)
{
  CoglDisplayOSX *ns_display;

  _COGL_RETURN_VAL_IF_FAIL (display->winsys == NULL, FALSE);

  ns_display = g_slice_new0 (CoglDisplayOSX);
  display->winsys = ns_display;

  if (!create_context (display, error))
    goto error;

  return TRUE;
error:
  _cogl_winsys_display_destroy (display);
  return FALSE;
}

static gboolean
_cogl_winsys_context_init (CoglContext *context, GError **error)
{
  return _cogl_context_update_features (context, error);
}

static void
_cogl_winsys_context_deinit (CoglContext *context)
{
}

static void
_cogl_winsys_onscreen_bind (CoglOnscreen *onscreen)
{
  CoglContext *context = COGL_FRAMEBUFFER (onscreen)->context;
  CoglDisplay *display = context->display;
  CoglDisplayOSX *ns_display = display->winsys;
  CoglOnscreenOSX *ns_onscreen = onscreen->winsys;

  NSAutoreleasePool *autorelease_pool = [[NSAutoreleasePool alloc] init];

  [ns_display->ns_context setView: ns_onscreen->ns_view];

  [autorelease_pool release];
}

static void
_cogl_winsys_onscreen_deinit (CoglOnscreen *onscreen)
{
}

static gboolean
_cogl_winsys_onscreen_init (CoglOnscreen *onscreen,
                            GError **error)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (onscreen);
  CoglContext *context = framebuffer->context;
  CoglDisplay *display = context->display;
  CoglDisplayOSX *ns_display = display->winsys;
  CoglOnscreenOSX *ns_onscreen;
  int width, height;
  NSRect rect;
  gboolean status = TRUE;

  NSAutoreleasePool *autorelease_pool = [[NSAutoreleasePool alloc] init];

  ns_onscreen = g_slice_new0 (CoglOnscreenOSX);
  onscreen->winsys = ns_onscreen;

  ns_onscreen->ns_pixel_format =
    ns_pixel_format_from_framebuffer_config (&framebuffer->config);
  if (!ns_onscreen->ns_pixel_format)
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_ONSCREEN,
                   "Unable to create suitable NSOpenGLPixelFormat");
      status = FALSE;
      goto done;
    }

  width = cogl_framebuffer_get_width (framebuffer);
  height = cogl_framebuffer_get_height (framebuffer);

  rect = NSMakeRect (0, 0, width, height);

  ns_onscreen->ns_view = [[NSOpenGLView alloc]
                initWithFrame: rect
                  pixelFormat: ns_onscreen->ns_pixel_format];
  if (!ns_onscreen->ns_view)
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_ONSCREEN,
                   "Unable to create suitable NSView");
      status = FALSE;
      goto done;
    }
  [ns_onscreen->ns_view setOpenGLContext:ns_display->ns_context];

  ns_onscreen->ns_window = [[NSWindow alloc]
            initWithContentRect: [ns_onscreen->ns_view frame]
                      styleMask: NSTitledWindowMask | NSClosableWindowMask | NSResizableWindowMask
                        backing: NSBackingStoreBuffered
                          defer: NO];
  if (!ns_onscreen->ns_window)
    {
      g_set_error (error, COGL_WINSYS_ERROR,
                   COGL_WINSYS_ERROR_CREATE_ONSCREEN,
                   "Unable to create suitable NSWindow");
      status = FALSE;
      goto done;
    }

  [ns_onscreen->ns_window setContentView:ns_onscreen->ns_view];
  [ns_onscreen->ns_window useOptimizedDrawing:YES];

  //[ns_onscreen->ns_view setOpenGLContext:ns_display->ns_context];
#if 0

  [ns_display->ns_context setView: ns_onscreen->ns_view];
  [ns_display->ns_context makeCurrentContext];
#endif

done:
  [autorelease_pool release];
  return status;
}

static void
_cogl_winsys_onscreen_swap_buffers (CoglOnscreen *onscreen)
{
  CoglContext *context = COGL_FRAMEBUFFER (onscreen)->context;
  CoglDisplay *display = context->display;
  CoglDisplayOSX *ns_display = display->winsys;

  [ns_display->ns_context flushBuffer];
}

static void
_cogl_winsys_onscreen_update_swap_throttled (CoglOnscreen *onscreen)
{
  /* OSX doesn't appear to provide a way to set this per window */
}

static void
_cogl_winsys_onscreen_set_visibility (CoglOnscreen *onscreen,
                                      gboolean visibility)
{
  CoglContext *context = COGL_FRAMEBUFFER (onscreen)->context;
  CoglDisplay *display = context->display;
  CoglDisplayOSX *ns_display = display->winsys;
  CoglOnscreenOSX *ns_onscreen = onscreen->winsys;

  NSAutoreleasePool *autorelease_pool = [[NSAutoreleasePool alloc] init];
  if (visibility)
    [ns_onscreen->ns_window makeKeyAndOrderFront: nil];
  else
    [ns_onscreen->ns_window orderOut: nil];

  /* XXX: HACK */
  [ns_display->ns_context setView: ns_onscreen->ns_view];
  [ns_display->ns_context makeCurrentContext];

  [autorelease_pool release];
}

const CoglWinsysVtable *
_cogl_winsys_osx_get_vtable (void)
{
  static gboolean vtable_inited = FALSE;
  static CoglWinsysVtable vtable;

  if (!vtable_inited)
    {
      memset (&vtable, 0, sizeof (vtable));

      vtable.id = COGL_WINSYS_ID_SDL;
      vtable.name = "OSX";
      vtable.renderer_get_proc_address = _cogl_winsys_renderer_get_proc_address;
      vtable.renderer_connect = _cogl_winsys_renderer_connect;
      vtable.renderer_disconnect = _cogl_winsys_renderer_disconnect;
      vtable.display_setup = _cogl_winsys_display_setup;
      vtable.display_destroy = _cogl_winsys_display_destroy;
      vtable.context_init = _cogl_winsys_context_init;
      vtable.context_deinit = _cogl_winsys_context_deinit;
      vtable.onscreen_init = _cogl_winsys_onscreen_init;
      vtable.onscreen_deinit = _cogl_winsys_onscreen_deinit;
      vtable.onscreen_bind = _cogl_winsys_onscreen_bind;
      vtable.onscreen_swap_buffers = _cogl_winsys_onscreen_swap_buffers;
      vtable.onscreen_update_swap_throttled =
        _cogl_winsys_onscreen_update_swap_throttled;
      vtable.onscreen_set_visibility = _cogl_winsys_onscreen_set_visibility;

      vtable_inited = TRUE;
    }

  return &vtable;
}
