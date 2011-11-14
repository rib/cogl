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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-util.h"
#include "cogl-framebuffer-private.h"
#include "cogl-virtual-framebuffer-private.h"
#include "cogl-virtual-framebuffer.h"
#include "cogl-context-private.h"
#include "cogl-object-private.h"

#include <glib.h>

static void
_cogl_virtual_framebuffer_free (CoglVirtualFramebuffer *virtual_framebuffer);

COGL_OBJECT_DEFINE (VirtualFramebuffer, virtual_framebuffer);

CoglVirtualFramebuffer *
cogl_virtual_framebuffer_new (CoglContext *context,
                              int width,
                              int height)
{
  CoglVirtualFramebuffer *virtual_framebuffer =
    g_new0 (CoglVirtualFramebuffer, 1);

  _cogl_framebuffer_init (COGL_FRAMEBUFFER (virtual_framebuffer),
                          context,
                          COGL_FRAMEBUFFER_TYPE_VIRTUAL,
                          COGL_PIXEL_FORMAT_RGBA_8888, /* arbitrary */
                          width,
                          height);

  COGL_FRAMEBUFFER (virtual_framebuffer)->allocated = TRUE;

  return _cogl_virtual_framebuffer_object_new (virtual_framebuffer);
}

void
cogl_virtual_framebuffer_add_slice (CoglVirtualFramebuffer *virtual_framebuffer,
                                    CoglFramebuffer *slice,
                                    int sample_x,
                                    int sample_y,
                                    int sample_width,
                                    int sample_height,
                                    int virtual_x,
                                    int virtual_y)
{
  CoglFramebufferSlice *new_slice = g_new0 (CoglFramebufferSlice, 1);

  new_slice->framebuffer = cogl_object_ref (slice);
  new_slice->sample_region[0] = sample_x;
  new_slice->sample_region[1] = sample_y;
  if (sample_width == -1)
    sample_width = cogl_framebuffer_get_width (slice);
  new_slice->sample_region[2] = sample_width;
  if (sample_height == -1)
    sample_height = cogl_framebuffer_get_height (slice);
  new_slice->sample_region[3] = sample_height;
  new_slice->virtual_x = virtual_x;
  new_slice->virtual_y = virtual_y;

  if (virtual_framebuffer->slices == NULL)
    {
      CoglFramebuffer *fb = COGL_FRAMEBUFFER (virtual_framebuffer);
      fb->format = cogl_framebuffer_get_color_format (fb);
    }

  virtual_framebuffer->slices =
    g_list_prepend (virtual_framebuffer->slices, new_slice);
}

static void
_cogl_framebuffer_slice_free (CoglFramebufferSlice *slice)
{
  cogl_object_unref (slice->framebuffer);
  g_free (slice);
}

static void
_cogl_virtual_framebuffer_free (CoglVirtualFramebuffer *virtual_framebuffer)
{
  CoglFramebuffer *framebuffer = COGL_FRAMEBUFFER (virtual_framebuffer);
  GList *l;

  for (l = virtual_framebuffer->slices; l; l = l->next)
    _cogl_framebuffer_slice_free (l->data);

  g_list_free (virtual_framebuffer->slices);

  /* Chain up to parent */
  _cogl_framebuffer_free (framebuffer);

  g_free (virtual_framebuffer);
}

static CoglFramebufferSlice *
lookup_slice (CoglVirtualFramebuffer *virtual_framebuffer,
              CoglFramebuffer *slice)
{
  GList *l;

  for (l = virtual_framebuffer->slices; l; l = l->next)
    {
      CoglFramebufferSlice *current_slice = l->data;
      if (current_slice->framebuffer == slice)
        return current_slice;
    }

  return NULL;
}

void
cogl_virtual_framebuffer_remove_slice (
                                    CoglVirtualFramebuffer *virtual_framebuffer,
                                    CoglFramebuffer *slice)
{
  GList *l;

  for (l = virtual_framebuffer->slices; l; l = l->next)
    {
      CoglFramebufferSlice *current_slice = l->data;
      if (current_slice->framebuffer == slice)
        {
          _cogl_framebuffer_slice_free (current_slice);
          virtual_framebuffer->slices =
            g_list_remove_list (virtual_framebuffer->slices, l);
          return;
        }
    }

  g_warn_if_reached ();
}

void
cogl_virtual_framebuffer_move_slice (
                                    CoglVirtualFramebuffer *virtual_framebuffer,
                                    CoglFramebuffer *slice,
                                    int virtual_x,
                                    int virtual_y)
{
  CoglFramebufferSlice *match = lookup_slice (virtual_framebuffer, slice);

  _COGL_RETURN_IF_FAIL (match);

  /* XXX: do we need to flush anything here? */

  match->virtual_x = virtual_x;
  match->virtual_y = virtual_y;
}

void
cogl_virtual_framebuffer_set_slice_sample_region (
                                    CoglVirtualFramebuffer *virtual_framebuffer,
                                    CoglFramebuffer *slice,
                                    int x,
                                    int y,
                                    int width,
                                    int height)
{
  CoglFramebufferSlice *match = lookup_slice (virtual_framebuffer, slice);

  _COGL_RETURN_IF_FAIL (match);

  /* XXX: do we need to flush anything here? */

  match->sample_region[0] = x;
  match->sample_region[1] = y;

  if (width == -1)
    width = cogl_framebuffer_get_width (slice);
  match->sample_region[2] = width;
  if (height == -1)
    height = cogl_framebuffer_get_height (slice);
  match->sample_region[3] = height;
}

int
cogl_virtual_framebuffer_get_slice_sample_x (
                                    CoglVirtualFramebuffer *virtual_framebuffer,
                                    CoglFramebuffer *slice)
{

  CoglFramebufferSlice *match = lookup_slice (virtual_framebuffer, slice);

  _COGL_RETURN_VAL_IF_FAIL (match, 0);

  /* XXX: do we need to flush anything here? */

  return match->sample_region[0];
}

int
cogl_virtual_framebuffer_get_slice_sample_y (
                                    CoglVirtualFramebuffer *virtual_framebuffer,
                                    CoglFramebuffer *slice)
{
  CoglFramebufferSlice *match = lookup_slice (virtual_framebuffer, slice);

  _COGL_RETURN_VAL_IF_FAIL (match, 0);

  /* XXX: do we need to flush anything here? */

  return match->sample_region[1];
}

int
cogl_virtual_framebuffer_get_slice_sample_width (
                                    CoglVirtualFramebuffer *virtual_framebuffer,
                                    CoglFramebuffer *slice)
{
  CoglFramebufferSlice *match = lookup_slice (virtual_framebuffer, slice);

  _COGL_RETURN_VAL_IF_FAIL (match, 0);

  /* XXX: do we need to flush anything here? */

  return match->sample_region[2];
}

int
cogl_virtual_framebuffer_get_slice_sample_height (
                                    CoglVirtualFramebuffer *virtual_framebuffer,
                                    CoglFramebuffer *slice)
{
  CoglFramebufferSlice *match = lookup_slice (virtual_framebuffer, slice);

  _COGL_RETURN_VAL_IF_FAIL (match, 0);

  /* XXX: do we need to flush anything here? */

  return match->sample_region[3];
}

void
cogl_virtual_framebuffer_set_size (CoglVirtualFramebuffer *virtual_framebuffer,
                                   int width,
                                   int height)
{
  CoglFramebuffer *fb = COGL_FRAMEBUFFER (virtual_framebuffer);

  /* XXX: do we need to flush anything here? */

  fb->width = width;
  fb->height = height;
}

void
_cogl_virtual_framebuffer_foreach (CoglVirtualFramebuffer *virtual_framebuffer,
                                   CoglVirtialFramebufferCallback callback,
                                   void *user_data)
{
  GList *l;
  gboolean cont = TRUE;

  for (l = virtual_framebuffer->slices; l && cont; l = l->next)
    {
      CoglFramebufferSlice *slice = l->data;
      cont = callback (virtual_framebuffer,
                       slice->framebuffer,
                       slice->sample_region,
                       slice->virtual_x,
                       slice->virtual_y,
                       user_data);
    }
}
