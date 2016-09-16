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

#ifndef _COGL_UTIL_VULKAN_PRIVATE_H_
#define _COGL_UTIL_VULKAN_PRIVATE_H_

#include "cogl-pipeline-private.h"
#include "cogl-types.h"
#include "cogl-vulkan-header.h"

#define VK(ctx, x) (ctx)->x

#define VK_RET(ctx, x)                    G_STMT_START {  \
    VkResult __err;                                       \
    __err = (ctx)->x;                                     \
    while (__err != VK_SUCCESS)                           \
      {                                                   \
        g_warning ("%s: "                                 \
                   #x                                     \
                   ": VK error (%d): %s\n",               \
                   G_STRLOC,                              \
                   __err,                                 \
                   _cogl_vulkan_error_to_string (__err)); \
        return;                                           \
      }                                   } G_STMT_END

#define VK_RET_VAL(ctx, x, val)          G_STMT_START {   \
    VkResult __err;                                       \
    __err = (ctx)->x;                                     \
    while (__err != VK_SUCCESS)                           \
      {                                                   \
        g_warning ("%s: "                                 \
                   #x                                     \
                   ": VK error (%d): %s\n",               \
                   G_STRLOC,                              \
                   __err,                                 \
                   _cogl_vulkan_error_to_string (__err)); \
        return val;                                       \
      }                                   } G_STMT_END

#define VK_RET_VAL_ERROR(ctx, x, val, err, dom, e) G_STMT_START {       \
    VkResult __err;                                                     \
    __err = (ctx)->x;                                                   \
    while (__err != VK_SUCCESS)                                         \
      {                                                                 \
        _cogl_set_error (err, dom, e,                                   \
                         "%s: "                                         \
                         #x                                             \
                         ": VK error (%d): %s\n",                       \
                         G_STRLOC,                                      \
                         __err,                                         \
                         _cogl_vulkan_error_to_string (__err));         \
        return val;                                                     \
      }                                   } G_STMT_END


#define VK_ERROR(ctx, x, err, dom, e) G_STMT_START {            \
    VkResult __err;                                             \
    __err = (ctx)->x;                                           \
    while (__err != VK_SUCCESS)                                 \
      {                                                         \
        _cogl_set_error (err, dom, e,                           \
                         "%s: "                                 \
                         #x                                     \
                         ": VK error (%d): %s\n",               \
                         G_STRLOC,                              \
                         __err,                                 \
                         _cogl_vulkan_error_to_string (__err)); \
        goto error;                                             \
      }                                   } G_STMT_END


#define VK_TODO() do {                                                  \
    g_warning("Unimplemented function %s : %s", G_STRFUNC, G_STRLOC);   \
  } while(0)

CoglPixelFormat
_cogl_vulkan_format_to_pixel_format (VkFormat format);

VkFormat
_cogl_vulkan_format_unorm (VkFormat format);

CoglBool
_cogl_pixel_format_compatible_with_vulkan_format (CoglPixelFormat cogl_format,
                                                  VkFormat vk_format);

VkFormat
_cogl_pixel_format_to_vulkan_format (CoglContext *context,
                                     CoglPixelFormat format,
                                     CoglBool *premultiplied,
                                     VkComponentMapping *mapping);

VkFormat
_cogl_pixel_format_to_vulkan_format_for_sampling (CoglContext *context,
                                                  CoglPixelFormat format,
                                                  CoglBool *premultiplied,
                                                  VkComponentMapping *mapping);

VkFormat
_cogl_attribute_type_to_vulkan_format (CoglAttributeType type,
                                       int n_components);

const char *
_cogl_vulkan_error_to_string (VkResult error);

void
_cogl_filter_to_vulkan_filter (CoglPipelineFilter filter,
                               VkFilter *vkfilter,
                               VkSamplerMipmapMode *vksamplermode);

VkSamplerAddressMode
_cogl_wrap_mode_to_vulkan_address_mode (CoglPipelineWrapMode mode);

VkCullModeFlagBits
_cogl_cull_mode_to_vulkan_cull_mode (CoglPipelineCullFaceMode mode);

VkFrontFace
_cogl_winding_to_vulkan_front_face (CoglWinding winding);

VkIndexType
_cogl_indices_type_to_vulkan_indices_type (CoglIndicesType type);

VkPrimitiveTopology
_cogl_vertices_mode_to_vulkan_primitive_topology (CoglVerticesMode mode);

VkPolygonMode
_cogl_vertices_mode_to_vulkan_polygon_mode (CoglVerticesMode mode);

void
_cogl_vulkan_util_get_texture_target_string (CoglTextureType texture_type,
                                             const char **target_string_out,
                                             const char **swizzle_out);

VkBlendFactor
_cogl_blend_factor_to_vulkan_blend_factor (CoglPipelineBlendFactor factor);

VkBlendOp
_cogl_blend_equation_to_vulkan_blend_op (CoglPipelineBlendEquation equation);

VkCompareOp
_cogl_depth_test_function_to_vulkan_compare_op (CoglDepthTestFunction function);

#endif /* _COGL_UTIL_VULKAN_PRIVATE_H_ */
