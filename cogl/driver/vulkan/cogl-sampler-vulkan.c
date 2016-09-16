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

#include <config.h>

#include "cogl-context-private.h"
#include "cogl-driver-vulkan-private.h"
#include "cogl-sampler-vulkan-private.h"
#include "cogl-util-vulkan-private.h"

void
_cogl_sampler_vulkan_create (CoglContext *context,
                             CoglSamplerCacheEntry *entry)
{
  CoglContextVulkan *vk_ctx = context->winsys;
  VkSamplerCreateInfo info = {
    .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
    .mipLodBias = 0.0f,
    .anisotropyEnable = VK_FALSE,
    .maxAnisotropy = 1,
    .compareOp = VK_COMPARE_OP_NEVER,
    .minLod = 0.0f,
    .maxLod = 0.0f,
    .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
    .unnormalizedCoordinates = VK_FALSE,
  };
  VkSampler vk_sampler;

  _cogl_filter_to_vulkan_filter (entry->mag_filter, &info.magFilter, NULL);
  _cogl_filter_to_vulkan_filter (entry->min_filter, &info.minFilter,
                                 &info.mipmapMode);

  info.addressModeU =
    _cogl_wrap_mode_to_vulkan_address_mode (entry->wrap_mode_s);
  info.addressModeV =
    _cogl_wrap_mode_to_vulkan_address_mode (entry->wrap_mode_t);
  info.addressModeW =
    _cogl_wrap_mode_to_vulkan_address_mode (entry->wrap_mode_p);

  VK_RET ( context,
           vkCreateSampler (vk_ctx->device, &info, NULL, &vk_sampler) );

  entry->winsys = (void *) vk_sampler;
}

void
_cogl_sampler_vulkan_destroy (CoglContext *context,
                              CoglSamplerCacheEntry *entry)
{
  CoglContextVulkan *vk_ctx = context->winsys;
  VkSampler vk_sampler = (VkSampler) entry->winsys;

  VK ( context,
       vkDestroySampler (vk_ctx->device, vk_sampler, NULL) );
}
