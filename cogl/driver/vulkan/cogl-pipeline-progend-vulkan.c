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

#include "cogl-util.h"
#include "cogl-context-private.h"
#include "cogl-util-gl-private.h"
#include "cogl-pipeline-private.h"
#include "cogl-pipeline-vulkan-private.h"
#include "cogl-offscreen.h"

#include "cogl-object-private.h"
#include "cogl-attribute-private.h"
#include "cogl-buffer-vulkan-private.h"
#include "cogl-context-private.h"
#include "cogl-driver-vulkan-private.h"
#include "cogl-framebuffer-vulkan-private.h"
#include "cogl-gtype-private.h"
#include "cogl-pipeline-cache.h"
#include "cogl-pipeline-fragend-vulkan-private.h"
#include "cogl-pipeline-progend-vulkan-private.h"
#include "cogl-pipeline-state-private.h"
#include "cogl-pipeline-vertend-vulkan-private.h"
#include "cogl-program-private.h"
#include "cogl-shader-vulkan-private.h"
#include "cogl-texture-2d-vulkan-private.h"
#include "cogl-util-vulkan-private.h"

/* These are used to generalise updating some uniforms that are
   required when building for drivers missing some fixed function
   state that we use */

typedef void (* UpdateUniformFunc) (CoglPipeline *pipeline,
                                    CoglShaderVulkanUniform *location,
                                    void *getter_func);

static void update_float_uniform (CoglPipeline *pipeline,
                                  CoglShaderVulkanUniform *location,
                                  void *getter_func);

typedef struct _CoglUniformBuffer CoglUniformBuffer;

typedef struct
{
  const char *uniform_name;
  void *getter_func;
  UpdateUniformFunc update_func;
  CoglPipelineState change;

  /* This builtin is only necessary if the following private feature
   * is not implemented in the driver */
  CoglPrivateFeature feature_replacement;
} BuiltinUniformData;

static BuiltinUniformData builtin_uniforms[] =
  {
    { "cogl_point_size_in",
      cogl_pipeline_get_point_size, update_float_uniform,
      COGL_PIPELINE_STATE_POINT_SIZE,
      COGL_PRIVATE_FEATURE_BUILTIN_POINT_SIZE_UNIFORM },
    { "_cogl_alpha_test_ref",
      cogl_pipeline_get_alpha_test_reference, update_float_uniform,
      COGL_PIPELINE_STATE_ALPHA_FUNC_REFERENCE,
      COGL_PRIVATE_FEATURE_ALPHA_TEST }
  };

const CoglPipelineProgend _cogl_pipeline_vulkan_progend;

typedef struct _UnitState
{
  CoglPipelineLayerState changes;

  unsigned int dirty_combine_constant:1;
  unsigned int dirty_texture_matrix:1;

  CoglShaderVulkanUniform *combine_constant_uniform;
  CoglShaderVulkanUniform *texture_matrix_uniform;

  int binding;

  /* Not owned */
  VkSampler sampler;
  VkImageView image_view;
} UnitState;

typedef struct
{
  unsigned int ref_count;

  CoglBuffer *uniform_buffers[COGL_SHADER_VULKAN_NB_STAGES];
  /* Pointers to mapped uniform_buffers memory. */
  void *uniform_datas[COGL_SHADER_VULKAN_NB_STAGES];
  CoglBool uniforms_dirty[COGL_SHADER_VULKAN_NB_STAGES];

  VkPipelineLayout pipeline_layout;

  VkDescriptorSetLayout descriptor_set_layout;
  VkDescriptorPool descriptor_pool;
  VkDescriptorSet descriptor_set;

  CoglShaderVulkan *shader;

  VkPipelineShaderStageCreateInfo stage_info[COGL_SHADER_VULKAN_NB_STAGES];

  unsigned long dirty_builtin_uniforms;
  CoglShaderVulkanUniform *builtin_uniform_locations[G_N_ELEMENTS (builtin_uniforms)];

  CoglShaderVulkanUniform *modelview_uniform;
  CoglShaderVulkanUniform *projection_uniform;
  CoglShaderVulkanUniform *mvp_uniform;

  CoglMatrixEntryCache projection_cache;
  CoglMatrixEntryCache modelview_cache;

  /* We need to track the last pipeline that the program was used with
   * so know if we need to update all of the uniforms */
  CoglPipeline *last_used_for_pipeline;

  /* Array of GL uniform locations indexed by Cogl's uniform
     location. We are careful only to allocated this array if a custom
     uniform is actually set */
  GArray *uniform_locations;

  /* Array of descriptors to write before doing any drawing. This array is
     allocated with (n_layer + 2) elements (including 1 uniform vertex
     buffer & 1 uniform fragment buffer). */
  VkWriteDescriptorSet *write_descriptor_sets;
  int n_write_descriptor_sets;

  /* Array of image descriptors mapping 1:1 with write_descriptor_sets. This
     array is allocated with n_layer elements. */
  VkDescriptorImageInfo *descriptor_image_infos;

  UnitState *unit_state;
  int n_layers;

  CoglPipelineCacheEntry *cache_entry;

  /* Pipelines currently used by the GPU and using this program. */
  GHashTable *pipelines;
} CoglPipelineProgramState;


typedef struct _CoglUniformBuffer CoglUniformBuffer;
struct _CoglUniformBuffer
{
  CoglBuffer _parent;
};

#ifdef COGL_HAS_GTYPE_SUPPORT
GType cogl_uniform_buffer_get_gtype (void);
#endif
CoglBool cogl_is_uniform_buffer (void *object);
static void _cogl_uniform_buffer_free (CoglUniformBuffer *uniforms);

COGL_BUFFER_DEFINE (UniformBuffer, uniform_buffer);
COGL_GTYPE_DEFINE_CLASS (UniformBuffer, uniform_buffer);

static CoglUniformBuffer *
cogl_uniform_buffer_new (CoglContext *context, size_t bytes)
{
  CoglUniformBuffer *uniforms = g_slice_new (CoglUniformBuffer);

  /* parent's constructor */
  _cogl_buffer_initialize (COGL_BUFFER (uniforms),
                           context,
                           bytes,
                           COGL_BUFFER_BIND_TARGET_UNIFORM_BUFFER,
                           COGL_BUFFER_USAGE_HINT_UNIFORM_BUFFER,
                           COGL_BUFFER_UPDATE_HINT_STATIC);

  return _cogl_uniform_buffer_object_new (uniforms);
}

static void
_cogl_uniform_buffer_free (CoglUniformBuffer *uniforms)
{
  /* parent's destructor */
  _cogl_buffer_fini (COGL_BUFFER (uniforms));

  g_slice_free (CoglUniformBuffer, uniforms);
}

static CoglUserDataKey program_state_key;

static CoglPipelineProgramState *
get_program_state (CoglPipeline *pipeline)
{
  return cogl_object_get_user_data (COGL_OBJECT (pipeline), &program_state_key);
}

#define UNIFORM_LOCATION_UNKNOWN -2

static void
clear_flushed_matrix_stacks (CoglPipelineProgramState *program_state)
{
  _cogl_matrix_entry_cache_destroy (&program_state->projection_cache);
  _cogl_matrix_entry_cache_init (&program_state->projection_cache);
  _cogl_matrix_entry_cache_destroy (&program_state->modelview_cache);
  _cogl_matrix_entry_cache_init (&program_state->modelview_cache);
}

static CoglPipelineProgramState *
program_state_new (int n_layers,
                   CoglPipelineCacheEntry *cache_entry)
{
  CoglPipelineProgramState *program_state;

  program_state = g_slice_new0 (CoglPipelineProgramState);
  program_state->ref_count = 1;
  program_state->n_layers = n_layers;
  program_state->unit_state = g_new0 (UnitState, n_layers);
  program_state->write_descriptor_sets = g_new0 (VkWriteDescriptorSet, n_layers + 2);
  program_state->descriptor_image_infos = g_new0 (VkDescriptorImageInfo, n_layers);
  program_state->uniform_locations = NULL;
  program_state->cache_entry = cache_entry;
  _cogl_matrix_entry_cache_init (&program_state->modelview_cache);
  _cogl_matrix_entry_cache_init (&program_state->projection_cache);
  program_state->pipelines = g_hash_table_new (g_direct_hash, g_direct_equal);

  return program_state;
}

static void
destroy_program_state (void *user_data,
                       void *instance)
{
  CoglPipelineProgramState *program_state = user_data;
  CoglContextVulkan *vk_ctx;
  int i;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  vk_ctx = ctx->winsys;

  /* If the program state was last used for this pipeline then clear
     it so that if same address gets used again for a new pipeline
     then we won't think it's the same pipeline and avoid updating the
     uniforms */
  if (program_state->last_used_for_pipeline == instance)
    program_state->last_used_for_pipeline = NULL;

  if (program_state->cache_entry &&
      program_state->cache_entry->pipeline != instance)
    program_state->cache_entry->usage_count--;

  if (--program_state->ref_count == 0)
    {
      _cogl_matrix_entry_cache_destroy (&program_state->projection_cache);
      _cogl_matrix_entry_cache_destroy (&program_state->modelview_cache);

      for (i = 0; i < G_N_ELEMENTS (program_state->uniform_buffers); i++)
        {
          cogl_buffer_unmap (COGL_BUFFER (program_state->uniform_buffers[i]));
          cogl_object_unref (COGL_OBJECT (program_state->uniform_buffers[i]));
        }

      if (program_state->descriptor_set != VK_NULL_HANDLE)
        VK ( ctx,
             vkFreeDescriptorSets (vk_ctx->device,
                                   program_state->descriptor_pool,
                                   1, &program_state->descriptor_set) );

      if (program_state->descriptor_pool != VK_NULL_HANDLE)
        VK ( ctx,
             vkDestroyDescriptorPool (vk_ctx->device,
                                      program_state->descriptor_pool,
                                      NULL) );

      if (program_state->descriptor_set_layout != VK_NULL_HANDLE)
        VK ( ctx,
             vkDestroyDescriptorSetLayout (vk_ctx->device,
                                           program_state->descriptor_set_layout,
                                           NULL) );

      if (program_state->pipeline_layout != VK_NULL_HANDLE)
        VK ( ctx,
             vkDestroyPipelineLayout (vk_ctx->device,
                                      program_state->pipeline_layout, NULL) );

      g_free (program_state->unit_state);

      g_free (program_state->write_descriptor_sets);
      g_free (program_state->descriptor_image_infos);

      if (program_state->uniform_locations)
        g_array_free (program_state->uniform_locations, TRUE);

      if (program_state->shader)
        _cogl_shader_vulkan_free (program_state->shader);

      g_hash_table_unref (program_state->pipelines);

      g_slice_free (CoglPipelineProgramState, program_state);
    }
}

static void
set_program_state (CoglPipeline *pipeline,
                  CoglPipelineProgramState *program_state)
{
  if (program_state)
    {
      program_state->ref_count++;

      /* If we're not setting the state on the template pipeline then
       * mark it as a usage of the pipeline cache entry */
      if (program_state->cache_entry &&
          program_state->cache_entry->pipeline != pipeline)
        program_state->cache_entry->usage_count++;
    }

  _cogl_object_set_user_data (COGL_OBJECT (pipeline),
                              &program_state_key,
                              program_state,
                              destroy_program_state);
}

static void
dirty_program_state (CoglPipeline *pipeline)
{
  cogl_object_set_user_data (COGL_OBJECT (pipeline),
                             &program_state_key,
                             NULL,
                             NULL);
  _cogl_pipeline_vulkan_invalidate (pipeline);
}

/* Waits for all pipeline using this program to have completed their
   work. */
static void
program_state_end (CoglPipelineProgramState *program_state)
{
  CoglPipeline *pipeline;
  GHashTableIter iter;

  if (g_hash_table_size (program_state->pipelines) < 1)
    return;

  g_hash_table_iter_init (&iter, program_state->pipelines);
  while (g_hash_table_iter_next (&iter, (gpointer *) &pipeline, NULL)) {
    g_hash_table_iter_remove (&iter);
    _cogl_pipeline_vulkan_end (pipeline);
  }
}

static CoglShaderVulkanUniform *
get_program_state_uniform_location (CoglPipelineProgramState *program_state,
                                    const char *name)
{
  return _cogl_shader_vulkan_get_uniform (program_state->shader,
                                          name);
}

static int
get_program_state_sampler_binding (CoglPipelineProgramState *program_state,
                                   const char *name)
{
  return _cogl_shader_vulkan_get_sampler_binding (program_state->shader,
                                                  COGL_GLSL_SHADER_TYPE_VERTEX,
                                                  name);
}

static void
set_program_state_uniform (CoglPipelineProgramState *program_state,
                           CoglShaderVulkanUniform *location,
                           const void *data,
                           size_t size)
{
  int i;

  g_assert (program_state->shader);

  program_state_end (program_state);

  for (i = 0; i < COGL_SHADER_VULKAN_NB_STAGES; i++)
    {
      if (location->offsets[i] != -1)
        {
          COGL_NOTE (VULKAN, "Uniform=%s stage=%i offset=%i size=%lu",
                     location->name, i, location->offsets[i], size);

          memcpy ((uint8_t *) program_state->uniform_datas[i] +
                  location->offsets[i],
                  data, size);
          program_state->uniforms_dirty[i] = TRUE;
        }
    }
}

#define set_program_state_uniform1i(program_state, location, data)      \
  set_program_state_uniform(program_state, location, &data, sizeof(int))
#define set_program_state_uniform1f(program_state, location, data)      \
  set_program_state_uniform(program_state, location, &data, sizeof(float))
#define set_program_state_uniform4fv(program_state, location, count, data) \
  set_program_state_uniform(program_state, location, data,              \
                            (count) * 4 * sizeof(float))
#define set_program_state_uniform_matrix4fv(program_state, location, count, data) \
  set_program_state_uniform(program_state, location, data,              \
                            (count) * 16 * sizeof(float))

VkDescriptorSet
_cogl_pipeline_progend_get_vulkan_descriptor_set (CoglPipeline *pipeline)
{
  CoglPipelineProgramState *program_state = get_program_state (pipeline);

  return program_state->descriptor_set;
}

VkPipelineLayout
_cogl_pipeline_progend_get_vulkan_pipeline_layout (CoglPipeline *pipeline)
{
  CoglPipelineProgramState *program_state = get_program_state (pipeline);

  return program_state->pipeline_layout;
}

VkPipelineShaderStageCreateInfo *
_cogl_pipeline_progend_get_vulkan_stage_info (CoglPipeline *pipeline)
{
  CoglPipelineProgramState *program_state = get_program_state (pipeline);

  return program_state->stage_info;
}

CoglShaderVulkan *
_cogl_pipeline_progend_get_vulkan_shader (CoglPipeline *pipeline)
{
  CoglPipelineProgramState *program_state = get_program_state (pipeline);

  return program_state->shader;
}

typedef struct
{
  int unit;
  CoglBool update_all;
  CoglPipelineProgramState *program_state;
} UpdateUniformsState;

static CoglBool
get_uniform_cb (CoglPipeline *pipeline,
                int layer_index,
                void *user_data)
{
  UpdateUniformsState *state = user_data;
  CoglPipelineProgramState *program_state = state->program_state;
  UnitState *unit_state = &program_state->unit_state[state->unit];

  _COGL_GET_CONTEXT (ctx, FALSE);

  g_string_set_size (ctx->codegen_source_buffer, 0);
  g_string_append_printf (ctx->codegen_source_buffer,
                          "_cogl_layer_constant_%i", layer_index);

  unit_state->combine_constant_uniform =
    get_program_state_uniform_location (program_state,
                                        ctx->codegen_source_buffer->str);

  g_string_set_size (ctx->codegen_source_buffer, 0);
  g_string_append_printf (ctx->codegen_source_buffer,
                          "cogl_texture_matrix[%i]", layer_index);

  unit_state->texture_matrix_uniform =
    get_program_state_uniform_location (program_state,
                                        ctx->codegen_source_buffer->str);

  state->unit++;

  return TRUE;
}

static CoglBool
update_constants_cb (CoglPipeline *pipeline,
                     int layer_index,
                     void *user_data)
{
  UpdateUniformsState *state = user_data;
  CoglPipelineProgramState *program_state = state->program_state;
  UnitState *unit_state = &program_state->unit_state[state->unit++];

  if (unit_state->combine_constant_uniform != NULL &&
      (state->update_all || unit_state->dirty_combine_constant))
    {
      float constant[4];
      _cogl_pipeline_get_layer_combine_constant (pipeline,
                                                 layer_index,
                                                 constant);
      set_program_state_uniform4fv (program_state,
                                    unit_state->combine_constant_uniform,
                                    1, constant);
      unit_state->dirty_combine_constant = FALSE;
    }

  if (unit_state->texture_matrix_uniform != NULL &&
      (state->update_all || unit_state->dirty_texture_matrix))
    {
      const CoglMatrix *matrix;
      const float *array;

      matrix = _cogl_pipeline_get_layer_matrix (pipeline, layer_index);
      array = cogl_matrix_get_array (matrix);
      set_program_state_uniform_matrix4fv (program_state,
                                           unit_state->texture_matrix_uniform,
                                           1, array);
      unit_state->dirty_texture_matrix = FALSE;
    }

  return TRUE;
}

static void
update_builtin_uniforms (CoglContext *context,
                         CoglPipeline *pipeline,
                         CoglPipelineProgramState *program_state)
{
  int i;

  if (program_state->dirty_builtin_uniforms == 0)
    return;

  for (i = 0; i < G_N_ELEMENTS (builtin_uniforms); i++)
    if (!_cogl_has_private_feature (context,
                                    builtin_uniforms[i].feature_replacement) &&
        (program_state->dirty_builtin_uniforms & (1 << i)) &&
        program_state->builtin_uniform_locations[i] != NULL)
      builtin_uniforms[i].update_func (pipeline,
                                       program_state
                                       ->builtin_uniform_locations[i],
                                       builtin_uniforms[i].getter_func);

  program_state->dirty_builtin_uniforms = 0;
}

typedef struct
{
  CoglPipelineProgramState *program_state;
  unsigned long *uniform_differences;
  int n_differences;
  CoglContext *ctx;
  const CoglBoxedValue *values;
  int value_index;
} FlushUniformsClosure;

static CoglBool
flush_uniform_cb (int uniform_num, void *user_data)
{
  FlushUniformsClosure *data = user_data;

  if (COGL_FLAGS_GET (data->uniform_differences, uniform_num))
    {
      GArray *uniform_locations;
      CoglShaderVulkanUniform *uniform_location;

      if (data->program_state->uniform_locations == NULL)
        data->program_state->uniform_locations =
          g_array_new (FALSE, FALSE, sizeof (CoglShaderVulkanUniform *));

      uniform_locations = data->program_state->uniform_locations;

      if (uniform_locations->len <= uniform_num)
        {
          unsigned int old_len = uniform_locations->len;

          g_array_set_size (uniform_locations, uniform_num + 1);

          while (old_len <= uniform_num)
            {
              g_array_index (uniform_locations,
                             CoglShaderVulkanUniform*,
                             old_len) = NULL;
              old_len++;
            }
        }

      uniform_location = g_array_index (uniform_locations,
                                        CoglShaderVulkanUniform *,
                                        uniform_num);

      if (uniform_location == NULL)
        {
          const char *uniform_name =
            g_ptr_array_index (data->ctx->uniform_names, uniform_num);

          uniform_location =
            get_program_state_uniform_location (data->program_state, uniform_name);
          g_array_index (uniform_locations,
                         CoglShaderVulkanUniform *,
                         uniform_num) = uniform_location;
        }

      if (uniform_location != NULL)
        {
          const CoglBoxedValue *v = &data->values[data->value_index];

          set_program_state_uniform (data->program_state,
                                     uniform_location,
                                     _cogl_boxed_value_get_pointer (v),
                                     _cogl_boxed_value_get_size (v));
        }

      data->n_differences--;
      COGL_FLAGS_SET (data->uniform_differences, uniform_num, FALSE);
    }

  data->value_index++;

  return data->n_differences > 0;
}

static void
_cogl_pipeline_progend_vulkan_flush_uniforms (CoglPipeline *pipeline,
                                            CoglPipelineProgramState *
                                                                  program_state,
                                            CoglBool program_changed)
{
  CoglPipelineUniformsState *uniforms_state;
  FlushUniformsClosure data;
  int n_uniform_longs;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (pipeline->differences & COGL_PIPELINE_STATE_UNIFORMS)
    uniforms_state = &pipeline->big_state->uniforms_state;
  else
    uniforms_state = NULL;

  data.program_state = program_state;
  data.ctx = ctx;

  n_uniform_longs = COGL_FLAGS_N_LONGS_FOR_SIZE (ctx->n_uniform_names);

  data.uniform_differences = g_newa (unsigned long, n_uniform_longs);

  /* Try to find a common ancestor for the values that were already
     flushed on the pipeline that this program state was last used for
     so we can avoid flushing those */

  if (program_changed || program_state->last_used_for_pipeline == NULL)
    {
      if (program_changed)
        {
          /* The program has changed so all of the uniform locations are
             invalid */
          if (program_state->uniform_locations)
            g_array_set_size (program_state->uniform_locations, 0);
        }

      /* We need to flush everything so mark all of the uniforms as dirty */
      memset (data.uniform_differences, 0xff,
              n_uniform_longs * sizeof (unsigned long));
      data.n_differences = G_MAXINT;
    }
  else if (program_state->last_used_for_pipeline)
    {
      int i;

      memset (data.uniform_differences, 0,
              n_uniform_longs * sizeof (unsigned long));
      _cogl_pipeline_compare_uniform_differences
        (data.uniform_differences,
         program_state->last_used_for_pipeline,
         pipeline);

      /* We need to be sure to flush any uniforms that have changed
         since the last flush */
      if (uniforms_state)
        _cogl_bitmask_set_flags (&uniforms_state->changed_mask,
                                 data.uniform_differences);

      /* Count the number of differences. This is so we can stop early
         when we've flushed all of them */
      data.n_differences = 0;

      for (i = 0; i < n_uniform_longs; i++)
        data.n_differences +=
          _cogl_util_popcountl (data.uniform_differences[i]);
    }

  while (pipeline && data.n_differences > 0)
    {
      if (pipeline->differences & COGL_PIPELINE_STATE_UNIFORMS)
        {
          const CoglPipelineUniformsState *parent_uniforms_state =
            &pipeline->big_state->uniforms_state;

          data.values = parent_uniforms_state->override_values;
          data.value_index = 0;

          _cogl_bitmask_foreach (&parent_uniforms_state->override_mask,
                                 flush_uniform_cb,
                                 &data);
        }

      pipeline = _cogl_pipeline_get_parent (pipeline);
    }

  if (uniforms_state)
    _cogl_bitmask_clear_all (&uniforms_state->changed_mask);
}

typedef struct
{
  CoglPipelineProgramState *program_state;
  int n_units;
  int n_bindings;

  VkShaderStageFlags stage_flags;
  VkDescriptorSetLayoutBinding *bindings;
} CreateDescriptorSetLayout;

static CoglBool
add_layer_to_descriptor_set_layout (CoglPipelineLayer *layer,
                                    void *user_data)
{
  CreateDescriptorSetLayout *data = user_data;
  VkDescriptorSetLayoutBinding *binding = &data->bindings[data->n_bindings];
  UnitState *unit_state = &data->program_state->unit_state[data->n_units];

  _COGL_GET_CONTEXT (ctx, FALSE);

  /* We can reuse the source buffer to create the uniform name because
     the program has now been linked */
  g_string_set_size (ctx->codegen_source_buffer, 0);
  g_string_append_printf (ctx->codegen_source_buffer,
                          "cogl_sampler%i", layer->index);

  binding->binding =
    unit_state->binding =
    get_program_state_sampler_binding (data->program_state,
                                       ctx->codegen_source_buffer->str);
  binding->descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  binding->descriptorCount = 1;
  binding->stageFlags = data->stage_flags;
  binding->pImmutableSamplers = NULL;

  COGL_NOTE (VULKAN, "Descriptor layout cogl_sampler%i binding=%i",
             layer->index, unit_state->binding);

  if (binding->binding == -1)
    return FALSE;

  data->n_bindings++;
  data->n_units++;

  return TRUE;
}

static void
_cogl_pipeline_create_descriptor_set_layout (CoglPipeline *pipeline,
                                             CoglPipelineProgramState *program_state,
                                             CoglContext *ctx)
{
  CoglContextVulkan *vk_ctx = ctx->winsys;
  VkDescriptorSetLayoutCreateInfo info;
  CreateDescriptorSetLayout data;

  memset (&data, 0, sizeof (data));
  data.bindings = g_new0 (VkDescriptorSetLayoutBinding,
                          cogl_pipeline_get_n_layers (pipeline) + 2);
  data.program_state = program_state;

  /* Uniform buffers for all our uniforms. */
  data.bindings[data.n_bindings].binding = 0;
  data.bindings[data.n_bindings].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  data.bindings[data.n_bindings].descriptorCount = 1;
  data.bindings[data.n_bindings].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  data.bindings[data.n_bindings].pImmutableSamplers = NULL;
  data.n_bindings++;

  data.bindings[data.n_bindings].binding = 1;
  data.bindings[data.n_bindings].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  data.bindings[data.n_bindings].descriptorCount = 1;
  data.bindings[data.n_bindings].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  data.bindings[data.n_bindings].pImmutableSamplers = NULL;
  data.n_bindings++;

  /* All other potential samplers for each layer. */
  data.stage_flags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  _cogl_pipeline_foreach_layer_internal (pipeline,
                                         add_layer_to_descriptor_set_layout,
                                         &data);

  memset (&info, 0, sizeof (info));
  info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  info.bindingCount = data.n_bindings;
  info.pBindings = data.bindings;

  VK_RET ( ctx,
           vkCreateDescriptorSetLayout (vk_ctx->device, &info,
                                        NULL,
                                        &program_state->descriptor_set_layout) );
}

typedef struct
{
  CoglContext *context;
  CoglPipelineProgramState *program_state;
  CoglPipeline *pipeline;
} FlushDescriptors;

static CoglBool
compare_layer_differences_cb (CoglPipelineLayer *layer, void *user_data)
{
  FlushDescriptors *data = user_data;
  CoglContext *context = data->context;
  CoglPipelineProgramState *program_state = data->program_state;
  int unit_index = _cogl_pipeline_layer_get_unit_index (layer);
  UnitState *unit_state = &program_state->unit_state[unit_index];
  const CoglSamplerCacheEntry *sampler_entry =
    _cogl_pipeline_layer_get_sampler_state (layer);
  CoglTexture *texture = _cogl_pipeline_layer_get_texture (layer);
  VkSampler vk_sampler = (VkSampler) sampler_entry->winsys;

  /* TODO: We only support 2D texture for now. */
  g_assert (layer->texture_type == COGL_TEXTURE_TYPE_2D);

  if (!texture)
    texture = COGL_TEXTURE (context->default_gl_texture_2d_tex);

  if (unit_state->sampler != vk_sampler ||
      unit_state->image_view != _cogl_texture_get_vulkan_image_view (texture))
    {
      int index = program_state->n_write_descriptor_sets++;
      VkWriteDescriptorSet *write_set =
        &program_state->write_descriptor_sets[index];
      VkDescriptorImageInfo *image_info =
        &program_state->descriptor_image_infos[index];

      COGL_NOTE (VULKAN,
                 "Writing descriptor set cogl_sampler%i unit=%i binding=%i",
                 layer->index, unit_index, unit_state->binding);

      write_set->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      write_set->dstSet = program_state->descriptor_set;
      write_set->dstBinding = unit_state->binding;
      write_set->descriptorCount = 1;
      write_set->descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      write_set->pImageInfo = image_info;

      image_info->sampler = vk_sampler;
      image_info->imageView = _cogl_texture_get_vulkan_image_view (texture);
      image_info->imageLayout = _cogl_texture_get_vulkan_image_layout (texture);

      /* Update unit state for layer flushes. */
      unit_state->sampler = image_info->sampler;
      unit_state->image_view = image_info->imageView;
    }

  return TRUE;
}

void
_cogl_pipeline_progend_flush_descriptors (CoglContext *context,
                                          CoglPipeline *pipeline)
{
  FlushDescriptors data = {
    .context = context,
    .program_state = get_program_state (pipeline),
    .pipeline = pipeline,
  };

  _cogl_pipeline_foreach_layer_internal (pipeline,
                                         compare_layer_differences_cb,
                                         &data);
}

void
_cogl_pipeline_progend_discard_pipeline (CoglPipeline *pipeline)
{
  CoglPipelineProgramState *program_state = get_program_state (pipeline);

  if (program_state)
    g_hash_table_remove (program_state->pipelines, pipeline);
}

static CoglBool
_cogl_pipeline_progend_vulkan_start (CoglPipeline *pipeline)
{
  return TRUE;
}

static void
_cogl_pipeline_progend_vulkan_end (CoglPipeline *pipeline,
                                 unsigned long pipelines_difference)
{
  CoglContextVulkan *vk_ctx;
  CoglPipelineProgramState *program_state;
  CoglBool program_changed = FALSE;
  UpdateUniformsState state;
  CoglPipelineCacheEntry *cache_entry = NULL;
  int i;

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  vk_ctx = ctx->winsys;

  program_state = get_program_state (pipeline);

  if (G_UNLIKELY (cogl_pipeline_get_user_program (pipeline)))
    g_warning ("The Vulkan backend doesn't support legacy user program");

  if (program_state == NULL)
    {
      CoglPipeline *authority;

      /* Get the authority for anything affecting program state. This
         should include both fragment codegen state and vertex codegen
         state */
      authority = _cogl_pipeline_find_equivalent_parent
        (pipeline,
         (_cogl_pipeline_get_state_for_vertex_codegen (ctx) |
          _cogl_pipeline_get_state_for_fragment_codegen (ctx)) &
         ~COGL_PIPELINE_STATE_LAYERS,
         _cogl_pipeline_get_layer_state_for_fragment_codegen (ctx) |
         COGL_PIPELINE_LAYER_STATE_AFFECTS_VERTEX_CODEGEN);

      program_state = get_program_state (authority);

      if (program_state == NULL)
        {
          /* Check if there is already a similar cached pipeline whose
             program state we can share */
          if (G_LIKELY (!(COGL_DEBUG_ENABLED
                          (COGL_DEBUG_DISABLE_PROGRAM_CACHES))))
            {
              cache_entry =
                _cogl_pipeline_cache_get_combined_template (ctx->pipeline_cache,
                                                            authority);

              program_state = get_program_state (cache_entry->pipeline);
            }

          if (program_state)
            program_state->ref_count++;
          else
            program_state
              = program_state_new (cogl_pipeline_get_n_layers (authority),
                                   cache_entry);

          set_program_state (authority, program_state);

          program_state->ref_count--;

          if (cache_entry)
            set_program_state (cache_entry->pipeline, program_state);
        }

      if (authority != pipeline)
        set_program_state (pipeline, program_state);
    }

  if (program_state->shader == NULL)
    {
      GString *source;

      program_state->shader = _cogl_shader_vulkan_new (ctx);

      source = _cogl_pipeline_vertend_vulkan_get_shader (pipeline);
      _cogl_shader_vulkan_set_source (program_state->shader,
                                      COGL_GLSL_SHADER_TYPE_VERTEX,
                                      source->str);

      source = _cogl_pipeline_fragend_vulkan_get_shader (pipeline);
      _cogl_shader_vulkan_set_source (program_state->shader,
                                      COGL_GLSL_SHADER_TYPE_FRAGMENT,
                                      source->str);

      if (!_cogl_shader_vulkan_link (program_state->shader))
        {
          g_warning ("Vertex shader compilation failed");
          _cogl_shader_vulkan_free (program_state->shader);
          program_state->shader = NULL;
        }
    }

  /* Allocate uniform buffers for vertex & fragment shaders. */
  for (i = 0; i < COGL_SHADER_VULKAN_NB_STAGES; i++)
    {
      if (program_state->uniform_buffers[i] == NULL)
        {
          int block_size =
            _cogl_shader_vulkan_get_uniform_block_size (program_state->shader,
                                                        i, 0);

          program_state->uniform_buffers[i] =
            COGL_BUFFER (cogl_uniform_buffer_new (ctx, block_size));
          program_state->uniform_datas[i] =
            _cogl_buffer_map (program_state->uniform_buffers[i],
                              COGL_BUFFER_ACCESS_WRITE,
                              COGL_BUFFER_MAP_HINT_DISCARD,
                              NULL);

          COGL_NOTE (VULKAN, "Uniform buffer alloc stage=%i size=%i",
                     i, block_size);
        }
    }

  if (program_state->pipeline_layout == VK_NULL_HANDLE)
    {
      VkPipelineLayoutCreateInfo layout_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
      };

      _cogl_pipeline_create_descriptor_set_layout (pipeline,
                                                   program_state,
                                                   ctx);

      layout_create_info.pSetLayouts = &program_state->descriptor_set_layout;
      VK_RET ( ctx,
               vkCreatePipelineLayout (vk_ctx->device, &layout_create_info,
                                       NULL,
                                       &program_state->pipeline_layout) );

      program_state->stage_info[0] = (VkPipelineShaderStageCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = _cogl_shader_vulkan_get_shader_module (program_state->shader,
                                                         COGL_GLSL_SHADER_TYPE_VERTEX),
        .pName = "main",
      };
      program_state->stage_info[1] = (VkPipelineShaderStageCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = _cogl_shader_vulkan_get_shader_module (program_state->shader,
                                                         COGL_GLSL_SHADER_TYPE_FRAGMENT),
        .pName = "main",
      };

      program_changed = TRUE;
    }

  if (program_state->descriptor_pool == VK_NULL_HANDLE)
    {
      int n_samplers =
        _cogl_shader_vulkan_get_n_samplers (program_state->shader,
                                            COGL_GLSL_SHADER_TYPE_FRAGMENT);
      const VkDescriptorPoolCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = NULL,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 1,
        .poolSizeCount = n_samplers > 0 ? 2 : 1,
        .pPoolSizes = (VkDescriptorPoolSize[]) {
          {
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = COGL_SHADER_VULKAN_NB_STAGES,
          },
          {
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = n_samplers,
          },
        }
      };

      VK_RET ( ctx,
               vkCreateDescriptorPool (vk_ctx->device, &create_info,
                                       NULL, &program_state->descriptor_pool) );

    }

  if (program_state->descriptor_set == VK_NULL_HANDLE)
    {
      VkDescriptorSetAllocateInfo allocate_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = program_state->descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &program_state->descriptor_set_layout,
      };

      VK_RET ( ctx,
               vkAllocateDescriptorSets (vk_ctx->device, &allocate_info,
                                         &program_state->descriptor_set) );
    }

  state.unit = 0;
  state.program_state = program_state;

  if (program_changed)
    {
      cogl_pipeline_foreach_layer (pipeline,
                                   get_uniform_cb,
                                   &state);
    }

  state.unit = 0;
  state.update_all = (program_changed ||
                      program_state->last_used_for_pipeline != pipeline);

  cogl_pipeline_foreach_layer (pipeline,
                               update_constants_cb,
                               &state);

  if (program_changed)
    {
      int i;

      clear_flushed_matrix_stacks (program_state);

      for (i = 0; i < G_N_ELEMENTS (builtin_uniforms); i++)
        if (!_cogl_has_private_feature
            (ctx, builtin_uniforms[i].feature_replacement))
          program_state->builtin_uniform_locations[i] =
            get_program_state_uniform_location (program_state,
                                                builtin_uniforms[i].uniform_name);

      program_state->modelview_uniform =
        get_program_state_uniform_location (program_state, "cogl_modelview_matrix");

      program_state->projection_uniform =
        get_program_state_uniform_location (program_state, "cogl_projection_matrix");

      program_state->mvp_uniform =
        get_program_state_uniform_location (program_state,
                                            "cogl_modelview_projection_matrix");
    }

  if (program_changed ||
      program_state->last_used_for_pipeline != pipeline)
    program_state->dirty_builtin_uniforms = ~(unsigned long) 0;

  update_builtin_uniforms (ctx, pipeline, program_state);

  _cogl_pipeline_progend_vulkan_flush_uniforms (pipeline,
                                                program_state,
                                                program_changed);

  /* We need to track the last pipeline that the program was used with
   * so know if we need to update all of the uniforms */
  program_state->last_used_for_pipeline = pipeline;
}

static void
_cogl_pipeline_progend_vulkan_pre_change_notify (CoglPipeline *pipeline,
                                                 CoglPipelineState change,
                                                 const CoglColor *new_color)
{
  CoglPipelineProgramState *program_state = get_program_state (pipeline);

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  _cogl_pipeline_vulkan_pre_change_notify (pipeline, change);

  if (!program_state)
    return;

  if (change & (_cogl_pipeline_get_state_for_vertex_codegen (ctx) |
                _cogl_pipeline_get_state_for_fragment_codegen (ctx)))
    {
      dirty_program_state (pipeline);
    }
  else
    {
      int i;

      for (i = 0; i < G_N_ELEMENTS (builtin_uniforms); i++)
        if (!_cogl_has_private_feature
            (ctx, builtin_uniforms[i].feature_replacement) &&
            (change & builtin_uniforms[i].change))
          {
            program_state->dirty_builtin_uniforms |= 1 << i;
            return;
          }
    }
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
_cogl_pipeline_progend_vulkan_layer_pre_change_notify (
                                                CoglPipeline *owner,
                                                CoglPipelineLayer *layer,
                                                CoglPipelineLayerState change)
{
  CoglPipelineProgramState *program_state = get_program_state (owner);
  int unit_index = _cogl_pipeline_layer_get_unit_index (layer);

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  if (!program_state)
    return;

  if ((change & (_cogl_pipeline_get_layer_state_for_fragment_codegen (ctx) |
                 COGL_PIPELINE_LAYER_STATE_AFFECTS_VERTEX_CODEGEN)))
    {
      dirty_program_state (owner);
    }
  else
    {
      if (change & COGL_PIPELINE_LAYER_STATE_COMBINE_CONSTANT)
        program_state->unit_state[unit_index].dirty_combine_constant = TRUE;
      if (change & COGL_PIPELINE_LAYER_STATE_USER_MATRIX)
        program_state->unit_state[unit_index].dirty_texture_matrix = TRUE;
      if (change & COGL_PIPELINE_LAYER_STATE_SAMPLER)
        program_state->unit_state[unit_index].sampler = VK_NULL_HANDLE;
      if (change & COGL_PIPELINE_LAYER_STATE_TEXTURE_DATA)
        program_state->unit_state[unit_index].image_view = VK_NULL_HANDLE;
    }
}

static void
_cogl_pipeline_progend_write_descriptors (CoglPipelineProgramState *program_state,
                                          CoglFramebuffer *framebuffer)
{
  CoglContext *ctx = framebuffer->context;
  CoglContextVulkan *vk_ctx = ctx->winsys;
  VkDescriptorBufferInfo descriptor_buffer_info[COGL_SHADER_VULKAN_NB_STAGES];
  int i;

  for (i = 0; i < COGL_SHADER_VULKAN_NB_STAGES; i++)
    {
      CoglBuffer *buffer = program_state->uniform_buffers[i];
      CoglBufferVulkan *vk_uniform_buffer = buffer->winsys;
      VkWriteDescriptorSet *write_set =
        &program_state->write_descriptor_sets[program_state->n_write_descriptor_sets];

      if (!program_state->uniforms_dirty[i])
        continue;

      write_set->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      write_set->dstSet = program_state->descriptor_set;
      write_set->dstBinding = i;
      write_set->dstArrayElement = 0;
      write_set->descriptorCount = 1;
      write_set->descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      write_set->pBufferInfo = &descriptor_buffer_info[i];

      descriptor_buffer_info[i].buffer = vk_uniform_buffer->buffer;
      descriptor_buffer_info[i].offset = 0;
      descriptor_buffer_info[i].range = buffer->size;

      program_state->n_write_descriptor_sets++;

      program_state->uniforms_dirty[i] = FALSE;
    }

  if (program_state->n_write_descriptor_sets > 0)
    {
      program_state_end (program_state);
      _cogl_framebuffer_vulkan_begin_render_pass (framebuffer);


      VK ( ctx,
           vkUpdateDescriptorSets (vk_ctx->device,
                                   program_state->n_write_descriptor_sets,
                                   program_state->write_descriptor_sets,
                                   0, NULL) );
      program_state->n_write_descriptor_sets = 0;
    }
}

static void
_cogl_pipeline_progend_vulkan_pre_paint (CoglPipeline *pipeline,
                                         CoglFramebuffer *framebuffer)
{
  CoglContext *ctx = framebuffer->context;
  CoglContextVulkan *vk_ctx = ctx->winsys;
  CoglMatrixEntry *projection_entry;
  CoglMatrixEntry *modelview_entry;
  CoglPipelineProgramState *program_state;
  CoglBool modelview_changed;
  CoglBool projection_changed;
  CoglBool need_modelview;
  CoglBool need_projection;
  CoglMatrix modelview, projection;

  program_state = get_program_state (pipeline);

  g_assert (program_state->uniform_buffers[0]);

  projection_entry = ctx->current_projection_entry;
  modelview_entry = ctx->current_modelview_entry;

  /* An initial pipeline is flushed while creating the context. At
     this point there are no matrices selected so we can't do
     anything */
  if (modelview_entry == NULL || projection_entry == NULL)
    return;

  projection_changed =
    _cogl_matrix_entry_cache_maybe_update (&program_state->projection_cache,
                                           projection_entry,
                                           FALSE);

  modelview_changed =
    _cogl_matrix_entry_cache_maybe_update (&program_state->modelview_cache,
                                           modelview_entry,
                                           FALSE);

  if (modelview_changed || projection_changed)
    {
      if (program_state->mvp_uniform != NULL)
        need_modelview = need_projection = TRUE;
      else
        {
          need_projection = (program_state->projection_uniform != NULL &&
                             projection_changed);
          need_modelview = (program_state->modelview_uniform != NULL &&
                            modelview_changed);
        }

      if (need_modelview)
        cogl_matrix_entry_get (modelview_entry, &modelview);

      {
        CoglMatrix tmp_projection;
        cogl_matrix_entry_get (projection_entry, &tmp_projection);
        cogl_matrix_multiply (&projection, &vk_ctx->mat, &tmp_projection);
      }

      if (projection_changed && program_state->projection_uniform != NULL)
        set_program_state_uniform_matrix4fv (program_state,
                                             program_state->projection_uniform,
                                             1, /* count */
                                             cogl_matrix_get_array (&projection));

      if (modelview_changed && program_state->modelview_uniform != NULL)
        set_program_state_uniform_matrix4fv (program_state,
                                             program_state->modelview_uniform,
                                             1, /* count */
                                             cogl_matrix_get_array (&modelview));

      if (program_state->mvp_uniform != NULL)
        {
          /* The journal usually uses an identity matrix for the
             modelview so we can optimise this common case by
             avoiding the matrix multiplication */
          if (cogl_matrix_entry_is_identity (modelview_entry))
            {
              set_program_state_uniform_matrix4fv (program_state,
                                                   program_state->mvp_uniform,
                                                   1, /* count */
                                                   cogl_matrix_get_array (&projection));
            }
          else
            {
              CoglMatrix combined;

              cogl_matrix_multiply (&combined,
                                    &projection,
                                    &modelview);

              set_program_state_uniform_matrix4fv (program_state,
                                                   program_state->mvp_uniform,
                                                   1, /* count */
                                                   cogl_matrix_get_array (&combined));
            }
        }
    }

  _cogl_pipeline_progend_write_descriptors (program_state, framebuffer);

  g_hash_table_insert (program_state->pipelines, pipeline, pipeline);
}

static void
update_float_uniform (CoglPipeline *pipeline,
                      CoglShaderVulkanUniform *location,
                      void *getter_func)
{
  float (* float_getter_func) (CoglPipeline *) = getter_func;
  float value;
  CoglPipelineProgramState *program_state = get_program_state (pipeline);

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);

  value = float_getter_func (pipeline);
  set_program_state_uniform1f (program_state, location, value);
}

const CoglPipelineProgend _cogl_pipeline_vulkan_progend =
  {
    COGL_PIPELINE_VERTEND_VULKAN,
    COGL_PIPELINE_FRAGEND_VULKAN,
    _cogl_pipeline_progend_vulkan_start,
    _cogl_pipeline_progend_vulkan_end,
    _cogl_pipeline_progend_vulkan_pre_change_notify,
    _cogl_pipeline_progend_vulkan_layer_pre_change_notify,
    _cogl_pipeline_progend_vulkan_pre_paint
  };
