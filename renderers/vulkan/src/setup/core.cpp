#pragma once

#include "keptech/vulkan/renderer.hpp"

#include "keptech/core/window.hpp"
#include "keptech/vulkan/helpers/device.hpp"
#include "keptech/vulkan/helpers/instance.hpp"
#include "keptech/vulkan/structs.hpp"
#include "macros.hpp"
#include <SDL3/SDL_vulkan.h>
#include <array>
#include <expected>
#include <keptech/components/camera.hpp>
#include <set>
#include <string>

#include "setup.hpp"

namespace kt::vkh::setup {
  using namespace kt::vkh;

  std::expected<std::array<Pools, 2>, std::string> createPools(std::set<uint32_t>& uniqueQueueFamilies, const QueueIndices& queueIndices,
                                                               VkDevice& device, const Queues& queues) {
    Pools pools1;
    Pools pools2;

    std::array<Pools*, 2> poolsArray = {&pools1, &pools2};

    VkCommandPoolCreateInfo poolCreateInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
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
        Pools& pools = *poolsArray[i];

        VkCommandPool vkpool;
        VK_MAKE(vkCreateCommandPool(device, &poolCreateInfo, nullptr, &vkpool), "Failed to create command pool.");

        CommandPool pool{.pool = vkpool, .queue = queue};

        if (familyIndex == queueIndices.graphics) {
          pools.graphics = pool;
        }
        if (familyIndex == queueIndices.compute) {
          pools.compute = pool;
        }
      }
    }

    return std::move(std::array<Pools, 2>{std::move(pools1), std::move(pools2)});
  }

  std::expected<Renderer::VulkanCore, std::string> createVulkanCore(const RendererCreateInfo& createInfo,
                                                                    const core::window::Window& window) {

    constexpr bool enableValidationLayers =
#ifndef NDEBUG
        true;
#else
        false;
#endif

    auto instance_res = createInstance(createInfo.applicationName, enableValidationLayers);
    if (!instance_res) {
      return std::unexpected(instance_res.error());
    }
    auto& instance = instance_res.value();

    VkSurfaceKHR surface = nullptr;
    if (!SDL_Vulkan_CreateSurface(window.getHandle(), instance, nullptr, &surface)) {
      return std::unexpected("Failed to create Vulkan surface from SDL window.");
    }

    VKH_MAKE(physDevice, createPhysicalDevice(instance, surface), "Failed to create physical device.");

    VKH_MAKE(queueIndices, findQueues(physDevice, surface), "Failed to find required queue families.");

    std::set<uint32_t> uniqueQueueFamilies = {queueIndices.graphics, queueIndices.present, queueIndices.compute, queueIndices.transfer};

    VKH_MAKE(device, createDevice(physDevice, uniqueQueueFamilies), "Failed to create logical device.");

    VKH_MAKE(queues, getQueues(device, queueIndices, uniqueQueueFamilies), "Failed to get device queues.");

    VmaAllocator allocator;
    VmaAllocatorCreateInfo allocInfo{
        .flags = VmaAllocatorCreateFlagBits::VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice = physDevice,
        .device = device,
        .instance = instance,
    };
    VK_MAKE(vmaCreateAllocator(&allocInfo, &allocator), "Failed to create VMA allocator.");

    VKH_MAKE(swapchain, createSwapchain(physDevice, window.getRenderSize(), device, surface, queues, std::nullopt),
             "Failed to create swapchain.");

    VkCommandPool transferPool;
    VkCommandPoolCreateInfo poolCreateInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VkCommandPoolCreateFlagBits::VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
        .queueFamilyIndex = queueIndices.transfer,
    };
    VK_MAKE(vkCreateCommandPool(device, &poolCreateInfo, nullptr, &transferPool), "Failed to create transfer command pool.");

    VKH_MAKE(poolsArray, createPools(uniqueQueueFamilies, queueIndices, device, queues), "Failed to create command pools.");

    CommandPool transferPoolStruct{
        .pool = transferPool,
        .queue = queues.transfer,
    };

    VkFence fence1;
    VkFence fence2;
    VkFenceCreateInfo fenceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VkFenceCreateFlagBits::VK_FENCE_CREATE_SIGNALED_BIT,
    };
    VK_MAKE(vkCreateFence(device, &fenceCreateInfo, nullptr, &fence1), "Failed to create fence1.");
    VK_MAKE(vkCreateFence(device, &fenceCreateInfo, nullptr, &fence2), "Failed to create fence2.")

    VkSemaphore sem1;
    VkSemaphore sem2;
    VkSemaphoreCreateInfo semaphoreCreateInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    VK_MAKE(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &sem1), "Failed to create sem1");
    VK_MAKE(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &sem2), "Failed to create sem2");

    VkSemaphoreTypeCreateInfo timelineCreateInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
        .semaphoreType = VkSemaphoreType::VK_SEMAPHORE_TYPE_TIMELINE,
        .initialValue = 0,
    };
    VkSemaphoreCreateInfo timelineSemaphoreCreateInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = &timelineCreateInfo,
    };

    VkSemaphore timelineSem1;
    VkSemaphore timelineSem2;
    VK_MAKE(vkCreateSemaphore(device, &timelineSemaphoreCreateInfo, nullptr, &timelineSem1), "Failed to create timeline sem1");
    VK_MAKE(vkCreateSemaphore(device, &timelineSemaphoreCreateInfo, nullptr, &timelineSem2), "Failed to create timeline sem2");

    return Renderer::VulkanCore{
        .instance = instance,
        .surface = surface,
        .device = Device{.physical = physDevice, .logical = device},
        .allocator = allocator,
        .queues = queues,
        .swapchain = std::move(swapchain),
        .perFrame =
            {
                Renderer::PerFrame{
                    .inFlightFence = fence1,
                    .imageAvailableSemaphore = sem1,
                    .timelineSemaphore = timelineSem1,
                    .pools = poolsArray[0],
                },
                Renderer::PerFrame{
                    .inFlightFence = fence2,
                    .imageAvailableSemaphore = sem2,
                    .timelineSemaphore = timelineSem2,
                    .pools = poolsArray[1],
                },
            },
        .transferPool = transferPoolStruct,
    };
  }
} // namespace kt::vkh::setup
