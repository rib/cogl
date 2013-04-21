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

#ifndef _COGL_OVERLAY_H_
#define _COGL_OVERLAY_H_

/* We forward declare the CoglOverlay type here to avoid some
 * circular dependency issues with the following headers.
 */
typedef struct _CoglOverlay CoglOverlay;

#include <cogl/cogl-onscreen.h>
#include <cogl/cogl-output.h>

COGL_BEGIN_DECLS

/**
 * SECTION:cogl-overlay
 * @short_description: A single overlay to composite on a #CoglOutput
 *
 * A #CoglOverlay represents a single image source to composite on a
 * given #CoglOutput. Depending on the capabilities of the output an
 * overlay can - for example - be offset and scaled within that
 * output.
 *
 * If an output can be composited with overlyas using dedicated,
 * fixed-function hardware then it can be more power effecient than
 * compositing with the more general purpose GPU pipeline.
 */

CoglBool cogl_is_overlay (void *object);

/**
 * cogl_overlay_new:
 * @output: A #CoglOutput pointer
 *
 * Creates a new #CoglOverlay that is associated with the given @output,
 * positioned on top of any existing overlays.
 *
 * Return value: A newly allocated #CoglOverlay.
 *
 * Since: 1.16
 * Stability: unstable
 */
CoglOverlay *
cogl_overlay_new (CoglOutput *output);

void
cogl_overlay_set_onscreen_source (CoglOverlay *overlay,
                                  CoglOnscreen *onscreen_source);

CoglOnscreen *
cogl_overlay_get_onscreen_source (CoglOverlay *overlay);

int
cogl_overlay_get_source_x (CoglOverlay *overlay);

void
cogl_overlay_set_source_x (CoglOverlay *overlay, int src_x);

int
cogl_overlay_get_source_y (CoglOverlay *overlay);

void
cogl_overlay_set_source_y (CoglOverlay *overlay, int src_y);

int
cogl_overlay_get_source_width (CoglOverlay *overlay);

void
cogl_overlay_set_source_width (CoglOverlay *overlay, int src_width);

int
cogl_overlay_get_source_height (CoglOverlay *overlay);

void
cogl_overlay_set_source_height (CoglOverlay *overlay, int src_height);

int
cogl_overlay_get_x (CoglOverlay *overlay);

void
cogl_overlay_set_x (CoglOverlay *overlay, int dst_x);

int
cogl_overlay_get_y (CoglOverlay *overlay);

void
cogl_overlay_set_y (CoglOverlay *overlay, int dst_y);

COGL_END_DECLS

#endif /* _COGL_OVERLAY_H_ */
