#pragma once

#include "keptech/vulkan/renderer.hpp"

#include "keptech/core/window.hpp"
#include "keptech/vulkan/helpers/device.hpp"
#include "keptech/vulkan/helpers/instance.hpp"
#include "keptech/vulkan/structs.hpp"
#include "macros.hpp"
#include "setup/swapchain.hpp"
#include "vulkan/vulkan.hpp"
#include <SDL3/SDL_vulkan.h>
#include <array>
#include <expected>
#include <keptech/core/components/camera.hpp>
#include <set>
#include <string>

#include "device.hpp"

namespace keptech::vkh::setup {
  using namespace keptech::vkh;

  std::expected<std::array<RendererBackend::Pools, 2>, std::string>
  createPools(std::set<uint32_t>& uniqueQueueFamilies,
              const QueueIndices& queueIndices, vk::raii::Device& device,
              const RendererBackend::Queues& queues) {
    RendererBackend::Pools pools1;
    RendererBackend::Pools pools2;

    std::array<RendererBackend::Pools*, 2> poolsArray = {&pools1, &pools2};

    vk::CommandPoolCreateInfo poolCreateInfo{
        .flags = vk::CommandPoolCreateFlagBits::eTransient,
    };
    for (uint32_t familyIndex : uniqueQueueFamilies) {
      poolCreateInfo.queueFamilyIndex = familyIndex;

      Queue queue;
      if (familyIndex == queueIndices.graphics) {
        queue = queues.graphics;
      } else if (familyIndex == queueIndices.present) {
        queue = queues.present;
      } else if (familyIndex == queueIndices.compute) {
        queue = queues.compute;
      } else {
        continue;
      }

      for (int i = 0; i < 2; i++) {
        RendererBackend::Pools& pools = *poolsArray[i];

        VK_MAKE(poolRaii, device.createCommandPool(poolCreateInfo),
                "Failed to create command pool.");

        std::shared_ptr pool = std::make_shared<CommandPool>(std::move(
            CommandPool{.pool = std::move(poolRaii), .queue = queue}));

        if (familyIndex == queueIndices.graphics) {
          pools.graphics = pool;
        }
        if (familyIndex == queueIndices.compute) {
          pools.compute = pool;
        }
      }
    }

    return std::move(std::array<RendererBackend::Pools, 2>{std::move(pools1),
                                                           std::move(pools2)});
  }

  std::expected<RendererBackend::VulkanCore, std::string>
  createVulkanCore(const RendererCreateInfo& createInfo,
                   const core::window::Window& window) {
    auto context = vk::raii::Context{};

#ifndef NDEBUG
    constexpr bool enableValidationLayers = true;
#else
    constexpr bool enableValidationLayers = false;
#endif

    auto instance_res = createInstance(context, createInfo.applicationName,
                                       enableValidationLayers);
    if (!instance_res) {
      return std::unexpected(instance_res.error());
    }
    auto& instance = instance_res.value();

    VkSurfaceKHR rawSurface = nullptr;
    if (!SDL_Vulkan_CreateSurface(window.getHandle(),
                                  static_cast<VkInstance>(*instance), nullptr,
                                  &rawSurface)) {
      return std::unexpected(
          "Failed to create Vulkan surface from SDL window.");
    }
    vk::raii::SurfaceKHR surface{instance, rawSurface};

    VKH_MAKE(physDevice, createPhysicalDevice(instance, surface),
             "Failed to create physical device.");

    VKH_MAKE(queueIndices, findQueues(physDevice, surface),
             "Failed to find required queue families.");

    std::set<uint32_t> uniqueQueueFamilies = {
        queueIndices.graphics, queueIndices.present, queueIndices.compute,
        queueIndices.transfer};

    VKH_MAKE(device, createDevice(physDevice, uniqueQueueFamilies),
             "Failed to create logical device.");

    VKH_MAKE(queues, getQueues(device, queueIndices, uniqueQueueFamilies),
             "Failed to get device queues.");

    VMA_MAKE(allocator,
             vma::createAllocator(vma::AllocatorCreateInfo{
                 .flags = vma::AllocatorCreateFlagBits::eBufferDeviceAddress,
                 .physicalDevice = *physDevice,
                 .device = *device,
                 .instance = *instance,
             }),
             "Failed to create VMA allocator.");

    VKH_MAKE(swapchain,
             createSwapchain(physDevice, window.getRenderSize(), device,
                             surface, queues, std::nullopt),
             "Failed to create swapchain.");

    VK_MAKE(transferPool,
            device.createCommandPool(vk::CommandPoolCreateInfo{
                .flags = vk::CommandPoolCreateFlagBits::eTransient,
                .queueFamilyIndex = queueIndices.transfer,
            }),
            "Failed to create transfer command pool.");

    VKH_MAKE(poolsArray,
             createPools(uniqueQueueFamilies, queueIndices, device, queues),
             "Failed to create command pools.");

    CommandPool transferPoolStruct{
        .pool = std::move(transferPool),
        .queue = queues.transfer,
    };

    VK_MAKE(fence1,
            device.createFence({.flags = vk::FenceCreateFlagBits::eSignaled}),
            "Failed to create fence1.");
    VK_MAKE(fence2,
            device.createFence({.flags = vk::FenceCreateFlagBits::eSignaled}),
            "Failed to create fence2.")

    VK_MAKE(sem1, device.createSemaphore({}), "Failed to create sem1");
    VK_MAKE(sem2, device.createSemaphore({}), "Failed to create sem2");

    vk::SemaphoreTypeCreateInfo timelineCreateInfo{
        .semaphoreType = vk::SemaphoreType::eTimeline,
        .initialValue = 0,
    };

    VK_MAKE(timelineSem1,
            device.createSemaphore(vk::SemaphoreCreateInfo{
                .pNext = &timelineCreateInfo,
            }),
            "Failed to create timeline sem1");
    VK_MAKE(timelineSem2,
            device.createSemaphore(vk::SemaphoreCreateInfo{
                .pNext = &timelineCreateInfo,
            }),
            "Failed to create timeline sem2");

    return RendererBackend::VulkanCore{
        .context = std::move(context),
        .instance = std::move(instance),
        .surface = std::move(surface),
        .device = Device{.physical = std::move(physDevice),
                         .logical = std::move(device)},
        .allocator = allocator,
        .queues = std::move(queues),
        .swapchain = std::move(swapchain),
        .perFrame =
            {
                RendererBackend::PerFrame{
                    .inFlightFence = std::move(fence1),
                    .imageAvailableSemaphore = std::move(sem1),
                    .timelineSemaphore = std::move(timelineSem1),
                    .pools = std::move(poolsArray[0]),
                },
                RendererBackend::PerFrame{
                    .inFlightFence = std::move(fence2),
                    .imageAvailableSemaphore = std::move(sem2),
                    .timelineSemaphore = std::move(timelineSem2),
                    .pools = std::move(poolsArray[1]),
                },
            },
        .transferPool = std::move(transferPoolStruct),
    };
  }
} // namespace keptech::vkh::setup
