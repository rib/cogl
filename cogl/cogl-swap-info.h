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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 *
 *
 * Authors:
 *   Owen Taylor <otaylor@redhat.com>
 */
#if !defined(__COGL_H_INSIDE__) && !defined(COGL_COMPILATION)
#error "Only <cogl/cogl.h> can be included directly."
#endif

#ifndef __COGL_SWAP_INFO_H
#define __COGL_SWAP_INFO_H

#include <cogl/cogl-types.h>
#include <glib.h>

G_BEGIN_DECLS

typedef struct _CoglSwapInfo CoglSwapInfo;
#define COGL_SWAP_INFO(X) ((CoglSwapInfo *)(X))

/**
 * cogl_is_swap_info:
 * @object: A #CoglObject pointer
 *
 * Gets whether the given object references a #CoglSwapInfo.
 *
 * Return value: %TRUE if the object references a #CoglSwapInfo
 *   and %FALSE otherwise.
 * Since: 2.0
 * Stability: unstable
 */
CoglBool
cogl_is_swap_info (void *object);

/**
 * cogl_swap_info_get_complete:
 * @info: a #CoglSwapInfo object
 *
 * Gets whether all information that will potentially be provided for
 * the frame has been provided. Once a swap info object is complete,
 * no further changes will be made to it.
 *
 * Return value: whether the swap info object is complete.
 * Since: 2.0
 * Stability: unstable
 */
CoglBool cogl_swap_info_get_complete (CoglSwapInfo *info);

/**
 * cogl_swap_info_get_frame_counter:
 * @info: a #CoglSwapInfo object
 *
 * Gets the frame counter for the #CoglOnscreen that corresponds
 * to this frame.
 *
 * Return value: The frame counter value
 * Since: 2.0
 * Stability: unstable
 */
int64_t cogl_swap_info_get_frame_counter (CoglSwapInfo *info);

/**
 * cogl_swap_info_get_frame_time:
 * @info: a #CoglSwapInfo object
 *
 * Gets the time used for creating content for the frame. This
 * is determined by the time passed to cogl_onscreen_begin_frame(),
 * and will typically be the current time when rendering started
 * for the frame.
 *
 * Return value: the time used for coreating content for the frame,
 *  in the timescale of g_get_monotonic_time().
 * Since: 2.0
 * Stability: unstable
 */
int64_t cogl_swap_info_get_frame_time (CoglSwapInfo *info);

/**
 * cogl_swap_info_get_presentation_time:
 * @info: a #CoglSwapInfo object
 *
 * Gets the presentation time for the frame. This is the time at which
 * the frame became visible to the user.
 *
 * Return value: the presentation time for the frame, in
 *  the timescale of g_get_monotonic_time().
 * Since: 2.0
 * Stability: unstable
 */
int64_t cogl_swap_info_get_presentation_time (CoglSwapInfo *info);

/**
 * cogl_swap_info_get_refresh_interval:
 * @info: a #CoglSwapInfo object
 *
 * Gets the refresh interval for the output that the frame was on at the
 * time the frame was presented. This is the number of microseconds between
 * refreshes of the screen, and is equal to 1000000 / refresh_rate.
 *
 * Return value: the refresh interval, in microsecoonds.
 *  .
 * Since: 2.0
 * Stability: unstable
 */
int64_t cogl_swap_info_get_refresh_interval (CoglSwapInfo *info);

G_END_DECLS

#endif /* __COGL_SWAP_INFO_H */



