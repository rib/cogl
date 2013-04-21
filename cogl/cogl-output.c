/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2012 Red Hat, Inc.
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

#include <config.h>

#include "cogl-output-private.h"
#include "cogl-overlay-private.h"
#include "cogl-mode-private.h"

#include <string.h>

static void _cogl_output_free (CoglOutput *output);

COGL_OBJECT_DEFINE (Output, output);

CoglOutput *
_cogl_output_new (const char *name)
{
  CoglOutput *output;

  output = g_slice_new0 (CoglOutput);

  output->state = g_slice_new0 (CoglOutputState);
  output->state->name = g_strdup (name);

  output->pending = output->state;

  return _cogl_output_object_new (output);
}

static void
free_output_state (CoglOutputState *state)
{
  GList *l;

  for (l = state->overlays; l; l = l->next)
    cogl_object_unref (l->data);
  g_list_free (state->overlays);

  g_free (state->name);

  g_slice_free (CoglOutputState, state);
}

static void
_cogl_output_free (CoglOutput *output)
{
  if (output->winsys_destroy_callback)
    output->winsys_destroy_callback (output->winsys);

  if (output->pending != output->state)
    free_output_state (output->pending);

  free_output_state (output->state);

  g_slice_free (CoglOutput, output);
}

void
_cogl_output_set_winsys_data (CoglOutput *output,
                              void *winsys,
                              CoglUserDataDestroyCallback destroy_callback)
{
  output->winsys = winsys;
  output->winsys_destroy_callback = destroy_callback;
}

static CoglBool
_cogl_mode_equal (CoglMode *mode0, CoglMode *mode1)
{
  if (memcmp ((const char *)mode0 + G_STRUCT_OFFSET (CoglMode, width),
              (const char *)mode1 + G_STRUCT_OFFSET (CoglMode, width),
              sizeof (CoglMode) - G_STRUCT_OFFSET (CoglMode, width)) != 0)
    return FALSE;

  return TRUE;
}

static CoglBool
_cogl_mode_lists_equal (GList *modes0, GList *modes1)
{
  GList *l, *m;

  for (l = modes0, m = modes1;
       l && m;
       l = l->next, m = m->next)
    {
      if (!_cogl_mode_equal (l->data, m->data))
        return FALSE;
    }

  if (l || m)
    return FALSE;

  return TRUE;
}

CoglBool
_cogl_output_equal (CoglOutput *output,
                    CoglOutput *other)
{
  GList *l, *l2;

  if (output == other)
    return TRUE;

  if (!_cogl_mode_lists_equal (output->modes, other->modes))
    return FALSE;

  if (!_cogl_mode_equal (output->pending->mode, other->pending->mode))
    return FALSE;

  if (memcmp ((const char *)output->pending +
              G_STRUCT_OFFSET (CoglOutputState, x),
              (const char *)other->pending +
              G_STRUCT_OFFSET (CoglOutputState, x),
              sizeof (CoglOutputState) -
              G_STRUCT_OFFSET (CoglOutputState, x)) != 0)
    return FALSE;

  for (l = output->pending->overlays, l2 = other->pending->overlays;
       l && l2;
       l = l->next, l2 = l2->next)
    {
      if (!_cogl_overlay_equal (l->data, l2->data))
        return FALSE;
    }

  if (l || l2)
    return FALSE;

  return TRUE;
}

int
cogl_output_get_x (CoglOutput *output)
{
  return output->pending->x;
}

int
cogl_output_get_y (CoglOutput *output)
{
  return output->pending->y;
}

int
cogl_output_get_width (CoglOutput *output)
{
  return output->pending->mode->width;
}

int
cogl_output_get_height (CoglOutput *output)
{
  return output->pending->mode->height;
}

int
cogl_output_get_mm_width (CoglOutput *output)
{
  return output->pending->mm_width;
}

int
cogl_output_get_mm_height (CoglOutput *output)
{
  return output->pending->mm_height;
}

CoglSubpixelOrder
cogl_output_get_subpixel_order (CoglOutput *output)
{
  return output->pending->subpixel_order;
}

float
cogl_output_get_refresh_rate (CoglOutput *output)
{
  return output->pending->mode->refresh_rate;
}

CoglOverlay *
cogl_output_get_overlay0 (CoglOutput *output)
{
  return output->pending->overlays->data;
}

static void
ensure_pending_state (CoglOutput *output)
{
  CoglOutputState *state = output->state;
  CoglOutputState *pending;

  if (output->pending != state)
    return;

  pending = g_slice_dup (CoglOutputState, state);
  pending->name = g_strdup (state->name);
  pending->overlays = g_list_copy (state->overlays);

  output->pending = state;
}

void
_cogl_output_update_state (CoglOutput *output)
{
  if (output->pending == output->state)
    return;

  free_output_state (output->state);

  output->state = output->pending;
}

void
cogl_output_append_overlay (CoglOutput *output,
                            CoglOverlay *overlay)
{
  ensure_pending_state (output);

  output->pending->overlays =
    g_list_append (output->pending->overlays, overlay);
}

void
cogl_output_put_overlay_above (CoglOutput *output,
                               CoglOverlay *overlay,
                               CoglOverlay *sibling)
{
  GList *l;

  ensure_pending_state (output);

  if (sibling)
    {
      l = g_list_find (output->pending->overlays, sibling);

      _COGL_RETURN_IF_FAIL (l != NULL);

      l = l->next;
    }
  else
    l = NULL;

  output->pending->overlays =
    g_list_insert_before (output->pending->overlays,
                          l,
                          overlay);
}

void
cogl_output_put_overlay_below (CoglOutput *output,
                               CoglOverlay *overlay,
                               CoglOverlay *sibling)
{
  GList *l;

  ensure_pending_state (output);

  if (sibling)
    {
      l = g_list_find (output->pending->overlays, sibling);

      _COGL_RETURN_IF_FAIL (l != NULL);
    }
  else
    l = NULL;

  output->pending->overlays =
    g_list_insert_before (output->pending->overlays,
                          l,
                          overlay);
}

void
cogl_output_remove_overlay (CoglOutput *output,
                            CoglOverlay *overlay)
{
  GList *l;

  ensure_pending_state (output);

  l = g_list_find (output->pending->overlays, overlay);

  _COGL_RETURN_IF_FAIL (l != NULL);

  cogl_object_unref (l->data);

  output->pending->overlays =
    g_list_delete_link (output->pending->overlays, l);
}

void
cogl_output_foreach_mode (CoglOutput *output,
                          CoglOutputModeCallback callback,
                          void *user_data)
{
  GList *l;
  for (l = output->modes; l; l = l->next)
    callback (l->data, user_data);
}

void
cogl_output_set_mode (CoglOutput *output,
                      CoglMode *mode)
{
  _COGL_RETURN_IF_FAIL (mode);

  if (_cogl_mode_equal (output->pending->mode, mode))
    return;

  ensure_pending_state (output);

  if (output->pending->mode)
    cogl_object_unref (output->pending->mode);

  output->pending->mode = cogl_object_ref (mode);
}

CoglMode *
cogl_output_get_mode (CoglOutput *output)
{
  _COGL_RETURN_VAL_IF_FAIL (output->pending->mode, NULL);

  return output->pending->mode;
}

void
cogl_output_set_dpms_mode (CoglOutput *output,
                           CoglDpmsMode dpms_mode)
{
  if (output->pending->dpms_mode == dpms_mode)
    return;

  ensure_pending_state (output);

  output->pending->dpms_mode = dpms_mode;
}

CoglDpmsMode
cogl_output_get_dpms_mode (CoglOutput *output)
{
  return output->pending->dpms_mode;
}

void
cogl_output_foreach_overlay (CoglOutput *output,
                             CoglOutputOverlayCallback callback,
                             void *user_data)
{
  GList *l;
  for (l = output->pending->overlays; l; l = l->next)
    callback (l->data, user_data);
}
