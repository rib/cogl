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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>

#include <v8.h>

#include <cogl/cogl.h>

#include "cogl-v8.h"

#if 0
static v8::Handle<v8::Value>
primitiveSetColor (const v8::Arguments& args)
{
  if (args.Length () < 1) return v8::Undefined();
  v8::HandleScope scope;
  v8::Handle<v8::Value> arg = args[0];
  v8::String::AsciiValue value (arg);

  CoglColor color;
  if (_cogl_color_init_from_string (&color,
                                    *value,
                                    &_cogl_v8_context.colors_hash))
    {
      v8::Handle<v8::Object> holder = args.Holder();
      CoglPrimitive *primitive =
        static_cast<CoglPrimitive *>(holder->GetPointerFromInternalField (0));

      cogl_primitive_set_color (primitive, &color);
    }
  else
    {
      v8::ThrowException(v8::String::New ("Error parsing color"));
      return v8::Undefined();
    }

  return v8::Undefined();
}
#endif

static v8::Handle<v8::Value>
Primitive (const v8::Arguments& args)
{
  v8::HandleScope scope;
  v8::Handle<v8::FunctionTemplate> _template = _cogl_v8_context.primitive_template;
  v8::Handle<v8::Object> _this = args.This();

  if (!(_template->HasInstance (_this)))
    return _template->GetFunction ()->NewInstance ();

  CoglVertexP2C4 triangle_vertices[] = {
        {0, 0.7, 0xff, 0x00, 0x00, 0x80},
        {-0.7, -0.7, 0x00, 0xff, 0x00, 0xff},
        {0.7, -0.7, 0x00, 0x00, 0xff, 0xff}
  };

  CoglPrimitive *primitive =
    cogl_primitive_new_p2c4 (_cogl_v8_context.ctx,
                             COGL_VERTICES_MODE_TRIANGLES,
                             3, triangle_vertices);
  _this->SetPointerInInternalField (0, primitive);

  v8::Persistent<v8::Object> primitive_instance =
    v8::Persistent<v8::Object>::New (_this);
  primitive_instance.MakeWeak (primitive, _cogl_v8_object_weak_callback);

  return v8::Undefined ();
}

extern "C" void
_cogl_v8_primitive_bind (v8::Handle<v8::ObjectTemplate> cogl)
{
  v8::HandleScope scope;

  _cogl_v8_context.primitive_template =
    v8::Persistent<v8::FunctionTemplate>::New (v8::FunctionTemplate::New (Primitive));

  v8::Handle<v8::Signature> signature =
    v8::Signature::New (_cogl_v8_context.primitive_template);

  v8::Local<v8::ObjectTemplate> instance_template =
    _cogl_v8_context.primitive_template->InstanceTemplate ();

#if 0
  v8::Handle<v8::FunctionTemplate> set_color_template =
    v8::FunctionTemplate::New (primitiveSetColor,
                               v8::Handle<v8::Value>(),
                               signature);
  instance_template->Set (v8::String::New ("setColor"), set_color_template);
#endif

  instance_template->SetInternalFieldCount (1);
  cogl->Set (v8::String::New ("Primitive"), _cogl_v8_context.primitive_template);
}
