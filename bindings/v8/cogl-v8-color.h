/*
 * Cogl V8 Bindings
 *
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
 */

#ifndef __COGL_V8_COLOR_H__
#define __COGL_V8_COLOR_H__

gboolean
_cogl_color_init_from_string (CoglColor *color,
                              const char *str,
                              GHashTable **colors_hash);

#endif /* __COGL_V8_COLOR_H__ */
