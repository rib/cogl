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

#ifndef _COGL_MODE_H_
#define _COGL_MODE_H_

#include <cogl/cogl-types.h>

COGL_BEGIN_DECLS

typedef struct _CoglMode CoglMode;

CoglBool
cogl_is_mode (void *object);

const char *
cogl_mode_get_name (CoglMode *mode);

float
cogl_mode_get_refresh_rate (CoglMode *mode);

int
cogl_mode_get_width (CoglMode *mode);

int
cogl_mode_get_height (CoglMode *mode);

COGL_END_DECLS

#endif /* _COGL_MODE_H_ */
