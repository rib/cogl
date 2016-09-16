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
 *
 */

#include "config.h"

#include "cogl-debug.h"
#include "cogl-buffer-private.h"
#include "cogl-buffer-vulkan-private.h"
#include "cogl-context-private.h"
#include "cogl-driver-vulkan-private.h"
#include "cogl-framebuffer-private.h"
#include "cogl-framebuffer-vulkan-private.h"
#include "cogl-pipeline-private.h"
#include "cogl-pipeline-fragend-vulkan-private.h"
#include "cogl-pipeline-progend-vulkan-private.h"
#include "cogl-pipeline-vertend-vulkan-private.h"
#include "cogl-pipeline-vulkan-private.h"
#include "cogl-texture-private.h"
#include "cogl-util-vulkan-private.h"

#include <test-fixtures/test-unit.h>

#include <glib.h>
#include <string.h>

typedef struct _CoglPipelineVulkan
{
  VkPipeline pipeline;

  VkPipelineVertexInputStateCreateInfo *vertex_inputs;
  int n_user_vertex_inputs;
  int n_vertex_inputs;

  VkBuffer *attribute_buffers; /* VkBuffer elements are not owned */
  VkDeviceSize *attribute_offsets;

  /* A list of buffer to reference in the framebuffer until GPU has
     completed work. */
  CoglBuffer **seen_attribute_buffers;
  int n_seen_attribute_buffers;

  CoglColorMask color_mask;
  CoglVerticesMode vertices_mode;

  /* Color & depth format of the last */
  VkFormat color_format;
  VkFormat depth_format;

  /* Holds the color of a pipeline if required. */
  CoglBuffer *color_buffer;

  /* List of framebuffers on which the GPU is currently working and using
     this pipeline. */
  GHashTable *framebuffers;
} CoglPipelineVulkan;

static CoglUserDataKey vk_pipeline_key;

typedef struct
{
  CoglAttributeNameID name_id;
  const char *name;
  size_t offset;
  VkFormat vk_format;
} DefaultBuiltinAttribute;

typedef struct
{
  /* Leave the color attribute at the beginning as we're storing it in a
     separate buffer. */
  struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
  } cogl_color_in;
  struct {
    float x;
    float y;
    float z;
  } cogl_normal_in;
  struct {
    float x;
    float y;
    float s;
    float t;
  } cogl_tex_coord_in;
} DefaultBuiltinAttributeValues;

static const DefaultBuiltinAttributeValues default_attributes_values = {
  .cogl_color_in = { 1.0, 1.0, 1.0, 1.0 },
  .cogl_normal_in = { 0.0, 0.0, 1.0 },
  .cogl_tex_coord_in = { 0.0, 0.0, 0.0, 0.0 },
};

static DefaultBuiltinAttribute default_attributes[] =
  {
    {
      COGL_ATTRIBUTE_NAME_ID_COLOR_ARRAY,
      "cogl_color_in",
      G_STRUCT_OFFSET (DefaultBuiltinAttributeValues, cogl_color_in),
      VK_FORMAT_R8G8B8A8_UNORM,
    },
    {
      COGL_ATTRIBUTE_NAME_ID_NORMAL_ARRAY,
      "cogl_normal_in",
      G_STRUCT_OFFSET (DefaultBuiltinAttributeValues, cogl_normal_in),
      VK_FORMAT_R32G32B32_SFLOAT,
    },
    {
      COGL_ATTRIBUTE_NAME_ID_TEXTURE_COORD_ARRAY,
      "_cogl_tex_coord0_in",
      G_STRUCT_OFFSET (DefaultBuiltinAttributeValues, cogl_tex_coord_in),
      VK_FORMAT_R32G32B32A32_SFLOAT,
    }
  };

CoglBuffer *
_cogl_pipeline_ensure_default_attributes (CoglContext *ctx)
{
  CoglContextVulkan *vk_ctx = ctx->winsys;
  void *data;

  vk_ctx->default_attributes =
    COGL_BUFFER (cogl_attribute_buffer_new_with_size (ctx, sizeof (default_attributes_values)));

  data = cogl_buffer_map (vk_ctx->default_attributes,
                          COGL_BUFFER_ACCESS_WRITE,
                          COGL_BUFFER_MAP_HINT_DISCARD);
  memcpy (data, &default_attributes_values, vk_ctx->default_attributes->size);
  cogl_buffer_unmap (vk_ctx->default_attributes);
}

static CoglPipelineVulkan *
get_vk_pipeline (CoglPipeline *pipeline)
{
  return cogl_object_get_user_data (COGL_OBJECT (pipeline), &vk_pipeline_key);
}

static void
_cogl_pipeline_vulkan_end_internal (CoglPipeline *pipeline,
                                    CoglPipelineVulkan *vk_pipeline,
                                    CoglBool keep_alive)
{
  CoglFramebuffer *framebuffer;
  GHashTableIter iter;

  if (g_hash_table_size (vk_pipeline->framebuffers) < 1)
    return;

  if (keep_alive)
    cogl_object_ref (pipeline);

  g_hash_table_iter_init (&iter, vk_pipeline->framebuffers);
  while (g_hash_table_iter_next (&iter, (gpointer *) &framebuffer, NULL)) {
    g_hash_table_iter_remove (&iter);
    _cogl_framebuffer_vulkan_end (framebuffer, TRUE);
  }

  if (keep_alive)
    cogl_object_unref (pipeline);
}

static void
_cogl_pipeline_vulkan_invalidate_internal (CoglPipeline *pipeline,
                                           CoglBool keep_alive)
{
  CoglContextVulkan *vk_ctx;
  CoglPipelineVulkan *vk_pipeline = get_vk_pipeline (pipeline);

  _COGL_GET_CONTEXT (ctx, NO_RETVAL);
  vk_ctx = ctx->winsys;

  if (!vk_pipeline)
    return;

  _cogl_pipeline_vulkan_end_internal (pipeline, vk_pipeline, keep_alive);

  if (vk_pipeline->pipeline != VK_NULL_HANDLE)
    {
      VK (ctx, vkDestroyPipeline (vk_ctx->device, vk_pipeline->pipeline,
                                  NULL) );
      vk_pipeline->pipeline = VK_NULL_HANDLE;
    }

  if (vk_pipeline->attribute_buffers)
    {
      g_free (vk_pipeline->attribute_buffers);
      vk_pipeline->attribute_buffers = NULL;
    }

  if (vk_pipeline->attribute_offsets)
    {
      g_free (vk_pipeline->attribute_offsets);
      vk_pipeline->attribute_offsets = NULL;
    }

  if (vk_pipeline->vertex_inputs)
    {
      g_free (vk_pipeline->vertex_inputs);
      vk_pipeline->vertex_inputs = NULL;
      vk_pipeline->n_user_vertex_inputs = 0;
      vk_pipeline->n_vertex_inputs = 0;
    }
}

static void
vk_pipeline_destroy (void *user_data,
                     void *instance)
{
  CoglPipelineVulkan *vk_pipeline = user_data;

  _cogl_pipeline_vulkan_invalidate_internal (instance, FALSE);

  if (vk_pipeline->color_buffer)
    {
      cogl_object_unref (vk_pipeline->color_buffer);
      vk_pipeline->color_buffer = NULL;
    }

  if (vk_pipeline->framebuffers)
    {
      g_hash_table_unref (vk_pipeline->framebuffers);
      vk_pipeline->framebuffers = NULL;
    }

  g_slice_free (CoglPipelineVulkan, vk_pipeline);
}

static CoglPipelineVulkan *
vk_pipeline_new (CoglPipeline *pipeline)
{
  CoglPipelineVulkan *vk_pipeline;

  vk_pipeline = g_slice_new0 (CoglPipelineVulkan);

  vk_pipeline->framebuffers = g_hash_table_new (g_direct_hash, g_direct_equal);

  _cogl_object_set_user_data (COGL_OBJECT (pipeline),
                              &vk_pipeline_key,
                              vk_pipeline,
                              vk_pipeline_destroy);

  return vk_pipeline;
}

void
_cogl_pipeline_vulkan_discard_framebuffer (CoglPipeline *pipeline,
                                           CoglFramebuffer *framebuffer)
{
  CoglPipelineVulkan *vk_pipeline = get_vk_pipeline (pipeline);

  if (vk_pipeline) {
    g_hash_table_remove (vk_pipeline->framebuffers, framebuffer);
    _cogl_pipeline_progend_discard_pipeline (pipeline);
  }
}

void
_cogl_pipeline_vulkan_end (CoglPipeline *pipeline)
{
  CoglPipelineVulkan *vk_pipeline = get_vk_pipeline (pipeline);

  if (vk_pipeline)
    _cogl_pipeline_vulkan_end_internal (pipeline, vk_pipeline, TRUE);
}

void
_cogl_pipeline_vulkan_invalidate (CoglPipeline *pipeline)
{
  _cogl_pipeline_vulkan_invalidate_internal (pipeline, TRUE);
}

typedef struct
{
  CoglFramebuffer *framebuffer;
  const CoglPipelineVertend *vertend;
  const CoglPipelineFragend *fragend;
  CoglPipeline *pipeline;
  CoglBool error_adding_layer;
} CoglPipelineAddLayerState;

static CoglBool
vertend_add_layer_cb (CoglPipelineLayer *layer,
                      void *user_data)
{
  CoglPipelineAddLayerState *state = user_data;
  const CoglPipelineVertend *vertend = state->vertend;
  CoglPipeline *pipeline = state->pipeline;

  /* Either generate per layer code snippets or setup the
   * fixed function glTexEnv for each layer... */
  if (G_LIKELY (!vertend->add_layer (pipeline,
                                     layer,
                                     0,
                                     state->framebuffer)))
    {
      state->error_adding_layer = TRUE;
      return FALSE;
    }

  return TRUE;
}

static CoglBool
fragend_add_layer_cb (CoglPipelineLayer *layer,
                      void *user_data)
{
  CoglPipelineAddLayerState *state = user_data;
  const CoglPipelineFragend *fragend = state->fragend;
  CoglPipeline *pipeline = state->pipeline;

  /* Either generate per layer code snippets or setup the
   * fixed function glTexEnv for each layer... */
  if (G_UNLIKELY (!fragend->add_layer (pipeline,
                                       layer,
                                       0)))
    {
      state->error_adding_layer = TRUE;
      return FALSE;
    }

  return TRUE;
}

static void
_cogl_pipeline_vulkan_update_color_buffer (CoglContext *ctx,
                                           CoglPipeline *pipeline,
                                           CoglPipelineVulkan *vk_pipeline)
{
  uint8_t colors[4];

  _cogl_pipeline_get_colorubv (pipeline, colors);

  if (!vk_pipeline->color_buffer)
    vk_pipeline->color_buffer =
      COGL_BUFFER (cogl_attribute_buffer_new (ctx, sizeof (colors), colors));
  else
    cogl_buffer_set_data (vk_pipeline->color_buffer, 0, colors, sizeof (colors));
}

static int
_get_attribute_unit_index (CoglPipeline *pipeline,
                           CoglAttribute *attribute)
{
  int layer_number = attribute->name_state->layer_number;
  CoglPipelineLayer *layer =
    _cogl_pipeline_get_layer_with_flags (pipeline,
                                         layer_number,
                                         COGL_PIPELINE_GET_LAYER_NO_CREATE);

  if (layer)
    return _cogl_pipeline_layer_get_unit_index (layer);

  return -1;
}

static const char *tex_coords_names[] = {
  "_cogl_tex_coord0_in",
  "_cogl_tex_coord1_in",
  "_cogl_tex_coord2_in",
  "_cogl_tex_coord3_in",
  "_cogl_tex_coord4_in",
  "_cogl_tex_coord5_in",
  "_cogl_tex_coord6_in",
  "_cogl_tex_coord7_in"
};

static int
_get_input_attribute_location (CoglShaderVulkan *shader,
                               CoglPipeline *pipeline,
                               CoglAttribute *attribute)
{
  int unit_index;
  if (attribute->name_state->name_id == COGL_ATTRIBUTE_NAME_ID_TEXTURE_COORD_ARRAY &&
      (unit_index = _get_attribute_unit_index (pipeline, attribute)) != -1)
    {
      char name[80];
      if (unit_index < G_N_ELEMENTS (tex_coords_names))
        return
          _cogl_shader_vulkan_get_input_attribute_location (shader,
                                                            COGL_GLSL_SHADER_TYPE_VERTEX,
                                                            tex_coords_names[unit_index]);

      g_snprintf (name, sizeof (name), "_cogl_tex_coord%i_in", unit_index);

      return
        _cogl_shader_vulkan_get_input_attribute_location (shader,
                                                          COGL_GLSL_SHADER_TYPE_VERTEX,
                                                          name);
    }

  return _cogl_shader_vulkan_get_input_attribute_location (shader,
                                                           COGL_GLSL_SHADER_TYPE_VERTEX,
                                                           attribute->name_state->name);
}

static void
_cogl_pipeline_vulkan_compute_attributes (CoglContext *ctx,
                                          CoglPipeline *pipeline,
                                          CoglPipelineVulkan *vk_pipeline,
                                          CoglFramebuffer *framebuffer,
                                          CoglAttribute **attributes,
                                          int n_user_attributes)
{
  CoglContextVulkan *vk_ctx = ctx->winsys;
  CoglFramebufferVulkan *vk_fb = framebuffer->winsys;
  CoglBufferVulkan *vk_buf_default_attributes =
    vk_ctx->default_attributes->winsys;
  CoglShaderVulkan *shader =
    _cogl_pipeline_progend_get_vulkan_shader (pipeline);
  int i, n_attributes, n_max_attributes = (n_user_attributes +
                                           G_N_ELEMENTS (default_attributes));
  void *ptr =
    g_malloc0 (sizeof (VkPipelineVertexInputStateCreateInfo) +
               n_max_attributes * (sizeof (VkVertexInputBindingDescription) +
                                   sizeof (VkVertexInputAttributeDescription)));
  uint32_t attributes_field = 0;
  VkPipelineVertexInputStateCreateInfo *info;
  const char **default_attributes_names =
    g_alloca (G_N_ELEMENTS (default_attributes) * sizeof (char *));

  info = ptr;
  info->pVertexBindingDescriptions = (VkVertexInputBindingDescription *)
    ((uint8_t *) ptr + sizeof (VkPipelineVertexInputStateCreateInfo));
  info->pVertexAttributeDescriptions = (VkVertexInputAttributeDescription *)
    ((uint8_t *) ptr + sizeof (VkPipelineVertexInputStateCreateInfo) +
     n_max_attributes * sizeof (VkVertexInputBindingDescription));

  info->sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

  /* Invalidate pipeline if number of user supplied attribute has
     changed. */
  if (vk_pipeline->n_user_vertex_inputs != 0 &&
      vk_pipeline->n_user_vertex_inputs != n_user_attributes)
    _cogl_pipeline_vulkan_invalidate (pipeline);

  if (vk_pipeline->attribute_buffers)
    {
      g_free (vk_pipeline->attribute_buffers);
      g_free (vk_pipeline->attribute_offsets);
      g_free (vk_pipeline->seen_attribute_buffers);
    }

  vk_pipeline->attribute_buffers =
    g_malloc (sizeof (VkBuffer) * n_max_attributes);
  vk_pipeline->attribute_offsets =
    g_malloc (sizeof (VkDeviceSize) * n_max_attributes);
  vk_pipeline->seen_attribute_buffers =
    g_malloc (sizeof (CoglBuffer *) * n_max_attributes);

  vk_pipeline->n_seen_attribute_buffers = 0;

  /* Process attributes given by the user. */
  for (i = 0; i < n_user_attributes; i++)
    {
      CoglAttribute *attribute = attributes[i];
      int location = _get_input_attribute_location (shader, pipeline, attribute);

      if (attribute->name_state->name_id != COGL_ATTRIBUTE_NAME_ID_CUSTOM_ARRAY)
        attributes_field |= 1 << attribute->name_state->name_id;

      if (attribute->is_buffered)
        {
          CoglBuffer *buffer = COGL_BUFFER (attribute->d.buffered.attribute_buffer);
          CoglBufferVulkan *vk_buf = buffer->winsys;

          VkVertexInputBindingDescription *vertex_bind =
            (VkVertexInputBindingDescription *) &info->pVertexBindingDescriptions[i];
          VkVertexInputAttributeDescription *vertex_desc =
            (VkVertexInputAttributeDescription *) &info->pVertexAttributeDescriptions[i];

          vertex_bind->binding = i;
          vertex_bind->stride = attribute->d.buffered.stride;
          vertex_bind->inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

          vertex_desc->location = location;
          vertex_desc->binding = i;
          vertex_desc->offset = 0;
          vertex_desc->format =
            _cogl_attribute_type_to_vulkan_format (attribute->d.buffered.type,
                                                   attribute->d.buffered.n_components);

          if (vk_pipeline->vertex_inputs &&
              (memcmp (&vk_pipeline->vertex_inputs->pVertexBindingDescriptions[i],
                       vertex_bind,
                       sizeof (VkVertexInputBindingDescription)) != 0 ||
               memcmp (&vk_pipeline->vertex_inputs->pVertexAttributeDescriptions[i],
                       vertex_desc,
                       sizeof (VkVertexInputAttributeDescription))))
            {
              COGL_NOTE (VULKAN,
                         "Invalidating pipeline because of user vertex "
                         "layout changed");
              _cogl_pipeline_vulkan_invalidate (pipeline);
            }

          vk_pipeline->attribute_buffers[i] = vk_buf->buffer;
          vk_pipeline->attribute_offsets[i] = attribute->d.buffered.offset;

          /* Only reference the buffer if it's a different one from the last. */
          if (vk_pipeline->n_seen_attribute_buffers == 0 ||
              vk_pipeline->seen_attribute_buffers[vk_pipeline->n_seen_attribute_buffers - 1] != buffer)
            vk_pipeline->seen_attribute_buffers[vk_pipeline->n_seen_attribute_buffers++] = buffer;
        }
      else
        {
          VK_TODO();
          g_assert_not_reached();
        }
    }

  /* Add any default missing GL attribute. */
  n_attributes = n_user_attributes;
  for (i = 0; i < G_N_ELEMENTS (default_attributes); i++)
    {
      VkVertexInputBindingDescription *vertex_bind =
        (VkVertexInputBindingDescription *) &info->pVertexBindingDescriptions[n_attributes];
      VkVertexInputAttributeDescription *vertex_desc =
        (VkVertexInputAttributeDescription *) &info->pVertexAttributeDescriptions[n_attributes];
      DefaultBuiltinAttribute *attribute = &default_attributes[i];
      CoglBufferVulkan *vk_buf = vk_buf_default_attributes;
      int location;

      if (_COGL_VULKAN_HAS_ATTRIBUTE (attributes_field, attribute->name_id))
        continue;

      location =
        _cogl_shader_vulkan_get_input_attribute_location (shader,
                                                          COGL_GLSL_SHADER_TYPE_VERTEX,
                                                          attribute->name);
      if (location == -1)
        continue;

      if (attribute->name_id == COGL_ATTRIBUTE_NAME_ID_COLOR_ARRAY)
        {
          _cogl_pipeline_vulkan_update_color_buffer (ctx, pipeline, vk_pipeline);
          vk_buf = vk_pipeline->color_buffer->winsys;
        }

      default_attributes_names[n_attributes - n_user_attributes] =
        attribute->name;

      vertex_bind->binding = n_attributes;
      vertex_bind->stride = 0;
      vertex_bind->inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

      vertex_desc->location = location;
      vertex_desc->binding = n_attributes;
      vertex_desc->offset = 0;
      vertex_desc->format = attribute->vk_format;

      vk_pipeline->attribute_buffers[n_attributes] = vk_buf->buffer;
      vk_pipeline->attribute_offsets[n_attributes] = attribute->offset;

      if (vk_pipeline->vertex_inputs &&
          (memcmp (&vk_pipeline->vertex_inputs->pVertexBindingDescriptions[n_attributes],
                   vertex_bind,
                   sizeof (VkVertexInputBindingDescription)) != 0 ||
           memcmp (&vk_pipeline->vertex_inputs->pVertexAttributeDescriptions[n_attributes],
                   vertex_desc,
                   sizeof (VkVertexInputAttributeDescription))))
        {
          COGL_NOTE (VULKAN,
                     "Invalidating pipeline because of default vertex "
                     "layout changed");
          _cogl_pipeline_vulkan_invalidate (pipeline);
        }

      n_attributes++;
    }

  info->vertexBindingDescriptionCount =
    info->vertexAttributeDescriptionCount = n_attributes;

  if (vk_pipeline->vertex_inputs == NULL)
    {
      if (G_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_VULKAN)))
        {
          for (i = 0; i < n_user_attributes; i++)
            {
              CoglAttribute *attribute = attributes[i];
              VkVertexInputAttributeDescription *vertex_desc =
                (VkVertexInputAttributeDescription *) &info->pVertexAttributeDescriptions[i];

              COGL_NOTE (VULKAN,
                         "User attribute '%s' location=%i offset=%lu"
                         " stride=%lu n_components=%i vk_format=%i",
                         attribute->name_state->name,
                         vertex_desc->location,
                         attribute->d.buffered.offset,
                         attribute->d.buffered.stride,
                         attribute->d.buffered.n_components,
                         vertex_desc->format);
            }
          for (i = n_user_attributes; i < n_attributes; i++)
            {
              VkVertexInputAttributeDescription *vertex_desc =
                (VkVertexInputAttributeDescription *) &info->pVertexAttributeDescriptions[i];

              COGL_NOTE (VULKAN,
                         "Default attribute '%s' location=%i vk_format=%i",
                         default_attributes_names[i - n_user_attributes],
                         vertex_desc->location, vertex_desc->format);
            }
        }

      vk_pipeline->vertex_inputs = info;
      vk_pipeline->n_user_vertex_inputs = n_user_attributes;
      vk_pipeline->n_vertex_inputs = n_attributes;
    }
  else
    g_free (info);
}

static void
_cogl_pipeline_vulkan_create_pipeline (CoglPipeline *pipeline,
                                       CoglPipelineVulkan *vk_pipeline,
                                       CoglFramebuffer *framebuffer)
{
  CoglContext *ctx = framebuffer->context;
  CoglContextVulkan *vk_ctx = ctx->winsys;
  CoglFramebufferVulkan *vk_fb = framebuffer->winsys;

  VkPipelineColorBlendStateCreateInfo vk_blend_state;
  VkPipelineColorBlendAttachmentState vk_blend_attachment_state;

  VkPipelineRasterizationStateCreateInfo vk_raster_state;

  VkPipelineDepthStencilStateCreateInfo vk_depth_state;

  VkPipelineViewportStateCreateInfo vk_viewport_state;
  VkViewport vk_viewport;
  VkRect2D vk_scissor;

  VkPipelineInputAssemblyStateCreateInfo input_assembly_state;

  VkPipelineMultisampleStateCreateInfo multisample_state;

  VkPipelineDynamicStateCreateInfo dynamic_state;
  VkDynamicState dynamic_states[2];

  VkGraphicsPipelineCreateInfo pipeline_state;

  if (vk_pipeline->pipeline != VK_NULL_HANDLE)
    return;

  vk_pipeline->color_mask = framebuffer->color_mask;
  vk_pipeline->vertices_mode = vk_fb->vertices_modes[vk_fb->n_vertices_modes - 1];
  vk_pipeline->color_format = vk_fb->color_format;
  vk_pipeline->depth_format = vk_fb->depth_format;

  /* Blending */
  {
    CoglPipeline *blend_authority =
      _cogl_pipeline_get_authority (pipeline, COGL_PIPELINE_STATE_BLEND);
    CoglPipelineBlendState *blend_state =
      &blend_authority->big_state->blend_state;

    memset (&vk_blend_state, 0, sizeof (vk_blend_state));
    memset (&vk_blend_attachment_state, 0, sizeof (vk_blend_attachment_state));

    vk_blend_state.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    vk_blend_state.attachmentCount = 1;
    vk_blend_state.pAttachments = &vk_blend_attachment_state;

    vk_blend_state.blendConstants[0] =
      cogl_color_get_red_float (&blend_state->blend_constant);
    vk_blend_state.blendConstants[1] =
      cogl_color_get_green_float (&blend_state->blend_constant);
    vk_blend_state.blendConstants[2] =
      cogl_color_get_blue_float (&blend_state->blend_constant);
    vk_blend_state.blendConstants[3] =
      cogl_color_get_alpha_float (&blend_state->blend_constant);

    vk_blend_attachment_state.blendEnable =
      _cogl_pipeline_get_blend_enabled (blend_authority) ==
      COGL_PIPELINE_BLEND_ENABLE_DISABLED ? VK_FALSE : VK_TRUE;
    vk_blend_attachment_state.srcColorBlendFactor =
      _cogl_blend_factor_to_vulkan_blend_factor (blend_state->blend_src_factor_rgb);
    vk_blend_attachment_state.dstColorBlendFactor =
      _cogl_blend_factor_to_vulkan_blend_factor (blend_state->blend_dst_factor_rgb);
    vk_blend_attachment_state.colorBlendOp =
      _cogl_blend_equation_to_vulkan_blend_op (blend_state->blend_equation_rgb);
    vk_blend_attachment_state.srcAlphaBlendFactor =
      _cogl_blend_factor_to_vulkan_blend_factor (blend_state->blend_src_factor_alpha);
    vk_blend_attachment_state.dstAlphaBlendFactor =
      _cogl_blend_factor_to_vulkan_blend_factor (blend_state->blend_dst_factor_alpha);
    vk_blend_attachment_state.alphaBlendOp =
      _cogl_blend_equation_to_vulkan_blend_op (blend_state->blend_equation_alpha);

    vk_blend_attachment_state.colorWriteMask =
      ((framebuffer->color_mask & COGL_COLOR_MASK_RED) ? VK_COLOR_COMPONENT_R_BIT : 0) |
      ((framebuffer->color_mask & COGL_COLOR_MASK_GREEN) ? VK_COLOR_COMPONENT_G_BIT : 0) |
      ((framebuffer->color_mask & COGL_COLOR_MASK_BLUE) ? VK_COLOR_COMPONENT_B_BIT : 0) |
      ((framebuffer->color_mask & COGL_COLOR_MASK_ALPHA) ? VK_COLOR_COMPONENT_A_BIT : 0);
  }

  /* Rastering */
  {
    memset (&vk_raster_state, 0, sizeof (vk_raster_state));

    vk_raster_state.sType =
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    vk_raster_state.rasterizerDiscardEnable = VK_FALSE;
    vk_raster_state.polygonMode =
      _cogl_vertices_mode_to_vulkan_polygon_mode (vk_pipeline->vertices_mode);
    vk_raster_state.cullMode =
      _cogl_cull_mode_to_vulkan_cull_mode (cogl_pipeline_get_cull_face_mode (pipeline));
    vk_raster_state.frontFace =
      _cogl_winding_to_vulkan_front_face (cogl_pipeline_get_front_face_winding (pipeline));

    vk_raster_state.lineWidth = 1.0;
  }

  /* Depth */
  {
    CoglPipeline *authority =
      _cogl_pipeline_get_authority (pipeline, COGL_PIPELINE_STATE_DEPTH);
    CoglDepthState *depth_state = &authority->big_state->depth_state;

    memset (&vk_depth_state, 0, sizeof (vk_depth_state));

    vk_depth_state.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    vk_depth_state.depthTestEnable =
      depth_state->test_enabled ? VK_TRUE : VK_FALSE;
    vk_depth_state.depthWriteEnable =
      depth_state->write_enabled ? VK_TRUE : VK_FALSE;
    vk_depth_state.depthCompareOp =
      _cogl_depth_test_function_to_vulkan_compare_op (depth_state->test_function);
    vk_depth_state.depthBoundsTestEnable = VK_FALSE;
    vk_depth_state.back.failOp = VK_STENCIL_OP_KEEP;
    vk_depth_state.back.passOp = VK_STENCIL_OP_KEEP;
    vk_depth_state.back.compareOp = VK_COMPARE_OP_ALWAYS;
    vk_depth_state.stencilTestEnable = VK_FALSE;
    vk_depth_state.front = vk_depth_state.back;
  }

  /* Viewport */
  {
    memset (&vk_viewport_state, 0, sizeof (vk_viewport_state));
    memset (&vk_viewport, 0, sizeof (vk_viewport));
    memset (&vk_scissor, 0, sizeof (vk_scissor));

    vk_viewport_state.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vk_viewport_state.viewportCount = 1;
    vk_viewport_state.pViewports = &vk_viewport;
    vk_viewport_state.scissorCount = 1;
    vk_viewport_state.pScissors = &vk_scissor;

    vk_viewport.x = 0;
    vk_viewport.y = 0;
    vk_viewport.width = framebuffer->width;
    vk_viewport.height = framebuffer->height;
    vk_viewport.minDepth = 0;
    vk_viewport.maxDepth = 1;

    vk_scissor.offset.x = 0;
    vk_scissor.offset.y = 0;
    vk_scissor.extent.width = framebuffer->width;
    vk_scissor.extent.height = framebuffer->height;
  }

  /* Input assembly */
  {
    memset (&input_assembly_state, 0, sizeof (input_assembly_state));

    input_assembly_state.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_state.topology =
      _cogl_vertices_mode_to_vulkan_primitive_topology (vk_pipeline->vertices_mode);
    input_assembly_state.primitiveRestartEnable = VK_FALSE;
  }

  /* Multisampling */
  {
    memset (&multisample_state, 0, sizeof (multisample_state));

    multisample_state.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample_state.rasterizationSamples = 1;
  }

  /* Dynamic */
  {
    memset (&dynamic_state, 0, sizeof (dynamic_state));

    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = 1;
    dynamic_state.pDynamicStates = dynamic_states;
    dynamic_states[0] = VK_DYNAMIC_STATE_VIEWPORT;
    dynamic_states[1] = VK_DYNAMIC_STATE_SCISSOR;
  }

  memset (&pipeline_state, 0, sizeof (pipeline_state));
  pipeline_state.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipeline_state.stageCount = 2;
  pipeline_state.pStages =
    _cogl_pipeline_progend_get_vulkan_stage_info (pipeline);
  pipeline_state.pVertexInputState = vk_pipeline->vertex_inputs;
  pipeline_state.pInputAssemblyState = &input_assembly_state;
  pipeline_state.pViewportState = &vk_viewport_state;
  pipeline_state.pRasterizationState = &vk_raster_state;
  pipeline_state.pMultisampleState = &multisample_state;
  pipeline_state.pDepthStencilState = &vk_depth_state;
  pipeline_state.pColorBlendState = &vk_blend_state;
  pipeline_state.pDynamicState = &dynamic_state;
  pipeline_state.flags = 0;
  pipeline_state.layout =
    _cogl_pipeline_progend_get_vulkan_pipeline_layout (pipeline);
  pipeline_state.renderPass = vk_fb->render_pass;
  pipeline_state.subpass = 0;
  pipeline_state.basePipelineHandle = NULL;
  pipeline_state.basePipelineIndex = -1;

  VK_RET ( ctx,
           vkCreateGraphicsPipelines (vk_ctx->device,
                                      (VkPipelineCache) { VK_NULL_HANDLE },
                                      1, &pipeline_state,
                                      NULL,
                                      &vk_pipeline->pipeline) );
}

void
_cogl_pipeline_flush_vulkan_state (CoglFramebuffer *framebuffer,
                                   CoglPipeline *pipeline,
                                   CoglAttribute **attributes,
                                   int n_attributes)
{
  CoglContext *ctx = framebuffer->context;
  CoglFramebufferVulkan *vk_fb = framebuffer->winsys;
  CoglPipelineVulkan *vk_pipeline = get_vk_pipeline (pipeline);
  int i, n_layers = cogl_pipeline_get_n_layers (pipeline);
  const CoglPipelineProgend *progend;
  const CoglPipelineVertend *vertend;
  const CoglPipelineFragend *fragend;
  CoglPipelineAddLayerState state;
  VkPipelineLayout pipeline_layout;
  VkDescriptorSet descriptor_set;

  COGL_STATIC_TIMER (pipeline_flush_timer,
                     "Mainloop", /* parent */
                     "Material Flush",
                     "The time spent flushing material state",
                     0 /* no application private data */);

  COGL_TIMER_START (_cogl_uprof_context, pipeline_flush_timer);

  if (vk_pipeline &&
      vk_pipeline->pipeline != VK_NULL_HANDLE &&
      _cogl_pipeline_vertend_vulkan_get_shader (pipeline) != NULL &&
      _cogl_pipeline_fragend_vulkan_get_shader (pipeline) != NULL)
    {
      if (vk_fb->vertices_modes[vk_fb->n_vertices_modes - 1] != vk_pipeline->vertices_mode ||
          framebuffer->color_mask != vk_pipeline->color_mask ||
          vk_pipeline->color_format != vk_fb->color_format ||
          vk_pipeline->depth_format != vk_fb->depth_format)
        _cogl_pipeline_vulkan_invalidate (pipeline);
      else
        _cogl_pipeline_vulkan_compute_attributes (ctx,
                                                  pipeline, vk_pipeline,
                                                  framebuffer,
                                                  attributes, n_attributes);

      if (vk_pipeline->pipeline != VK_NULL_HANDLE)
        goto done;
    }

  if (!vk_pipeline)
    vk_pipeline = vk_pipeline_new (pipeline);

  if (pipeline->progend == COGL_PIPELINE_PROGEND_UNDEFINED)
    _cogl_pipeline_set_progend (pipeline, COGL_PIPELINE_PROGEND_VULKAN);
  progend = _cogl_pipeline_progends[COGL_PIPELINE_PROGEND_VULKAN];

  /* Build up vertex shader. */
  g_assert (progend->start (pipeline));

  vertend = _cogl_pipeline_vertends[COGL_PIPELINE_VERTEND_VULKAN];
  vertend->start (pipeline, n_layers, 0);
  state.framebuffer = framebuffer;
  state.vertend = vertend;
  state.pipeline = pipeline;
  state.error_adding_layer = FALSE;

  _cogl_pipeline_foreach_layer_internal (pipeline,
                                         vertend_add_layer_cb,
                                         &state);
  g_assert (!state.error_adding_layer);
  g_assert (vertend->end (pipeline, 0));

  /* Build up fragment shader. */
  fragend = _cogl_pipeline_fragends[COGL_PIPELINE_FRAGEND_VULKAN];
  state.fragend = fragend;
  fragend->start (pipeline, n_layers, 0);

  _cogl_pipeline_foreach_layer_internal (pipeline,
                                         fragend_add_layer_cb,
                                         &state);
  g_assert (!state.error_adding_layer);
  g_assert (fragend->end (pipeline, 0));

  progend->end (pipeline, 0);

  /* Compute the attributes' layout by querying the AST of the generated
     vertex shader. */
  _cogl_pipeline_vulkan_compute_attributes (ctx,
                                            pipeline, vk_pipeline,
                                            framebuffer,
                                            attributes, n_attributes);
  _cogl_pipeline_vulkan_create_pipeline (pipeline, vk_pipeline, framebuffer);

done:

  progend = _cogl_pipeline_progends[pipeline->progend];

  _cogl_pipeline_progend_flush_descriptors (ctx, pipeline);

  /* Give the progend a chance to update any uniforms that might not
   * depend on the material state. This is used on GLES2 to update the
   * matrices */
  if (progend->pre_paint)
    progend->pre_paint (pipeline, framebuffer);

  _cogl_framebuffer_vulkan_begin_render_pass (framebuffer);

  VK ( ctx, vkCmdBindVertexBuffers (vk_fb->cmd_buffer,
                                    0, vk_pipeline->n_vertex_inputs,
                                    vk_pipeline->attribute_buffers,
                                    vk_pipeline->attribute_offsets) );

  for (i = 0; i < vk_pipeline->n_seen_attribute_buffers; i++)
    g_ptr_array_add (vk_fb->attribute_buffers,
                     cogl_object_ref (vk_pipeline->seen_attribute_buffers[i]));

  VK ( ctx, vkCmdBindPipeline (vk_fb->cmd_buffer,
                               VK_PIPELINE_BIND_POINT_GRAPHICS,
                               vk_pipeline->pipeline) );

  g_ptr_array_add (vk_fb->pipelines, cogl_object_ref (pipeline));

  pipeline_layout =
    _cogl_pipeline_progend_get_vulkan_pipeline_layout (pipeline);
  descriptor_set =
    _cogl_pipeline_progend_get_vulkan_descriptor_set (pipeline);

  VK ( ctx, vkCmdBindDescriptorSets (vk_fb->cmd_buffer,
                                     VK_PIPELINE_BIND_POINT_GRAPHICS,
                                     pipeline_layout,
                                     0, 1, &descriptor_set,
                                     0, NULL) );

  g_hash_table_insert (vk_pipeline->framebuffers, framebuffer, framebuffer);

  COGL_TIMER_STOP (_cogl_uprof_context, pipeline_flush_timer);
}

void
_cogl_vulkan_flush_attributes_state (CoglFramebuffer *framebuffer,
                                     CoglPipeline *pipeline,
                                     CoglFlushLayerState *layer_state,
                                     CoglDrawFlags flags,
                                     CoglAttribute **attributes,
                                     int n_attributes)
{
  _cogl_pipeline_flush_vulkan_state (framebuffer,
                                     pipeline,
                                     attributes,
                                     n_attributes);
}

void
_cogl_pipeline_vulkan_pre_change_notify (CoglPipeline *pipeline,
                                         CoglPipelineState change)
{
  if (change & (COGL_PIPELINE_STATE_BLEND |
                COGL_PIPELINE_STATE_BLEND_ENABLE |
                COGL_PIPELINE_STATE_CULL_FACE |
                COGL_PIPELINE_STATE_DEPTH))
    _cogl_pipeline_vulkan_invalidate (pipeline);
}
