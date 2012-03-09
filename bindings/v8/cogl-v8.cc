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
#include <string.h>

#include <v8.h>

#include <cogl/cogl.h>

#include "cogl-v8.h"
#include "cogl-v8-pipeline.h"
#include "cogl-v8-primitive.h"

CoglV8Context _cogl_v8_context;

extern "C" void
_cogl_v8_object_weak_callback (v8::Persistent<v8::Value> object, void *parameter)
{
  CoglObject *cogl_object = static_cast<CoglObject *>(parameter);

  cogl_object_unref (cogl_object);

  object.Dispose ();
  object.Clear ();
}

static void
cogl_v8_add_bindings (v8::Handle<v8::ObjectTemplate> global)
{
  v8::Handle<v8::ObjectTemplate> cogl = v8::ObjectTemplate::New ();
  global->Set (v8::String::New ("cogl"), cogl);

  _cogl_v8_pipeline_bind (cogl);
  _cogl_v8_primitive_bind (cogl);
}

static v8::Handle<v8::Value>
runMain (const v8::Arguments& args)
{
  g_main_loop_run (_cogl_v8_context.loop);
  return v8::Undefined ();
}

static gboolean
paint_cb (void *user_data)
{
  cogl_framebuffer_clear4f (_cogl_v8_context.fb,
                            COGL_BUFFER_BIT_COLOR, 0, 0, 0, 1);
  cogl_framebuffer_draw_primitive (_cogl_v8_context.fb,
                                   _cogl_v8_context.pipeline,
                                   _cogl_v8_context.triangle);
  cogl_onscreen_swap_buffers (COGL_ONSCREEN (_cogl_v8_context.fb));

  /* If the driver can deliver swap complete events then we can remove
   * the idle paint callback until we next get a swap complete event
   * otherwise we keep the idle paint callback installed and simply
   * paint as fast as the driver will allow... */
  if (cogl_has_feature (_cogl_v8_context.ctx,
                        COGL_FEATURE_ID_SWAP_BUFFERS_EVENT))
    return FALSE; /* remove the callback */
  else
    return TRUE;
}

static void
swap_complete_cb (CoglFramebuffer *framebuffer, void *user_data)
{
  g_idle_add (paint_cb, user_data);
}

static gboolean
garbage_collect (void *user_data)
{
  v8::V8::IdleNotification ();
  return TRUE;
}

int
main (int argc, char **argv)
{
  CoglOnscreen *onscreen;
  GError *error = NULL;
  CoglVertexP2C4 triangle_vertices[] = {
        {0, 0.7, 0xff, 0x00, 0x00, 0x80},
        {-0.7, -0.7, 0x00, 0xff, 0x00, 0xff},
        {0.7, -0.7, 0x00, 0x00, 0xff, 0xff}
  };
  GSource *cogl_source;
  char *script;
  gsize len;

  if (argc != 2)
    {
      g_printerr ("Usage: %s SCRIPT\n", argv[0]);
      return 1;
    }

  _cogl_v8_context.ctx = cogl_context_new (NULL, &error);
  if (!_cogl_v8_context.ctx)
    {
      g_error ("Failed to create context: %s\n", error->message);
      return 1;
    }

  onscreen = cogl_onscreen_new (_cogl_v8_context.ctx, 640, 480);
  cogl_onscreen_show (onscreen);
  _cogl_v8_context.fb = COGL_FRAMEBUFFER (onscreen);

  _cogl_v8_context.triangle =
    cogl_primitive_new_p2c4 (_cogl_v8_context.ctx,
                             COGL_VERTICES_MODE_TRIANGLES,
                             3, triangle_vertices);
  _cogl_v8_context.pipeline = cogl_pipeline_new (_cogl_v8_context.ctx);

  v8::HandleScope handle_scope;

  v8::Handle<v8::ObjectTemplate> global = v8::ObjectTemplate::New ();
  global->Set (v8::String::New ("runMain"),
               v8::FunctionTemplate::New (runMain));

  cogl_v8_add_bindings (global);

  _cogl_v8_context.v8_context = v8::Context::New (NULL, global);

  v8::Context::Scope context_scope (_cogl_v8_context.v8_context);

  if (!g_file_get_contents (argv[1], &script, &len, &error))
    {
      g_error ("Failed to read source: %s", error->message);
      return 1;
    }

  v8::Handle<v8::String> source = v8::String::New (script, len);

  v8::TryCatch trycatch;
  _cogl_v8_context.script = v8::Script::Compile (source);
  if (_cogl_v8_context.script.IsEmpty ())
    {
      v8::Handle<v8::Value> exception = trycatch.Exception ();
      v8::String::AsciiValue exception_str (exception);
      g_printerr ("Error compiling script: %s\n", *exception_str);
      return 1;
    }

  cogl_source = cogl_glib_source_new (_cogl_v8_context.ctx, G_PRIORITY_DEFAULT);

  g_source_attach (cogl_source, NULL);

  if (cogl_has_feature (_cogl_v8_context.ctx,
                        COGL_FEATURE_ID_SWAP_BUFFERS_EVENT))
    cogl_onscreen_add_swap_buffers_callback (onscreen,
                                             swap_complete_cb,
                                             &_cogl_v8_context);

  g_idle_add (paint_cb, &_cogl_v8_context);
  g_idle_add (garbage_collect, NULL);

  _cogl_v8_context.loop = g_main_loop_new (NULL, TRUE);

  v8::Handle<v8::Value> v = _cogl_v8_context.script->Run ();
  if (v.IsEmpty())
    {
      v8::Handle<v8::Value> exception = trycatch.Exception ();
      v8::String::AsciiValue exception_str (exception);
      g_printerr ("Error running script: %s\n", *exception_str);
    }

  _cogl_v8_context.v8_context.Dispose ();

  return 0;
}
