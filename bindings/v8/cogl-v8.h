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
#ifndef __COGL_V8_H__
#define __COGL_V8_H__

#include <glib.h>

#include <v8.h>

#include <cogl/cogl.h>

typedef struct _CoglV8Context
{
  GHashTable *colors_hash;

  CoglContext *ctx;
  v8::Persistent<v8::Context> v8_context;
  v8::Handle<v8::Script> script;

  v8::Handle<v8::FunctionTemplate> pipeline_template;
  v8::Handle<v8::FunctionTemplate> primitive_template;

  CoglFramebuffer *fb;

  CoglPrimitive *triangle;
  CoglPipeline *pipeline;

  GMainLoop *loop;
} CoglV8Context;

G_BEGIN_DECLS

extern CoglV8Context _cogl_v8_context;

void
_cogl_v8_object_weak_callback (v8::Persistent<v8::Value> object, void *parameter);

G_END_DECLS

#endif /* __COGL_V8_H__ */
