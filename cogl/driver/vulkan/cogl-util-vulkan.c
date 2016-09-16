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

#include "cogl-context-private.h"
#include "cogl-driver-vulkan-private.h"
#include "cogl-util-vulkan-private.h"

CoglPixelFormat
_cogl_vulkan_format_to_pixel_format (VkFormat format)
{
  switch (_cogl_vulkan_format_unorm (format))
    {
    case VK_FORMAT_R4G4B4A4_UNORM_PACK16:
      return COGL_PIXEL_FORMAT_RGBA_4444;
    case VK_FORMAT_R5G5B5A1_UNORM_PACK16:
      return COGL_PIXEL_FORMAT_RGBA_5551;
    case VK_FORMAT_R5G6B5_UNORM_PACK16:
      return COGL_PIXEL_FORMAT_RGB_565;
    case VK_FORMAT_R8_UNORM:
      return COGL_PIXEL_FORMAT_G_8;
    case VK_FORMAT_R8G8_UNORM:
      return COGL_PIXEL_FORMAT_RG_88;
    case VK_FORMAT_R8G8B8_UNORM:
      return COGL_PIXEL_FORMAT_RGB_888;
    case VK_FORMAT_B8G8R8_UNORM:
      return COGL_PIXEL_FORMAT_BGR_888;
    case VK_FORMAT_R8G8B8A8_UNORM:
      return COGL_PIXEL_FORMAT_RGBA_8888;
    case VK_FORMAT_B8G8R8A8_UNORM:
      return COGL_PIXEL_FORMAT_BGRA_8888;
    case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
      return COGL_PIXEL_FORMAT_ABGR_8888;
    default:
      return COGL_PIXEL_FORMAT_ANY;
    }
}

static const VkFormat _format_to_unorm[] = {
  [VK_FORMAT_R4G4B4A4_UNORM_PACK16] = VK_FORMAT_R4G4B4A4_UNORM_PACK16,

  [VK_FORMAT_R5G5B5A1_UNORM_PACK16] = VK_FORMAT_R5G5B5A1_UNORM_PACK16,

  [VK_FORMAT_R5G6B5_UNORM_PACK16] = VK_FORMAT_R5G6B5_UNORM_PACK16,

  [VK_FORMAT_R8_UNORM] = VK_FORMAT_R8_UNORM,
  [VK_FORMAT_R8_SRGB] = VK_FORMAT_R8_UNORM,

  [VK_FORMAT_R8G8_UNORM] = VK_FORMAT_R8G8_UNORM,
  [VK_FORMAT_R8G8_SRGB] = VK_FORMAT_R8G8_UNORM,

  [VK_FORMAT_R8G8B8_UNORM] = VK_FORMAT_R8G8B8_UNORM,
  [VK_FORMAT_R8G8B8_SRGB] = VK_FORMAT_R8G8B8_UNORM,

  [VK_FORMAT_B8G8R8_UNORM] = VK_FORMAT_B8G8R8_UNORM,
  [VK_FORMAT_B8G8R8_SRGB] = VK_FORMAT_B8G8R8_UNORM,

  [VK_FORMAT_R8G8B8A8_UNORM] = VK_FORMAT_R8G8B8A8_UNORM,
  [VK_FORMAT_R8G8B8A8_SRGB] = VK_FORMAT_R8G8B8A8_UNORM,

  [VK_FORMAT_B8G8R8A8_UNORM] = VK_FORMAT_B8G8R8A8_UNORM,
  [VK_FORMAT_B8G8R8A8_SRGB] = VK_FORMAT_B8G8R8A8_UNORM,

  [VK_FORMAT_A8B8G8R8_UNORM_PACK32] = VK_FORMAT_A8B8G8R8_UNORM_PACK32,
  [VK_FORMAT_A8B8G8R8_SRGB_PACK32] = VK_FORMAT_A8B8G8R8_UNORM_PACK32,

  [VK_FORMAT_A2B10G10R10_UNORM_PACK32] = VK_FORMAT_A2B10G10R10_UNORM_PACK32,
  [VK_FORMAT_A2R10G10B10_UINT_PACK32] = VK_FORMAT_A2R10G10B10_UNORM_PACK32,

  [VK_FORMAT_A2B10G10R10_UNORM_PACK32] = VK_FORMAT_A2B10G10R10_UNORM_PACK32,
  [VK_FORMAT_A2B10G10R10_UINT_PACK32] = VK_FORMAT_A2B10G10R10_UNORM_PACK32,
};

VkFormat
_cogl_vulkan_format_unorm (VkFormat format)
{
  if (_format_to_unorm[format] == VK_FORMAT_UNDEFINED)
    return VK_FORMAT_UNDEFINED;
  return _format_to_unorm[format];
}

CoglBool
_cogl_pixel_format_compatible_with_vulkan_format (CoglPixelFormat cogl_format,
                                                  VkFormat vk_format)
{
  switch (vk_format)
    {
    case VK_FORMAT_B8G8R8A8_SRGB:
    case VK_FORMAT_R8G8B8A8_SRGB:
      // TODO: should we add ABGR, ARGB?
      return (cogl_format == COGL_PIXEL_FORMAT_RGBA_8888 ||
              cogl_format == COGL_PIXEL_FORMAT_RGBA_8888_PRE ||
              cogl_format == COGL_PIXEL_FORMAT_BGRA_8888 ||
              cogl_format == COGL_PIXEL_FORMAT_BGRA_8888_PRE);

    case VK_FORMAT_R8G8B8_SRGB:
    case VK_FORMAT_B8G8R8_SRGB:
      return (cogl_format == COGL_PIXEL_FORMAT_RGB_888 ||
              cogl_format == COGL_PIXEL_FORMAT_BGR_888);

    case VK_FORMAT_R8G8_SRGB:
      return (cogl_format == COGL_PIXEL_FORMAT_RG_88);

    case VK_FORMAT_R8_SRGB:
      return (cogl_format == COGL_PIXEL_FORMAT_G_8);

    default:
      return FALSE;
    }
}

#define SWIZ(_r,_g,_b,_a)                           \
  {                                                 \
    .r = VK_COMPONENT_SWIZZLE_##_r,                 \
    .g = VK_COMPONENT_SWIZZLE_##_g,                 \
    .b = VK_COMPONENT_SWIZZLE_##_b,                 \
    .a = VK_COMPONENT_SWIZZLE_##_a                  \
  }

#define SWIZ_RGBA SWIZ(R,G,B,A)
#define SWIZ_RGB1 SWIZ(R,G,B,ONE)

#define COGL_VK_VIEW(_format, _swiz)                    \
  {                                                     \
    .format = VK_FORMAT_##_format,                      \
    .mapping = _swiz,                                   \
  }

#define COGL_VK_VIEW_NONE                       \
  {                                             \
    .format = VK_FORMAT_UNDEFINED               \
  }

#define RETURN_COGL_VK_VIEWS(_views...) do {        \
    static const CoglVulkanFormat __views[] = {     \
      _views                                        \
    };                                              \
    return __views;                                 \
  } while (0)

typedef struct
{
  VkFormat format;
  VkComponentMapping mapping;
} CoglVulkanFormat;

static const CoglVulkanFormat *
_get_pixel_format_guesses_for_render (CoglPixelFormat format)
{
  switch (format)
    {
    case COGL_PIXEL_FORMAT_ARGB_2101010:
    case COGL_PIXEL_FORMAT_ARGB_2101010_PRE:
      RETURN_COGL_VK_VIEWS (COGL_VK_VIEW (A2R10G10B10_UINT_PACK32, SWIZ_RGBA),
                            COGL_VK_VIEW_NONE);
    case COGL_PIXEL_FORMAT_ABGR_2101010:
    case COGL_PIXEL_FORMAT_ABGR_2101010_PRE:
      RETURN_COGL_VK_VIEWS (COGL_VK_VIEW (A2R10G10B10_UINT_PACK32, SWIZ (B, G, R, A)),
                            COGL_VK_VIEW_NONE);

    case COGL_PIXEL_FORMAT_RGBA_8888:
    case COGL_PIXEL_FORMAT_RGBA_8888_PRE:
      RETURN_COGL_VK_VIEWS (COGL_VK_VIEW (R8G8B8A8_SRGB, SWIZ_RGBA),
                            COGL_VK_VIEW (B8G8R8A8_SRGB, SWIZ (B, G, R, A)),
                            COGL_VK_VIEW (A8B8G8R8_SRGB_PACK32, SWIZ (A, B, G, R)),
                            COGL_VK_VIEW_NONE);
    case COGL_PIXEL_FORMAT_BGRA_8888:
    case COGL_PIXEL_FORMAT_BGRA_8888_PRE:
      RETURN_COGL_VK_VIEWS (COGL_VK_VIEW (B8G8R8A8_SRGB, SWIZ_RGBA),
                            COGL_VK_VIEW (R8G8B8A8_SRGB, SWIZ (B, G, R, A)),
                            COGL_VK_VIEW (A8B8G8R8_SRGB_PACK32, SWIZ (A, G, R, B)),
                            COGL_VK_VIEW_NONE);
    case COGL_PIXEL_FORMAT_ARGB_8888:
    case COGL_PIXEL_FORMAT_ARGB_8888_PRE:
      RETURN_COGL_VK_VIEWS (COGL_VK_VIEW (A8B8G8R8_SRGB_PACK32, SWIZ_RGBA),
                            COGL_VK_VIEW (B8G8R8A8_SRGB, SWIZ (G, B, A, R)),
                            COGL_VK_VIEW (R8G8B8A8_SRGB, SWIZ (A, B, G, R)),
                            COGL_VK_VIEW_NONE);

    case COGL_PIXEL_FORMAT_RGB_888:
      RETURN_COGL_VK_VIEWS (COGL_VK_VIEW (R8G8B8_SRGB, SWIZ_RGB1),
                            COGL_VK_VIEW (B8G8R8_SRGB, SWIZ (B, G, R, ONE)),
                            COGL_VK_VIEW (R8G8B8A8_SRGB, SWIZ (B, G, R, ONE)),
                            COGL_VK_VIEW_NONE);
    case COGL_PIXEL_FORMAT_BGR_888:
      RETURN_COGL_VK_VIEWS (COGL_VK_VIEW (B8G8R8_SRGB, SWIZ_RGB1),
                            COGL_VK_VIEW (R8G8B8_SRGB, SWIZ (B, G, R, ONE)),
                            COGL_VK_VIEW (B8G8R8A8_SRGB, SWIZ_RGB1),
                            COGL_VK_VIEW_NONE);

    case COGL_PIXEL_FORMAT_RGBA_4444:
    case COGL_PIXEL_FORMAT_RGBA_4444_PRE:
      RETURN_COGL_VK_VIEWS (COGL_VK_VIEW (R4G4B4A4_UNORM_PACK16, SWIZ_RGBA),
                            COGL_VK_VIEW (B4G4R4A4_UNORM_PACK16, SWIZ (B, G, R, A)),
                            COGL_VK_VIEW_NONE);

    case COGL_PIXEL_FORMAT_RGB_565:
      RETURN_COGL_VK_VIEWS (COGL_VK_VIEW (R5G6B5_UNORM_PACK16, SWIZ_RGB1),
                            COGL_VK_VIEW (B5G6R5_UNORM_PACK16, SWIZ (B, G, R, ONE)),
                            COGL_VK_VIEW_NONE);
    case COGL_PIXEL_FORMAT_RGBA_5551:
    case COGL_PIXEL_FORMAT_RGBA_5551_PRE:
      RETURN_COGL_VK_VIEWS (COGL_VK_VIEW (R5G5B5A1_UNORM_PACK16, SWIZ_RGBA),
                            COGL_VK_VIEW (B5G5R5A1_UNORM_PACK16, SWIZ (B, G, R, A)),
                            COGL_VK_VIEW_NONE);

    case COGL_PIXEL_FORMAT_RG_88:
      RETURN_COGL_VK_VIEWS (COGL_VK_VIEW (R8G8_SRGB, SWIZ (R, G, ZERO, ONE)),
                            COGL_VK_VIEW_NONE);
      break;
    case COGL_PIXEL_FORMAT_G_8:
      RETURN_COGL_VK_VIEWS (COGL_VK_VIEW (R8_SRGB, SWIZ (R, ZERO, ZERO, ONE)),
                            COGL_VK_VIEW_NONE);
      break;
    case COGL_PIXEL_FORMAT_A_8:
      RETURN_COGL_VK_VIEWS (COGL_VK_VIEW (R8_SRGB, SWIZ (ZERO, ZERO, ZERO, R)),
                            COGL_VK_VIEW_NONE);
      break;

    default:
      g_warning ("Unsupported CoglPixelFormat %i alpha=%i bgr=%i a_first=%i form=%i",
                 format,
                 (format & COGL_A_BIT) != 0,
                 (format & COGL_BGR_BIT) != 0,
                 (format & COGL_AFIRST_BIT) != 0,
                 format & 0xff);
      RETURN_COGL_VK_VIEWS (COGL_VK_VIEW_NONE);
    }
}

static const CoglVulkanFormat *
_get_pixel_format_guesses_for_sampling (CoglPixelFormat format)
{
  switch (format)
    {
    case COGL_PIXEL_FORMAT_ARGB_2101010:
    case COGL_PIXEL_FORMAT_ARGB_2101010_PRE:
      RETURN_COGL_VK_VIEWS (COGL_VK_VIEW (A2R10G10B10_UNORM_PACK32, SWIZ_RGBA),
                            COGL_VK_VIEW_NONE);
    case COGL_PIXEL_FORMAT_ABGR_2101010:
    case COGL_PIXEL_FORMAT_ABGR_2101010_PRE:
      RETURN_COGL_VK_VIEWS (COGL_VK_VIEW (A2R10G10B10_UNORM_PACK32, SWIZ (B, G, R, A)),
                            COGL_VK_VIEW_NONE);

    case COGL_PIXEL_FORMAT_RGBA_8888:
    case COGL_PIXEL_FORMAT_RGBA_8888_PRE:
      RETURN_COGL_VK_VIEWS (COGL_VK_VIEW (R8G8B8A8_UNORM, SWIZ_RGBA),
                            COGL_VK_VIEW (B8G8R8A8_UNORM, SWIZ (B, G, R, A)),
                            COGL_VK_VIEW_NONE);
    case COGL_PIXEL_FORMAT_BGRA_8888:
    case COGL_PIXEL_FORMAT_BGRA_8888_PRE:
      RETURN_COGL_VK_VIEWS (COGL_VK_VIEW (B8G8R8A8_UNORM, SWIZ_RGBA),
                            COGL_VK_VIEW (R8G8B8A8_UNORM, SWIZ (B, G, R, A)),
                            COGL_VK_VIEW_NONE);
    case COGL_PIXEL_FORMAT_ARGB_8888:
    case COGL_PIXEL_FORMAT_ARGB_8888_PRE:
      RETURN_COGL_VK_VIEWS (COGL_VK_VIEW (B8G8R8A8_UNORM, SWIZ (G, R, A, B)),
                            COGL_VK_VIEW (R8G8B8A8_UNORM, SWIZ (G, B, A, R)),
                            COGL_VK_VIEW_NONE);
    case COGL_PIXEL_FORMAT_ABGR_8888:
    case COGL_PIXEL_FORMAT_ABGR_8888_PRE:
      RETURN_COGL_VK_VIEWS (COGL_VK_VIEW (B8G8R8A8_UNORM, SWIZ (A, R, G, B)),
                            COGL_VK_VIEW (R8G8B8A8_UNORM, SWIZ (A, B, G, R)),
                            COGL_VK_VIEW_NONE);

    case COGL_PIXEL_FORMAT_RGB_888:
      RETURN_COGL_VK_VIEWS (COGL_VK_VIEW (R8G8B8_UNORM, SWIZ_RGB1),
                            COGL_VK_VIEW (B8G8R8_UNORM, SWIZ (B, G, R, ONE)),
                            COGL_VK_VIEW (R8G8B8A8_SRGB, SWIZ (B, G, R, ONE)),
                            COGL_VK_VIEW_NONE);
    case COGL_PIXEL_FORMAT_BGR_888:
      RETURN_COGL_VK_VIEWS (COGL_VK_VIEW (B8G8R8_UNORM, SWIZ_RGB1),
                            COGL_VK_VIEW (R8G8B8_UNORM, SWIZ (B, G, R, ONE)),
                            COGL_VK_VIEW (B8G8R8A8_UNORM, SWIZ_RGB1),
                            COGL_VK_VIEW_NONE);

    case COGL_PIXEL_FORMAT_RGBA_4444:
    case COGL_PIXEL_FORMAT_RGBA_4444_PRE:
      RETURN_COGL_VK_VIEWS (COGL_VK_VIEW (R4G4B4A4_UNORM_PACK16, SWIZ_RGBA),
                            COGL_VK_VIEW (B4G4R4A4_UNORM_PACK16, SWIZ (B, G, R, A)),
                            COGL_VK_VIEW_NONE);

    case COGL_PIXEL_FORMAT_RGB_565:
      RETURN_COGL_VK_VIEWS (COGL_VK_VIEW (R5G6B5_UNORM_PACK16, SWIZ_RGB1),
                            COGL_VK_VIEW (B5G6R5_UNORM_PACK16, SWIZ (B, G, R, ONE)),
                            COGL_VK_VIEW_NONE);
    case COGL_PIXEL_FORMAT_RGBA_5551:
    case COGL_PIXEL_FORMAT_RGBA_5551_PRE:
      RETURN_COGL_VK_VIEWS (COGL_VK_VIEW (R5G5B5A1_UNORM_PACK16, SWIZ_RGBA),
                            COGL_VK_VIEW (B5G5R5A1_UNORM_PACK16, SWIZ (B, G, R, A)),
                            COGL_VK_VIEW_NONE);

    case COGL_PIXEL_FORMAT_RG_88:
      RETURN_COGL_VK_VIEWS (COGL_VK_VIEW (R8G8_UNORM, SWIZ (R, G, ZERO, ONE)),
                            COGL_VK_VIEW_NONE);
      break;
    case COGL_PIXEL_FORMAT_G_8:
      RETURN_COGL_VK_VIEWS (COGL_VK_VIEW (R8_UNORM, SWIZ (R, ZERO, ZERO, ONE)),
                            COGL_VK_VIEW_NONE);
      break;
    case COGL_PIXEL_FORMAT_A_8:
      RETURN_COGL_VK_VIEWS (COGL_VK_VIEW (R8_UNORM, SWIZ (ZERO, ZERO, ZERO, R)),
                            COGL_VK_VIEW_NONE);
      break;

    default:
      g_warning ("Unsupported CoglPixelFormat %i alpha=%i bgr=%i a_first=%i form=%i",
                 format,
                 format & COGL_A_BIT,
                 format & COGL_BGR_BIT,
                 format & COGL_AFIRST_BIT,
                 format & 0xff);
      RETURN_COGL_VK_VIEWS (COGL_VK_VIEW_NONE);
    }
}

VkFormat
_cogl_pixel_format_to_vulkan_format (CoglContext *context,
                                     CoglPixelFormat format,
                                     CoglBool *premultiplied,
                                     VkComponentMapping *mapping)
{
  CoglContextVulkan *vk_ctx = context->winsys;
  const CoglVulkanFormat *formats_guesses = _get_pixel_format_guesses_for_render (format);
  int i;

  for (i = 0; formats_guesses[i].format != VK_FORMAT_UNDEFINED; i++)
    {
      VkFormat selected_format = formats_guesses[i].format;
      VkFormatProperties *properties =
        &vk_ctx->supported_formats[selected_format];
      const VkFormatFeatureFlags requested_flags =
        VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;

      if ((properties->linearTilingFeatures & requested_flags) == requested_flags &&
          (properties->optimalTilingFeatures & requested_flags) == requested_flags)
        break;
    }

  if (mapping)
    *mapping = formats_guesses[i].mapping;
  if (premultiplied)
    *premultiplied = (COGL_PREMULT_BIT & format) != 0;
  return formats_guesses[i].format;
}

VkFormat
_cogl_pixel_format_to_vulkan_format_for_sampling (CoglContext *context,
                                                  CoglPixelFormat format,
                                                  CoglBool *premultiplied,
                                                  VkComponentMapping *mapping)
{
  CoglContextVulkan *vk_ctx = context->winsys;
  const CoglVulkanFormat *formats_guesses = _get_pixel_format_guesses_for_sampling (format);
  int i;

  for (i = 0; formats_guesses[i].format != VK_FORMAT_UNDEFINED; i++)
    {
      VkFormat selected_format = formats_guesses[i].format;
      VkFormatProperties *properties =
        &vk_ctx->supported_formats[selected_format];
      const VkFormatFeatureFlags requested_flags =
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;

      if ((properties->linearTilingFeatures & requested_flags) == requested_flags &&
          (properties->optimalTilingFeatures & requested_flags) == requested_flags)
        break;
    }

  if (mapping)
    *mapping = formats_guesses[i].mapping;
  if (premultiplied)
    *premultiplied = (COGL_PREMULT_BIT & format) != 0;
  return formats_guesses[i].format;
}

static VkFormat _attributes_to_formats[5][4] = {
  /* COGL_ATTRIBUTE_TYPE_BYTE */
  { VK_FORMAT_R8_SNORM, VK_FORMAT_R8G8_SNORM, VK_FORMAT_R8G8B8_SNORM, VK_FORMAT_R8G8B8A8_SNORM },
  /* COGL_ATTRIBUTE_TYPE_UNSIGNED_BYTE */
  { VK_FORMAT_R8_UNORM, VK_FORMAT_R8G8_UNORM, VK_FORMAT_R8G8B8_UNORM, VK_FORMAT_R8G8B8A8_UNORM },
  /* COGL_ATTRIBUTE_TYPE_SHORT */
  { VK_FORMAT_R16_SNORM, VK_FORMAT_R16G16_SNORM, VK_FORMAT_R16G16B16_SNORM, VK_FORMAT_R16G16B16A16_SNORM },
  /* COGL_ATTRIBUTE_TYPE_UNSIGNED_SHORT */
  { VK_FORMAT_R16_UNORM, VK_FORMAT_R16G16_UNORM, VK_FORMAT_R16G16B16_UNORM, VK_FORMAT_R16G16B16A16_UNORM },
  /* COGL_ATTRIBUTE_TYPE_FLOAT */
  { VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT },
};

static int
_attribute_type_to_int (CoglAttributeType type)
{
  switch (type)
    {
      case COGL_ATTRIBUTE_TYPE_BYTE:
        return 0;
      case COGL_ATTRIBUTE_TYPE_UNSIGNED_BYTE:
        return 1;
      case COGL_ATTRIBUTE_TYPE_SHORT:
        return 2;
      case COGL_ATTRIBUTE_TYPE_UNSIGNED_SHORT:
        return 3;
      case COGL_ATTRIBUTE_TYPE_FLOAT:
        return 4;
      default:
        g_assert_not_reached();
    }
}

VkFormat
_cogl_attribute_type_to_vulkan_format (CoglAttributeType type, int n_components)
{
  g_assert (n_components <= 4);
  g_assert (type <= COGL_ATTRIBUTE_TYPE_FLOAT);

  return _attributes_to_formats[_attribute_type_to_int (type)][n_components - 1];
}

const char *
_cogl_vulkan_error_to_string (VkResult error)
{
  switch (error)
    {
    case VK_NOT_READY:
      return "not ready";
    case VK_TIMEOUT:
      return "timeout";
    case VK_INCOMPLETE:
      return "incomplete";
    case VK_ERROR_OUT_OF_HOST_MEMORY:
      return "out of host memory";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
      return "out of device memory";
    case VK_ERROR_INITIALIZATION_FAILED:
      return "initialization failed";
    case VK_ERROR_DEVICE_LOST:
      return "device lost";
    case VK_ERROR_MEMORY_MAP_FAILED:
      return "memory map failed";
    case VK_ERROR_LAYER_NOT_PRESENT:
      return "layer not present";
    case VK_ERROR_EXTENSION_NOT_PRESENT:
      return "extension not present";
    case VK_ERROR_FEATURE_NOT_PRESENT:
      return "feature not present";
    case VK_ERROR_INCOMPATIBLE_DRIVER:
      return "incompatible driver";
    case VK_ERROR_TOO_MANY_OBJECTS:
      return "too many objects";
    case VK_ERROR_FORMAT_NOT_SUPPORTED:
      return "format not supported";
    case VK_ERROR_SURFACE_LOST_KHR:
      return "surface lost";
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
       return "native window in use";
    case VK_ERROR_OUT_OF_DATE_KHR:
      return "out of date khr";
    case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
      return "incompatible display khr";
    case VK_ERROR_VALIDATION_FAILED_EXT:
      return "validation failed ext";
    default:
      return "unknown";
    }
}

void
_cogl_filter_to_vulkan_filter (CoglPipelineFilter filter,
                               VkFilter *vkfilter,
                               VkSamplerMipmapMode *vksamplermode)
{
  VkFilter _vkfilter = VK_FILTER_NEAREST;
  VkSamplerMipmapMode _vksamplermode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

  switch (filter)
    {
    case COGL_PIPELINE_FILTER_NEAREST:
      _vkfilter = VK_FILTER_NEAREST;
      break;
    case COGL_PIPELINE_FILTER_LINEAR:
      _vkfilter = VK_FILTER_LINEAR;
      break;
    case COGL_PIPELINE_FILTER_NEAREST_MIPMAP_NEAREST:
      _vkfilter = VK_FILTER_NEAREST;
      _vksamplermode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
      break;
    case COGL_PIPELINE_FILTER_LINEAR_MIPMAP_NEAREST:
      _vkfilter = VK_FILTER_LINEAR;
      _vksamplermode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
      break;
    case COGL_PIPELINE_FILTER_NEAREST_MIPMAP_LINEAR:
      _vkfilter = VK_FILTER_NEAREST;
      _vksamplermode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
      break;
    case COGL_PIPELINE_FILTER_LINEAR_MIPMAP_LINEAR:
      _vkfilter = VK_FILTER_LINEAR;
      _vksamplermode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
      break;
    default:
      g_assert_not_reached();
    }

  if (vkfilter)
    *vkfilter = _vkfilter;
  if (vksamplermode)
    *vksamplermode = _vksamplermode;
}

VkSamplerAddressMode
_cogl_wrap_mode_to_vulkan_address_mode (CoglPipelineWrapMode mode)
{
  switch (mode)
    {
    case COGL_PIPELINE_WRAP_MODE_REPEAT:
      return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    case COGL_PIPELINE_WRAP_MODE_MIRRORED_REPEAT:
      return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    case COGL_PIPELINE_WRAP_MODE_CLAMP_TO_EDGE:
      return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case COGL_PIPELINE_WRAP_MODE_AUTOMATIC:
      return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    default:
      g_assert_not_reached();
    }
}

VkCullModeFlagBits
_cogl_cull_mode_to_vulkan_cull_mode (CoglPipelineCullFaceMode mode)
{
  switch (mode)
    {
    case COGL_PIPELINE_CULL_FACE_MODE_NONE:
      return VK_CULL_MODE_NONE;
    case COGL_PIPELINE_CULL_FACE_MODE_FRONT:
      return VK_CULL_MODE_FRONT_BIT;
    case COGL_PIPELINE_CULL_FACE_MODE_BACK:
      return VK_CULL_MODE_BACK_BIT;
    case  COGL_PIPELINE_CULL_FACE_MODE_BOTH:
      return VK_CULL_MODE_FRONT_AND_BACK;
    default:
      g_assert_not_reached();
    }
}

VkFrontFace
_cogl_winding_to_vulkan_front_face (CoglWinding winding)
{
  switch (winding)
    {
    case COGL_WINDING_CLOCKWISE:
      return VK_FRONT_FACE_CLOCKWISE;
    case COGL_WINDING_COUNTER_CLOCKWISE:
      return VK_FRONT_FACE_COUNTER_CLOCKWISE;
    default:
      g_assert_not_reached();
    }
}

VkIndexType
_cogl_indices_type_to_vulkan_indices_type (CoglIndicesType type)
{
  switch (type)
    {
    case COGL_INDICES_TYPE_UNSIGNED_BYTE:
      g_warning ("unsigned bytes indices are not supported on Vulkan");
      g_assert_not_reached();
    case COGL_INDICES_TYPE_UNSIGNED_SHORT:
      return VK_INDEX_TYPE_UINT16;
    case COGL_INDICES_TYPE_UNSIGNED_INT:
      return VK_INDEX_TYPE_UINT32;
    default:
      g_assert_not_reached();
    }
}

VkPrimitiveTopology
_cogl_vertices_mode_to_vulkan_primitive_topology (CoglVerticesMode mode)
{
  switch (mode)
    {
    case COGL_VERTICES_MODE_POINTS:
      return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    case COGL_VERTICES_MODE_LINES:
      return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    case COGL_VERTICES_MODE_LINE_LOOP:
      /* ¯\_(ツ)_/¯ */
      return VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY;
    case COGL_VERTICES_MODE_LINE_STRIP:
      return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
    case COGL_VERTICES_MODE_TRIANGLES:
      return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    case COGL_VERTICES_MODE_TRIANGLE_STRIP:
      return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    case COGL_VERTICES_MODE_TRIANGLE_FAN:
      return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
    default:
      g_assert_not_reached();
    }
}

VkPolygonMode
_cogl_vertices_mode_to_vulkan_polygon_mode (CoglVerticesMode mode)
{
  switch (mode)
    {
    case COGL_VERTICES_MODE_POINTS:
      return VK_POLYGON_MODE_POINT;
    case COGL_VERTICES_MODE_LINES:
    case COGL_VERTICES_MODE_LINE_LOOP:
    case COGL_VERTICES_MODE_LINE_STRIP:
      return VK_POLYGON_MODE_LINE;
    case COGL_VERTICES_MODE_TRIANGLES:
    case COGL_VERTICES_MODE_TRIANGLE_STRIP:
    case COGL_VERTICES_MODE_TRIANGLE_FAN:
      return VK_POLYGON_MODE_FILL;
    default:
      g_assert_not_reached();
    }
}

void
_cogl_vulkan_util_get_texture_target_string (CoglTextureType texture_type,
                                             const char **target_string_out,
                                             const char **swizzle_out)
{
  const char *target_string, *tex_coord_swizzle;

  switch (texture_type)
    {
#if 0 /* TODO */
    case COGL_TEXTURE_TYPE_1D:
      target_string = "1D";
      tex_coord_swizzle = "s";
      break;
#endif

    case COGL_TEXTURE_TYPE_2D:
      target_string = "2D";
      tex_coord_swizzle = "st";
      break;

#if 0 /* TODO */
    case COGL_TEXTURE_TYPE_3D:
      target_string = "3D";
      tex_coord_swizzle = "stp";
      break;
#endif

    default:
      target_string = "Unknown";
      tex_coord_swizzle = NULL;
      g_assert_not_reached ();
    }

  if (target_string_out)
    *target_string_out = target_string;
  if (swizzle_out)
    *swizzle_out = tex_coord_swizzle;
}

VkBlendFactor
_cogl_blend_factor_to_vulkan_blend_factor (CoglPipelineBlendFactor factor)
{
  switch (factor)
    {
    case COGL_PIPELINE_BLEND_FACTOR_ZERO:
      return VK_BLEND_FACTOR_ZERO;
    case COGL_PIPELINE_BLEND_FACTOR_ONE:
      return VK_BLEND_FACTOR_ONE;
    case COGL_PIPELINE_BLEND_FACTOR_SRC_ALPHA_SATURATE:
      return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
    case COGL_PIPELINE_BLEND_FACTOR_ONE_MINUS_SRC_COLOR:
      return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    case COGL_PIPELINE_BLEND_FACTOR_SRC_COLOR:
      return VK_BLEND_FACTOR_SRC_COLOR;
    case COGL_PIPELINE_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA:
      return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case COGL_PIPELINE_BLEND_FACTOR_SRC_ALPHA:
      return VK_BLEND_FACTOR_SRC_ALPHA;
    case COGL_PIPELINE_BLEND_FACTOR_ONE_MINUS_DST_COLOR:
      return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
    case COGL_PIPELINE_BLEND_FACTOR_DST_COLOR:
      return VK_BLEND_FACTOR_DST_COLOR;
    case COGL_PIPELINE_BLEND_FACTOR_ONE_MINUS_DST_ALPHA:
      return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    case COGL_PIPELINE_BLEND_FACTOR_DST_ALPHA:
      return VK_BLEND_FACTOR_DST_ALPHA;
    case COGL_PIPELINE_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR:
      return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
    case COGL_PIPELINE_BLEND_FACTOR_CONSTANT_COLOR:
      return VK_BLEND_FACTOR_CONSTANT_COLOR;
    case COGL_PIPELINE_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA:
      return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
    case COGL_PIPELINE_BLEND_FACTOR_CONSTANT_ALPHA:
      return VK_BLEND_FACTOR_CONSTANT_ALPHA;
    default:
      g_assert_not_reached();
    }
}

VkBlendOp
_cogl_blend_equation_to_vulkan_blend_op (CoglPipelineBlendEquation equation)
{
  switch (equation)
    {
    case COGL_PIPELINE_BLEND_EQUATION_ADD:
      return VK_BLEND_OP_ADD;
    default:
      g_assert_not_reached();
    }
}

VkCompareOp
_cogl_depth_test_function_to_vulkan_compare_op (CoglDepthTestFunction function)
{
  switch (function)
    {
    case COGL_DEPTH_TEST_FUNCTION_NEVER:
      return VK_COMPARE_OP_NEVER;
    case COGL_DEPTH_TEST_FUNCTION_LESS:
      return VK_COMPARE_OP_LESS;
    case COGL_DEPTH_TEST_FUNCTION_EQUAL:
      return VK_COMPARE_OP_EQUAL;
    case COGL_DEPTH_TEST_FUNCTION_LEQUAL:
      return VK_COMPARE_OP_LESS_OR_EQUAL;
    case COGL_DEPTH_TEST_FUNCTION_GREATER:
      return VK_COMPARE_OP_GREATER;
    case COGL_DEPTH_TEST_FUNCTION_NOTEQUAL:
      return VK_COMPARE_OP_NOT_EQUAL;
    case COGL_DEPTH_TEST_FUNCTION_GEQUAL:
      return VK_COMPARE_OP_GREATER_OR_EQUAL;
    case COGL_DEPTH_TEST_FUNCTION_ALWAYS:
      return VK_COMPARE_OP_ALWAYS;
    default:
      g_assert_not_reached();
    }
}
