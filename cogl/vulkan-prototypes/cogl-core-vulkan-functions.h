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

/* This is included multiple times with different definitions for
 * these macros. The macros are given the following arguments:
 *
 * COGL_EXT_BEGIN:
 *
 * @name: a unique symbol name for this feature
 *
 * @min_vk_major: the major part of the minimum Vulkan version where these
 * functions are available in core, or 255 if it isn't available in
 * any version.
 * @min_vk_minor: the minor part of the minimum Vulkan version where these
 * functions are available in core, or 255 if it isn't available in
 * any version.
 * @min_vk_micro: the micro part of the minimum Vulkan version where these
 * functions are available in core, or 255 if it isn't available in
 * any version.
 *
 * @extension_suffixe:
 */

/* These are the core Vulkan functions which we assume will always be
   available */
COGL_VULKAN_EXT_BEGIN (core, 1, 0, 2, "\0")
COGL_VULKAN_EXT_FUNCTION (vkGetPhysicalDeviceFeatures)
COGL_VULKAN_EXT_FUNCTION (vkGetPhysicalDeviceFormatProperties)
COGL_VULKAN_EXT_FUNCTION (vkGetPhysicalDeviceImageFormatProperties)
COGL_VULKAN_EXT_FUNCTION (vkGetPhysicalDeviceQueueFamilyProperties)
COGL_VULKAN_EXT_FUNCTION (vkGetPhysicalDeviceMemoryProperties)
COGL_VULKAN_EXT_FUNCTION (vkCreateDevice)
COGL_VULKAN_EXT_FUNCTION (vkDestroyDevice)
COGL_VULKAN_EXT_FUNCTION (vkGetDeviceQueue)
COGL_VULKAN_EXT_FUNCTION (vkQueueSubmit)
COGL_VULKAN_EXT_FUNCTION (vkQueueWaitIdle)
COGL_VULKAN_EXT_FUNCTION (vkDeviceWaitIdle)
COGL_VULKAN_EXT_FUNCTION (vkAllocateMemory)
COGL_VULKAN_EXT_FUNCTION (vkFreeMemory)
COGL_VULKAN_EXT_FUNCTION (vkMapMemory)
COGL_VULKAN_EXT_FUNCTION (vkUnmapMemory)
COGL_VULKAN_EXT_FUNCTION (vkFlushMappedMemoryRanges)
COGL_VULKAN_EXT_FUNCTION (vkInvalidateMappedMemoryRanges)
COGL_VULKAN_EXT_FUNCTION (vkGetDeviceMemoryCommitment)
COGL_VULKAN_EXT_FUNCTION (vkBindBufferMemory)
COGL_VULKAN_EXT_FUNCTION (vkBindImageMemory)
COGL_VULKAN_EXT_FUNCTION (vkGetBufferMemoryRequirements)
COGL_VULKAN_EXT_FUNCTION (vkGetImageMemoryRequirements)
COGL_VULKAN_EXT_FUNCTION (vkGetImageSparseMemoryRequirements)
COGL_VULKAN_EXT_FUNCTION (vkGetPhysicalDeviceSparseImageFormatProperties)
COGL_VULKAN_EXT_FUNCTION (vkQueueBindSparse)
COGL_VULKAN_EXT_FUNCTION (vkCreateFence)
COGL_VULKAN_EXT_FUNCTION (vkDestroyFence)
COGL_VULKAN_EXT_FUNCTION (vkResetFences)
COGL_VULKAN_EXT_FUNCTION (vkGetFenceStatus)
COGL_VULKAN_EXT_FUNCTION (vkWaitForFences)
COGL_VULKAN_EXT_FUNCTION (vkCreateSemaphore)
COGL_VULKAN_EXT_FUNCTION (vkDestroySemaphore)
COGL_VULKAN_EXT_FUNCTION (vkCreateEvent)
COGL_VULKAN_EXT_FUNCTION (vkDestroyEvent)
COGL_VULKAN_EXT_FUNCTION (vkGetEventStatus)
COGL_VULKAN_EXT_FUNCTION (vkSetEvent)
COGL_VULKAN_EXT_FUNCTION (vkResetEvent)
COGL_VULKAN_EXT_FUNCTION (vkCreateQueryPool)
COGL_VULKAN_EXT_FUNCTION (vkDestroyQueryPool)
COGL_VULKAN_EXT_FUNCTION (vkGetQueryPoolResults)
COGL_VULKAN_EXT_FUNCTION (vkCreateBuffer)
COGL_VULKAN_EXT_FUNCTION (vkDestroyBuffer)
COGL_VULKAN_EXT_FUNCTION (vkCreateBufferView)
COGL_VULKAN_EXT_FUNCTION (vkDestroyBufferView)
COGL_VULKAN_EXT_FUNCTION (vkCreateImage)
COGL_VULKAN_EXT_FUNCTION (vkDestroyImage)
COGL_VULKAN_EXT_FUNCTION (vkGetImageSubresourceLayout)
COGL_VULKAN_EXT_FUNCTION (vkCreateImageView)
COGL_VULKAN_EXT_FUNCTION (vkDestroyImageView)
COGL_VULKAN_EXT_FUNCTION (vkCreateShaderModule)
COGL_VULKAN_EXT_FUNCTION (vkDestroyShaderModule)
COGL_VULKAN_EXT_FUNCTION (vkCreatePipelineCache)
COGL_VULKAN_EXT_FUNCTION (vkDestroyPipelineCache)
COGL_VULKAN_EXT_FUNCTION (vkGetPipelineCacheData)
COGL_VULKAN_EXT_FUNCTION (vkMergePipelineCaches)
COGL_VULKAN_EXT_FUNCTION (vkCreateGraphicsPipelines)
COGL_VULKAN_EXT_FUNCTION (vkCreateComputePipelines)
COGL_VULKAN_EXT_FUNCTION (vkDestroyPipeline)
COGL_VULKAN_EXT_FUNCTION (vkCreatePipelineLayout)
COGL_VULKAN_EXT_FUNCTION (vkDestroyPipelineLayout)
COGL_VULKAN_EXT_FUNCTION (vkCreateSampler)
COGL_VULKAN_EXT_FUNCTION (vkDestroySampler)
COGL_VULKAN_EXT_FUNCTION (vkCreateDescriptorSetLayout)
COGL_VULKAN_EXT_FUNCTION (vkDestroyDescriptorSetLayout)
COGL_VULKAN_EXT_FUNCTION (vkCreateDescriptorPool)
COGL_VULKAN_EXT_FUNCTION (vkDestroyDescriptorPool)
COGL_VULKAN_EXT_FUNCTION (vkResetDescriptorPool)
COGL_VULKAN_EXT_FUNCTION (vkAllocateDescriptorSets)
COGL_VULKAN_EXT_FUNCTION (vkFreeDescriptorSets)
COGL_VULKAN_EXT_FUNCTION (vkUpdateDescriptorSets)
COGL_VULKAN_EXT_FUNCTION (vkCreateFramebuffer)
COGL_VULKAN_EXT_FUNCTION (vkDestroyFramebuffer)
COGL_VULKAN_EXT_FUNCTION (vkCreateRenderPass)
COGL_VULKAN_EXT_FUNCTION (vkDestroyRenderPass)
COGL_VULKAN_EXT_FUNCTION (vkGetRenderAreaGranularity)
COGL_VULKAN_EXT_FUNCTION (vkCreateCommandPool)
COGL_VULKAN_EXT_FUNCTION (vkDestroyCommandPool)
COGL_VULKAN_EXT_FUNCTION (vkResetCommandPool)
COGL_VULKAN_EXT_FUNCTION (vkAllocateCommandBuffers)
COGL_VULKAN_EXT_FUNCTION (vkFreeCommandBuffers)
COGL_VULKAN_EXT_FUNCTION (vkBeginCommandBuffer)
COGL_VULKAN_EXT_FUNCTION (vkEndCommandBuffer)
COGL_VULKAN_EXT_FUNCTION (vkResetCommandBuffer)
COGL_VULKAN_EXT_FUNCTION (vkCmdBindPipeline)
COGL_VULKAN_EXT_FUNCTION (vkCmdSetViewport)
COGL_VULKAN_EXT_FUNCTION (vkCmdSetScissor)
COGL_VULKAN_EXT_FUNCTION (vkCmdSetLineWidth)
COGL_VULKAN_EXT_FUNCTION (vkCmdSetDepthBias)
COGL_VULKAN_EXT_FUNCTION (vkCmdSetBlendConstants)
COGL_VULKAN_EXT_FUNCTION (vkCmdSetDepthBounds)
COGL_VULKAN_EXT_FUNCTION (vkCmdSetStencilCompareMask)
COGL_VULKAN_EXT_FUNCTION (vkCmdSetStencilWriteMask)
COGL_VULKAN_EXT_FUNCTION (vkCmdSetStencilReference)
COGL_VULKAN_EXT_FUNCTION (vkCmdBindDescriptorSets)
COGL_VULKAN_EXT_FUNCTION (vkCmdBindIndexBuffer)
COGL_VULKAN_EXT_FUNCTION (vkCmdBindVertexBuffers)
COGL_VULKAN_EXT_FUNCTION (vkCmdDraw)
COGL_VULKAN_EXT_FUNCTION (vkCmdDrawIndexed)
COGL_VULKAN_EXT_FUNCTION (vkCmdDrawIndirect)
COGL_VULKAN_EXT_FUNCTION (vkCmdDrawIndexedIndirect)
COGL_VULKAN_EXT_FUNCTION (vkCmdDispatch)
COGL_VULKAN_EXT_FUNCTION (vkCmdDispatchIndirect)
COGL_VULKAN_EXT_FUNCTION (vkCmdCopyBuffer)
COGL_VULKAN_EXT_FUNCTION (vkCmdCopyImage)
COGL_VULKAN_EXT_FUNCTION (vkCmdBlitImage)
COGL_VULKAN_EXT_FUNCTION (vkCmdCopyBufferToImage)
COGL_VULKAN_EXT_FUNCTION (vkCmdCopyImageToBuffer)
COGL_VULKAN_EXT_FUNCTION (vkCmdUpdateBuffer)
COGL_VULKAN_EXT_FUNCTION (vkCmdFillBuffer)
COGL_VULKAN_EXT_FUNCTION (vkCmdClearColorImage)
COGL_VULKAN_EXT_FUNCTION (vkCmdClearDepthStencilImage)
COGL_VULKAN_EXT_FUNCTION (vkCmdClearAttachments)
COGL_VULKAN_EXT_FUNCTION (vkCmdResolveImage)
COGL_VULKAN_EXT_FUNCTION (vkCmdSetEvent)
COGL_VULKAN_EXT_FUNCTION (vkCmdResetEvent)
COGL_VULKAN_EXT_FUNCTION (vkCmdWaitEvents)
COGL_VULKAN_EXT_FUNCTION (vkCmdPipelineBarrier)
COGL_VULKAN_EXT_FUNCTION (vkCmdBeginQuery)
COGL_VULKAN_EXT_FUNCTION (vkCmdEndQuery)
COGL_VULKAN_EXT_FUNCTION (vkCmdResetQueryPool)
COGL_VULKAN_EXT_FUNCTION (vkCmdWriteTimestamp)
COGL_VULKAN_EXT_FUNCTION (vkCmdCopyQueryPoolResults)
COGL_VULKAN_EXT_FUNCTION (vkCmdPushConstants)
COGL_VULKAN_EXT_FUNCTION (vkCmdBeginRenderPass)
COGL_VULKAN_EXT_FUNCTION (vkCmdNextSubpass)
COGL_VULKAN_EXT_FUNCTION (vkCmdEndRenderPass)
COGL_VULKAN_EXT_FUNCTION (vkCmdExecuteCommands)
COGL_VULKAN_EXT_END ()

COGL_VULKAN_EXT_BEGIN (swapchain_khr, 1, 0, 2, VK_KHR_SWAPCHAIN_EXTENSION_NAME)
COGL_VULKAN_EXT_FUNCTION (vkCreateSwapchainKHR)
COGL_VULKAN_EXT_FUNCTION (vkDestroySwapchainKHR)
COGL_VULKAN_EXT_FUNCTION (vkGetSwapchainImagesKHR)
COGL_VULKAN_EXT_FUNCTION (vkAcquireNextImageKHR)
COGL_VULKAN_EXT_FUNCTION (vkQueuePresentKHR)
COGL_VULKAN_EXT_END ()

COGL_VULKAN_EXT_BEGIN (surface_khr, 1, 0, 2, VK_KHR_SURFACE_EXTENSION_NAME)
COGL_VULKAN_EXT_FUNCTION (vkDestroySurfaceKHR)
COGL_VULKAN_EXT_FUNCTION (vkGetPhysicalDeviceSurfaceSupportKHR)
COGL_VULKAN_EXT_FUNCTION (vkGetPhysicalDeviceSurfaceCapabilitiesKHR)
COGL_VULKAN_EXT_FUNCTION (vkGetPhysicalDeviceSurfaceFormatsKHR)
COGL_VULKAN_EXT_FUNCTION (vkGetPhysicalDeviceSurfacePresentModesKHR)
COGL_VULKAN_EXT_END ()

#ifdef COGL_HAS_VULKAN_PLATFORM_WAYLAND_SUPPORT
COGL_VULKAN_EXT_BEGIN (wayland_surface_khr, 1, 0, 2,
                       VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME)
COGL_VULKAN_EXT_FUNCTION (vkCreateWaylandSurfaceKHR)
COGL_VULKAN_EXT_FUNCTION (vkGetPhysicalDeviceWaylandPresentationSupportKHR)
COGL_VULKAN_EXT_END ()
#endif

#ifdef COGL_HAS_VULKAN_PLATFORM_XLIB_SUPPORT
COGL_VULKAN_EXT_BEGIN (xlib_surface_khr, 1, 0, 2,
                       VK_KHR_XLIB_SURFACE_EXTENSION_NAME)
COGL_VULKAN_EXT_FUNCTION (vkCreateXlibSurfaceKHR)
COGL_VULKAN_EXT_FUNCTION (vkGetPhysicalDeviceXlibPresentationSupportKHR)
COGL_VULKAN_EXT_END ()
#endif
