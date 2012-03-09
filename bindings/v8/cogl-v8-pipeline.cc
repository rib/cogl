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
#include "cogl-v8-color.h"

static v8::Handle<v8::Value>
pipelineSetColor (const v8::Arguments& args)
{
  if (args.Length () < 1)
    return v8::Undefined();

  v8::HandleScope scope;

  v8::Handle<v8::Value> arg = args[0];
  v8::String::AsciiValue value (arg);

  CoglColor color;
  if (_cogl_color_init_from_string (&color,
                                    *value,
                                    &_cogl_v8_context.colors_hash))
    {
      v8::Handle<v8::Object> holder = args.Holder();
      CoglPipeline *pipeline =
        static_cast<CoglPipeline *>(holder->GetPointerFromInternalField (0));

      g_print ("setColor (%f, %f, %f, %f)\n",
               cogl_color_get_red_float (&color),
               cogl_color_get_green_float (&color),
               cogl_color_get_blue_float (&color),
               cogl_color_get_alpha_float (&color));

      cogl_pipeline_set_color (pipeline, &color);
    }
  else
    {
      v8::ThrowException(v8::String::New ("Error parsing color"));
      return v8::Undefined();
    }

  return v8::Undefined();
}

static v8::Handle<v8::Value>
Pipeline (const v8::Arguments& args)
{
  v8::HandleScope scope;
  v8::Handle<v8::FunctionTemplate> _template = _cogl_v8_context.pipeline_template;
  v8::Handle<v8::Object> _this = args.This();

  /* This trick is to make the new operator optional for object
   * constructor functions. The idea was borrowed from jQuery.
   *
   * XXX: for reference a similar trick can be done in javascript using:
   * if (!(this instanceof arguments.callee))
   *   return new cogl.Pipeline();
   */
  if (!(_template->HasInstance (_this)))
    return _template->GetFunction ()->NewInstance ();

  CoglPipeline *pipeline = cogl_pipeline_new (_cogl_v8_context.ctx);
  _this->SetPointerInInternalField (0, pipeline);

  v8::Persistent<v8::Object> pipeline_instance =
    v8::Persistent<v8::Object>::New (_this);
  pipeline_instance.MakeWeak (pipeline, _cogl_v8_object_weak_callback);

  return v8::Undefined ();
}

extern "C" void
_cogl_v8_pipeline_bind (v8::Handle<v8::ObjectTemplate> cogl)
{
  v8::HandleScope scope;

  _cogl_v8_context.pipeline_template =
    v8::Persistent<v8::FunctionTemplate>::New (v8::FunctionTemplate::New (Pipeline));

  /* A signature seems to provide a good way of asking the v8 engine
   * to do some automatic type checking of arguments and of 'this'
   * when a method is called so we can be absolutely sure that our
   * native methods only ever get called for objects *we* created
   * where we know we own any internal fields set on those objects.
   *
   * Without this guarantee it would be possible for rouge code to
   * copy the function objects for methods into other objects and
   * potentially cause a crash if we were to try accessing internal
   * fields that might not be available or might be owned by something
   * else.
   *
   * Lots of digging through code and forums trying to figure out how
   * to safely validate 'this' when a method is called eventually lead
   * me to this forum topic which also details very important
   * information about the Arguments::Holder() function, required if
   * you want to write robust bindings...
   *
   * https://groups.google.com/forum/?fromgroups#!topic/v8-users/Axf4hF_RfZo
   *
   *  "In short: if you specify, through a Signature, that a function
   *   must only be called on instances of function template T, the
   *   value returned by Holder is guaranteed to hold an instance
   *   created from T or another function template that directly or
   *   indirectly "FunctionTemplate::Inherit"s from T.  No guarantees
   *   hold about the type of This."
   *
   * It would be nice if the v8 documentation said more than just:
   *
   *   "A Signature specifies which receivers and arguments a function
   *   can legally be called with."
   *
   * and said more than this about Arguments::Holder():
   *
   *   "Definition at line 3930 of file v8.h."
   */
  v8::Handle<v8::Signature> signature =
    v8::Signature::New (_cogl_v8_context.pipeline_template);

  v8::Local<v8::ObjectTemplate> instance_template =
    _cogl_v8_context.pipeline_template->InstanceTemplate ();

  v8::Handle<v8::FunctionTemplate> set_color_template =
    v8::FunctionTemplate::New (pipelineSetColor,
                               v8::Handle<v8::Value>(),
                               signature);
  instance_template->Set (v8::String::New ("setColor"), set_color_template);

  instance_template->SetInternalFieldCount (1);
  cogl->Set (v8::String::New ("Pipeline"), _cogl_v8_context.pipeline_template);
}
