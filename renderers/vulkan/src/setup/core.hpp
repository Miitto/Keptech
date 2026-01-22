#pragma once

#include "keptech/core/renderer.hpp"
#include "keptech/vulkan/renderer.hpp"

#include "keptech/core/window.hpp"
#include "keptech/vulkan/helpers/device.hpp"
#include "keptech/vulkan/helpers/instance.hpp"
#include "keptech/vulkan/structs.hpp"
#include "macros.hpp"
#include "setup/gbuffers.hpp"
#include "vulkan/vulkan.hpp"
#include <SDL3/SDL_vulkan.h>
#include <array>
#include <expected>
#include <keptech/core/components/camera.hpp>
#include <set>
#include <string>

#include "device.hpp"
#include "swapchain.hpp"

namespace keptech::vkh::setup {
  using namespace keptech::vkh;

  std::expected<std::array<Renderer::Pools, 2>, std::string>
  createPools(std::set<uint32_t>& uniqueQueueFamilies,
              const QueueIndices& queueIndices, vk::raii::Device& device,
              const Renderer::Queues& queues) {
    Renderer::Pools pools1;
    Renderer::Pools pools2;

    std::array<Renderer::Pools*, 2> poolsArray = {&pools1, &pools2};

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
        Renderer::Pools& pools = *poolsArray[i];

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

    return std::move(
        std::array<Renderer::Pools, 2>{std::move(pools1), std::move(pools2)});
  }

  std::expected<Renderer::VulkanCore, std::string>
  createVulkanCore(const core::renderer::CreateInfo& createInfo,
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

    VKH_MAKE(
        gBuffer,
        createGBuffer(allocator, device, physDevice, swapchain.config().extent),
        "Failed to create GBuffer.");

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

    VKH_MAKE(instanceBuffers1,
             Renderer::InstanceBuffers::create(allocator, device, 10),
             "Failed to create instance buffers.");

    VKH_MAKE(instanceBuffers2,
             Renderer::InstanceBuffers::create(allocator, device, 10),
             "Failed to create instance buffers.");

    return Renderer::VulkanCore{
        .context = std::move(context),
        .instance = std::move(instance),
        .surface = std::move(surface),
        .device = Device{.physical = std::move(physDevice),
                         .logical = std::move(device)},
        .allocator = allocator,
        .queues = std::move(queues),
        .swapchain = std::move(swapchain),
        .gBuffer = gBuffer,
        .perFrame = {Renderer::PerFrame{
                         .inFlightFence = std::move(fence1),
                         .imageAvailableSemaphore = std::move(sem1),
                         .pools = std::move(poolsArray[0]),
                         .instanceBuffers = instanceBuffers1,
                     },
                     Renderer::PerFrame{.inFlightFence = std::move(fence2),
                                        .imageAvailableSemaphore =
                                            std::move(sem2),
                                        .pools = std::move(poolsArray[1]),
                                        .instanceBuffers = instanceBuffers2}},
        .transferPool = std::move(transferPoolStruct),
    };
  }
} // namespace keptech::vkh::setup
