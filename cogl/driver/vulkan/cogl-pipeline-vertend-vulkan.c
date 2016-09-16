/*
 * Cogl
 *
 * A Low Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2016 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include <test-fixtures/test-unit.h>

#include "cogl-context-private.h"
#include "cogl-util-gl-private.h"
#include "cogl-pipeline-private.h"
#include "cogl-pipeline-vulkan-private.h"

#include "cogl-context-private.h"
#include "cogl-glsl-shader-private.h"
#include "cogl-object-private.h"
#include "cogl-program-private.h"
#include "cogl-pipeline-vertend-vulkan-private.h"
#include "cogl-pipeline-state-private.h"
#include "cogl-shader-vulkan-private.h"
#include "cogl-util-vulkan-private.h"

const CoglPipelineVertend _cogl_pipeline_vulkan_vertend;

typedef struct
{
  unsigned int ref_count;

  GString *block, *header, *source;

  CoglPipelineCacheEntry *cache_entry;

  GString *shader_source;
} CoglPipelineShaderState;

static CoglUserDataKey shader_state_key;

static CoglPipelineShaderState *
shader_state_new (CoglPipelineCacheEntry *cache_entry)
{
  CoglPipelineShaderState *shader_state;

  shader_state = g_slice_new0 (CoglPipelineShaderState);
  shader_state->ref_count = 1;
  shader_state->cache_entry = cache_entry;

  return shader_state;
}

static CoglPipelineShaderState *
get_shader_state (CoglPipeline *pipeline)
{
  return cogl_object_get_user_data (COGL_OBJECT (pipeline), &shader_state_key);
}

static void
destroy_shader_state (void *user_data,
                      void *instance)
{
  CoglPipelineShaderState *shader_state = user_data;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (shader_state->cache_entry &&
      shader_state->cache_entry->pipeline != instance)
    shader_state->cache_entry->usage_count--;

  if (--shader_state->ref_count == 0)
    {
      if (shader_state->shader_source)
        g_string_free (shader_state->shader_source, TRUE);

      g_slice_free (CoglPipelineShaderState, shader_state);
    }
}

static void
set_shader_state (CoglPipeline *pipeline,
                  CoglPipelineShaderState *shader_state)
{
  if (shader_state)
    {
      shader_state->ref_count++;

      /* If we're not setting the state on the template pipeline then
       * mark it as a usage of the pipeline cache entry */
      if (shader_state->cache_entry &&
          shader_state->cache_entry->pipeline != pipeline)
        shader_state->cache_entry->usage_count++;
    }

  _cogl_object_set_user_data (COGL_OBJECT (pipeline),
                              &shader_state_key,
                              shader_state,
                              destroy_shader_state);
}

static void
dirty_shader_state (CoglPipeline *pipeline)
{
  cogl_object_set_user_data (COGL_OBJECT (pipeline),
                             &shader_state_key,
                             NULL,
                             NULL);
}

GString *
_cogl_pipeline_vertend_vulkan_get_shader (CoglPipeline *pipeline)
{
  CoglPipelineShaderState *shader_state = get_shader_state (pipeline);

  if (shader_state)
    return shader_state->shader_source;
  else
    return NULL;
}

static CoglPipelineSnippetList *
get_vertex_snippets (CoglPipeline *pipeline)
{
  pipeline =
    _cogl_pipeline_get_authority (pipeline,
                                  COGL_PIPELINE_STATE_VERTEX_SNIPPETS);

  return &pipeline->big_state->vertex_snippets;
}

static CoglPipelineSnippetList *
get_layer_vertex_snippets (CoglPipelineLayer *layer)
{
  unsigned long state = COGL_PIPELINE_LAYER_STATE_VERTEX_SNIPPETS;
  layer = _cogl_pipeline_layer_get_authority (layer, state);

  return &layer->big_state->vertex_snippets;
}

static CoglBool
add_layer_declaration_cb (CoglPipelineLayer *layer,
                          void *user_data)
{
  CoglPipelineShaderState *shader_state = user_data;
  CoglTextureType texture_type =
    _cogl_pipeline_layer_get_texture_type (layer);
  const char *target_string;

  _cogl_vulkan_util_get_texture_target_string (texture_type,
                                               &target_string, NULL);

  g_string_append_printf (shader_state->header,
                          "uniform sampler%s cogl_sampler%i;\n",
                          target_string,
                          layer->index);

  return TRUE;
}

static void
add_layer_declarations (CoglPipeline *pipeline,
                        CoglPipelineShaderState *shader_state)
{
  /* We always emit sampler uniforms in case there will be custom
   * layer snippets that want to sample arbitrary layers. */

  _cogl_pipeline_foreach_layer_internal (pipeline,
                                         add_layer_declaration_cb,
                                         shader_state);
}

static void
add_global_declarations (CoglPipeline *pipeline,
                         CoglPipelineShaderState *shader_state)
{
  CoglSnippetHook hook = COGL_SNIPPET_HOOK_VERTEX_GLOBALS;
  CoglPipelineSnippetList *snippets = get_vertex_snippets (pipeline);

  /* Add the global data hooks. All of the code in these snippets is
   * always added and only the declarations data is used */

  _cogl_pipeline_snippet_generate_declarations (shader_state->header,
                                                hook,
                                                snippets);
}

static void
_cogl_pipeline_vertend_vulkan_start (CoglPipeline *pipeline,
                                   int n_layers,
                                   unsigned long pipelines_difference)
{
  CoglPipelineShaderState *shader_state;
  CoglPipelineCacheEntry *cache_entry = NULL;
  CoglProgram *user_program = cogl_pipeline_get_user_program (pipeline);

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  /* Now lookup our vulkan backend private state (allocating if
   * necessary) */
  shader_state = get_shader_state (pipeline);

  if (shader_state == NULL)
    {
      CoglPipeline *authority;

      /* Get the authority for anything affecting vertex shader
         state */
      authority = _cogl_pipeline_find_equivalent_parent
        (pipeline,
         _cogl_pipeline_get_state_for_vertex_codegen (ctx) &
         ~COGL_PIPELINE_STATE_LAYERS,
         COGL_PIPELINE_LAYER_STATE_AFFECTS_VERTEX_CODEGEN);

      shader_state = get_shader_state (authority);

      if (shader_state == NULL)
        {
          /* Check if there is already a similar cached pipeline whose
             shader state we can share */
          if (G_LIKELY (!(COGL_DEBUG_ENABLED
                          (COGL_DEBUG_DISABLE_PROGRAM_CACHES))))
            {
              cache_entry =
                _cogl_pipeline_cache_get_vertex_template (ctx->pipeline_cache,
                                                          authority);

              shader_state = get_shader_state (cache_entry->pipeline);
            }

          if (shader_state)
            shader_state->ref_count++;
          else
            shader_state = shader_state_new (cache_entry);

          set_shader_state (authority, shader_state);

          shader_state->ref_count--;

          if (cache_entry)
            set_shader_state (cache_entry->pipeline, shader_state);
        }

      if (authority != pipeline)
        set_shader_state (pipeline, shader_state);
    }

  if (user_program)
    {
      /* If the user program contains a vertex shader then we don't need
         to generate one */
      if (_cogl_program_has_vertex_shader (user_program))
        {
          if (shader_state->shader_source)
            {
              g_string_free (shader_state->shader_source, TRUE);
              shader_state->shader_source = NULL;
            }
          return;
        }
    }

  if (shader_state->shader_source)
    return;

  /* If we make it here then we have a shader_state struct without a gl_shader
     either because this is the first time we've encountered it or
     because the user program has changed */

  /* We reuse two grow-only GStrings for code-gen. One string
     contains the uniform and attribute declarations while the
     other contains the main function. We need two strings
     because we need to dynamically declare attributes as the
     add_layer callback is invoked */

  g_string_set_size (ctx->codegen_uniform_block_buffer, 0);
  g_string_set_size (ctx->codegen_header_buffer, 0);
  g_string_set_size (ctx->codegen_source_buffer, 0);
  shader_state->block = ctx->codegen_uniform_block_buffer;
  shader_state->header = ctx->codegen_header_buffer;
  shader_state->source = ctx->codegen_source_buffer;

  add_layer_declarations (pipeline, shader_state);
  add_global_declarations (pipeline, shader_state);

  g_string_append (shader_state->source,
                   "void\n"
                   "cogl_generated_source ()\n"
                   "{\n");

  if (cogl_pipeline_get_per_vertex_point_size (pipeline))
    g_string_append (shader_state->header,
                     "in float cogl_point_size_in;\n");
  else
    {
      /* There is no builtin uniform for the point size on Vulkan so we need
         to copy it from the custom uniform in the vertex shader if we're
         not using per-vertex point sizes, however we'll only do this if the
         point-size is non-zero. Toggle the point size between zero and
         non-zero causes a state change which generates a new program */
      if (cogl_pipeline_get_point_size (pipeline) > 0.0f)
        {
          g_string_append (shader_state->block,
                           "uniform float cogl_point_size_in;\n");
          g_string_append (shader_state->source,
                           "  cogl_point_size_out = cogl_point_size_in;\n");
        }
    }
}

static CoglBool
_cogl_pipeline_vertend_vulkan_add_layer (CoglPipeline *pipeline,
                                         CoglPipelineLayer *layer,
                                         unsigned long layers_difference,
                                         CoglFramebuffer *framebuffer)
{
  CoglPipelineShaderState *shader_state;
  CoglPipelineSnippetData snippet_data;
  int layer_index = layer->index;

  _COGL_GET_CONTEXT (ctx, FALSE);

  shader_state = get_shader_state (pipeline);

  if (shader_state->source == NULL)
    return TRUE;

  /* Transform the texture coordinates by the layer's user matrix.
   *
   * FIXME: this should avoid doing the transform if there is no user
   * matrix set. This might need a separate layer state flag for
   * whether there is a user matrix
   *
   * FIXME: we could be more clever here and try to detect if the
   * fragment program is going to use the texture coordinates and
   * avoid setting them if not
   */

  g_string_append_printf (shader_state->header,
                          "vec4\n"
                          "cogl_real_transform_layer%i (mat4 matrix, "
                          "vec4 tex_coord)\n"
                          "{\n"
                          "  return matrix * tex_coord;\n"
                          "}\n",
                          layer_index);

  /* Wrap the layer code in any snippets that have been hooked */
  memset (&snippet_data, 0, sizeof (snippet_data));
  snippet_data.snippets = get_layer_vertex_snippets (layer);
  snippet_data.hook = COGL_SNIPPET_HOOK_TEXTURE_COORD_TRANSFORM;
  snippet_data.chain_function = g_strdup_printf ("cogl_real_transform_layer%i",
                                                 layer_index);
  snippet_data.final_name = g_strdup_printf ("cogl_transform_layer%i",
                                             layer_index);
  snippet_data.function_prefix = g_strdup_printf ("cogl_transform_layer%i",
                                                  layer_index);
  snippet_data.return_type = "vec4";
  snippet_data.return_variable = "cogl_tex_coord";
  snippet_data.return_variable_is_argument = TRUE;
  snippet_data.arguments = "cogl_matrix, cogl_tex_coord";
  snippet_data.argument_declarations = "mat4 cogl_matrix, vec4 cogl_tex_coord";
  snippet_data.source_buf = shader_state->header;

  _cogl_pipeline_snippet_generate_code (&snippet_data);

  g_free ((char *) snippet_data.chain_function);
  g_free ((char *) snippet_data.final_name);
  g_free ((char *) snippet_data.function_prefix);

  g_string_append_printf (shader_state->source,
                          "  cogl_tex_coord%i_out = "
                          "cogl_transform_layer%i (cogl_texture_matrix%i,\n"
                          "                           "
                          "                        cogl_tex_coord%i_in);\n",
                          layer_index,
                          layer_index,
                          layer_index,
                          layer_index);

  return TRUE;
}

static CoglBool
_cogl_pipeline_vertend_vulkan_end (CoglPipeline *pipeline,
                                 unsigned long pipelines_difference)
{
  CoglPipelineShaderState *shader_state;

  _COGL_GET_CONTEXT (ctx, FALSE);

  shader_state = get_shader_state (pipeline);

  if (shader_state->source)
    {
      GString *shader_source;
      CoglPipelineSnippetData snippet_data;
      CoglPipelineSnippetList *vertex_snippets;
      CoglBool has_per_vertex_point_size =
        cogl_pipeline_get_per_vertex_point_size (pipeline);

      COGL_STATIC_COUNTER (vertend_vulkan_compile_counter,
                           "vulkan vertex compile counter",
                           "Increments each time a new VULKAN "
                           "vertex shader is compiled",
                           0 /* no application private data */);
      COGL_COUNTER_INC (_cogl_uprof_context, vertend_vulkan_compile_counter);

      g_string_append (shader_state->header,
                       "void\n"
                       "cogl_real_vertex_transform ()\n"
                       "{\n"
                       "  cogl_position_out = "
                       "cogl_modelview_projection_matrix * "
                       "cogl_position_in;\n"
                       "}\n");

      g_string_append (shader_state->source,
                       "  cogl_vertex_transform ();\n");

      if (has_per_vertex_point_size)
        {
          g_string_append (shader_state->header,
                           "void\n"
                           "cogl_real_point_size_calculation ()\n"
                           "{\n"
                           "  cogl_point_size_out = cogl_point_size_in;\n"
                           "}\n");
          g_string_append (shader_state->source,
                           "  cogl_point_size_calculation ();\n");
        }

      g_string_append (shader_state->source,
                       "  cogl_color_out = cogl_color_in;\n"
                       "}\n");

      vertex_snippets = get_vertex_snippets (pipeline);

      /* Add hooks for the vertex transform part */
      memset (&snippet_data, 0, sizeof (snippet_data));
      snippet_data.snippets = vertex_snippets;
      snippet_data.hook = COGL_SNIPPET_HOOK_VERTEX_TRANSFORM;
      snippet_data.chain_function = "cogl_real_vertex_transform";
      snippet_data.final_name = "cogl_vertex_transform";
      snippet_data.function_prefix = "cogl_vertex_transform";
      snippet_data.source_buf = shader_state->header;
      _cogl_pipeline_snippet_generate_code (&snippet_data);

      /* Add hooks for the point size calculation part */
      if (has_per_vertex_point_size)
        {
          memset (&snippet_data, 0, sizeof (snippet_data));
          snippet_data.snippets = vertex_snippets;
          snippet_data.hook = COGL_SNIPPET_HOOK_POINT_SIZE;
          snippet_data.chain_function = "cogl_real_point_size_calculation";
          snippet_data.final_name = "cogl_point_size_calculation";
          snippet_data.function_prefix = "cogl_point_size_calculation";
          snippet_data.source_buf = shader_state->header;
          _cogl_pipeline_snippet_generate_code (&snippet_data);
        }

      /* Add all of the hooks for vertex processing */
      memset (&snippet_data, 0, sizeof (snippet_data));
      snippet_data.snippets = vertex_snippets;
      snippet_data.hook = COGL_SNIPPET_HOOK_VERTEX;
      snippet_data.chain_function = "cogl_generated_source";
      snippet_data.final_name = "cogl_vertex_hook";
      snippet_data.function_prefix = "cogl_vertex_hook";
      snippet_data.source_buf = shader_state->source;
      _cogl_pipeline_snippet_generate_code (&snippet_data);

      g_string_append (shader_state->source,
                       "void\n"
                       "main ()\n"
                       "{\n"
                       "  cogl_vertex_hook ();\n"
                       "}\n");

      shader_source =
        _cogl_glsl_vulkan_shader_get_source_with_boilerplate (ctx,
                                                              COGL_GLSL_SHADER_TYPE_VERTEX,
                                                              pipeline,
                                                              shader_state->block,
                                                              shader_state->header,
                                                              shader_state->source);

      shader_state->block = NULL;
      shader_state->header = NULL;
      shader_state->source = NULL;

      shader_state->shader_source = shader_source;
    }

  return TRUE;
}

static void
_cogl_pipeline_vertend_vulkan_pre_change_notify (CoglPipeline *pipeline,
                                                 CoglPipelineState change,
                                                 const CoglColor *new_color)
{
  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if ((change & _cogl_pipeline_get_state_for_vertex_codegen (ctx)))
    dirty_shader_state (pipeline);
}

/* NB: layers are considered immutable once they have any dependants
 * so although multiple pipelines can end up depending on a single
 * static layer, we can guarantee that if a layer is being *changed*
 * then it can only have one pipeline depending on it.
 *
 * XXX: Don't forget this is *pre* change, we can't read the new value
 * yet!
 */
static void
_cogl_pipeline_vertend_vulkan_layer_pre_change_notify (
                                                CoglPipeline *owner,
                                                CoglPipelineLayer *layer,
                                                CoglPipelineLayerState change)
{
  CoglPipelineShaderState *shader_state;

  shader_state = get_shader_state (owner);
  if (!shader_state)
    return;

  if ((change & COGL_PIPELINE_LAYER_STATE_AFFECTS_VERTEX_CODEGEN))
    {
      dirty_shader_state (owner);
      return;
    }

  /* TODO: we could be saving snippets of texture combine code along
   * with each layer and then when a layer changes we would just free
   * the snippet. */
}

const CoglPipelineVertend _cogl_pipeline_vulkan_vertend =
  {
    _cogl_pipeline_vertend_vulkan_start,
    _cogl_pipeline_vertend_vulkan_add_layer,
    _cogl_pipeline_vertend_vulkan_end,
    _cogl_pipeline_vertend_vulkan_pre_change_notify,
    _cogl_pipeline_vertend_vulkan_layer_pre_change_notify
  };
