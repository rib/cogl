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

#ifndef __COGL_V8_PRIMITIVE_H__
#define __COGL_V8_PRIMITIVE_H__

#include <v8.h>

G_BEGIN_DECLS

void
_cogl_v8_primitive_bind (v8::Handle<v8::ObjectTemplate> cogl);

G_END_DECLS

#endif /* __COGL_V8_PRIMITIVE_H__ */
