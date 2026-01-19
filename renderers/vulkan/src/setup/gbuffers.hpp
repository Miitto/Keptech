#pragma once

#include "keptech/vulkan/renderer.hpp"

#include "keptech/vulkan/structs.hpp"
#include "macros.hpp"
#include <SDL3/SDL_vulkan.h>
#include <expected>
#include <keptech/core/components/camera.hpp>
#include <vk_mem_alloc.hpp>

namespace keptech::vkh::setup {
  using namespace keptech::vkh;

  std::expected<Renderer::GBuffers, std::string>
  createGBuffer(vma::Allocator& alloc, vk::raii::Device& device,
                vk::raii::PhysicalDevice& physicalDevice,
                const vk::Extent2D& extent) {
    Renderer::GBuffers gbuffers{};

    vma::AllocationCreateInfo allocCreateInfo{
        .usage = vma::MemoryUsage::eGpuOnly,
    };

    vk::ImageViewCreateInfo viewCreateInfo{
        .viewType = vk::ImageViewType::e2D,
        .subresourceRange =
            vk::ImageSubresourceRange{
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };

    vk::ImageCreateInfo baseImageCreateInfo{
        .imageType = vk::ImageType::e2D,
        .extent = vk::Extent3D{.width = extent.width,
                               .height = extent.height,
                               .depth = 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = vk::SampleCountFlagBits::e1,
        .tiling = vk::ImageTiling::eOptimal,
        .usage = vk::ImageUsageFlagBits::eInputAttachment |
                 vk::ImageUsageFlagBits::eSampled,
        .sharingMode = vk::SharingMode::eExclusive,
        .initialLayout = vk::ImageLayout::eUndefined,
    };

    vk::ImageCreateInfo albedoCreateInfo = baseImageCreateInfo;
    albedoCreateInfo.format = vk::Format::eR8G8B8A8Unorm;
    albedoCreateInfo.usage |= vk::ImageUsageFlagBits::eColorAttachment;

    VKH_MAKE(albedo,
             AllocatedImage::create(alloc, device, albedoCreateInfo,
                                    allocCreateInfo, viewCreateInfo, true),
             "Failed to create GBuffer albedo image.");

    vk::ImageCreateInfo normalCreateInfo = baseImageCreateInfo;
    normalCreateInfo.format = vk::Format::eR8G8B8A8Unorm;
    normalCreateInfo.usage |= vk::ImageUsageFlagBits::eColorAttachment;

    VKH_MAKE(normal,
             AllocatedImage::create(alloc, device, normalCreateInfo,
                                    allocCreateInfo, viewCreateInfo, true),
             "Failed to create GBuffer normal image.");

    vk::ImageCreateInfo depthCreateInfo = baseImageCreateInfo;
    depthCreateInfo.format = vk::Format::eD16Unorm;
    depthCreateInfo.usage |= vk::ImageUsageFlagBits::eDepthStencilAttachment;
    viewCreateInfo.subresourceRange.aspectMask =
        vk::ImageAspectFlagBits::eDepth;

    VKH_MAKE(depth,
             AllocatedImage::create(alloc, device, depthCreateInfo,
                                    allocCreateInfo, viewCreateInfo, true),
             "Failed to create GBuffer depth image.");

    return Renderer::GBuffers{
        .color = albedo,
        .normal = normal,
        .depth = depth,
    };
  }
} // namespace keptech::vkh::setup
