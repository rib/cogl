/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2013 Intel Corporation.
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
 *
 */

#include <config.h>

#include <string.h>

#include "cogl-object-private.h"
#include "cogl-overlay-private.h"
#include "cogl-onscreen.h"

static void _cogl_overlay_free (CoglOverlay *overlay);

COGL_OBJECT_DEFINE (Overlay, overlay);

static void
free_overlay_state (CoglOverlayState *state)
{
  if (state->onscreen_source)
    cogl_object_unref (state->onscreen_source);

  g_slice_free (CoglOverlayState, state);
}

static void
ensure_pending_state (CoglOverlay *overlay)
{
  CoglOverlayState *state = overlay->state;
  CoglOverlayState *pending;

  if (state != overlay->pending)
    return;

  pending = g_slice_dup (CoglOverlayState, state);

  pending->onscreen_source = state->onscreen_source;
  if (pending->onscreen_source)
    cogl_object_ref (pending->onscreen_source);

  overlay->pending = pending;
}

static void
_cogl_overlay_free (CoglOverlay *overlay)
{
  if (overlay->pending != overlay->state)
    free_overlay_state (overlay->pending);

  free_overlay_state (overlay->state);

  g_slice_free (CoglOverlay, overlay);
}

CoglOverlay *
cogl_overlay_new (CoglOutput *output)
{
  CoglOverlay *overlay = g_slice_new0 (CoglOverlay);

  overlay->state = g_slice_new0 (CoglOverlayState);

  /* Note: to avoid a circular reference we don't reference
   * the output here. */
  overlay->state->output = output;

  overlay->pending = overlay->state;

  overlay = _cogl_overlay_object_new (overlay);

  cogl_output_append_overlay (output, overlay);

  return overlay;
}

void
_cogl_overlay_update_state (CoglOverlay *overlay)
{
  if (overlay->pending == overlay->state)
    return;

  free_overlay_state (overlay->state);

  overlay->state = overlay->pending;
}

void
cogl_overlay_set_onscreen_source (CoglOverlay *overlay,
                                  CoglOnscreen *onscreen_source)
{
  if (overlay->pending->onscreen_source == onscreen_source)
    return;

  ensure_pending_state (overlay);

  if (overlay->pending->onscreen_source)
    cogl_object_unref (overlay->pending->onscreen_source);

  overlay->pending->onscreen_source = cogl_object_ref (onscreen_source);
}

CoglOnscreen *
cogl_overlay_get_onscreen_source (CoglOverlay *overlay)
{
  return overlay->pending->onscreen_source;
}

int
cogl_overlay_get_source_x (CoglOverlay *overlay)
{
  return overlay->pending->src_x;
}

void
cogl_overlay_set_source_x (CoglOverlay *overlay, int src_x)
{
  if (overlay->pending->src_x == src_x)
    return;

  ensure_pending_state (overlay);

  overlay->pending->src_x = src_x;
}

int
cogl_overlay_get_source_y (CoglOverlay *overlay)
{
  return overlay->pending->src_y;
}

void
cogl_overlay_set_source_y (CoglOverlay *overlay, int src_y)
{
  if (overlay->pending->src_y == src_y)
    return;

  ensure_pending_state (overlay);

  overlay->pending->src_y = src_y;
}

int
cogl_overlay_get_source_width (CoglOverlay *overlay)
{
  return overlay->pending->src_width;
}

void
cogl_overlay_set_source_width (CoglOverlay *overlay, int src_width)
{
  if (overlay->pending->src_width == src_width)
    return;

  ensure_pending_state (overlay);

  overlay->pending->src_width = src_width;
}

int
cogl_overlay_get_source_height (CoglOverlay *overlay)
{
  return overlay->pending->src_height;
}

void
cogl_overlay_set_source_height (CoglOverlay *overlay, int src_height)
{
  if (overlay->pending->src_height == src_height)
    return;

  ensure_pending_state (overlay);

  overlay->pending->src_height = src_height;
}

int
cogl_overlay_get_x (CoglOverlay *overlay)
{
  return overlay->pending->dst_x;
}

void
cogl_overlay_set_x (CoglOverlay *overlay, int dst_x)
{
  if (overlay->pending->dst_x == dst_x)
    return;

  ensure_pending_state (overlay);

  overlay->pending->dst_x = dst_x;
}

int
cogl_overlay_get_y (CoglOverlay *overlay)
{
  return overlay->pending->dst_y;
}

void
cogl_overlay_set_y (CoglOverlay *overlay, int dst_y)
{
  if (overlay->pending->dst_y == dst_y)
    return;

  ensure_pending_state (overlay);

  overlay->pending->dst_y = dst_y;
}

CoglBool
_cogl_overlay_equal (CoglOverlay *overlay0,
                     CoglOverlay *overlay1)
{
  if (overlay0 == overlay1)
    return TRUE;

  if (memcmp ((const char *)overlay0->pending +
              G_STRUCT_OFFSET (CoglOverlayState, src_x),
              (const char *)overlay1->pending +
              G_STRUCT_OFFSET (CoglOverlayState, src_x),
              sizeof (CoglOverlayState) -
              G_STRUCT_OFFSET (CoglOverlayState, src_x)) != 0)
    return FALSE;

  if (overlay0->pending->onscreen_source !=
      overlay1->pending->onscreen_source)
    return FALSE;

  return TRUE;
}
