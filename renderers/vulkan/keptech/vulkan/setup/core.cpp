#include "setup.hpp"

#include "keptech/core/window.hpp"
#include "keptech/rendering/renderer.hpp"
#include "keptech/vulkan/structs.hpp"
#include "keptech/vulkan/wrappers/device.hpp"
#include "macros.hpp"
#include "renderer.hpp"
#include <SDL3/SDL_vulkan.h>
#include <Volk/volk.h>
#include <array>
#include <expected>
#include <keptech/components/camera.hpp>
#include <set>
#include <string>

namespace kt::vkh {
  using namespace kt::vkh;

  std::expected<void, std::string> Renderer::initVulkanCore(const RendererCreateInfo& createInfo, const core::window::Window& window) {

    VK_CHECK(volkInitialize(), "Failed to initialize Volk.");

    constexpr bool enableValidationLayers =
#ifndef NDEBUG
        true;
#else
        false;
#endif

    auto instance_res = Instance::create(createInfo.applicationName, enableValidationLayers);
    if (!instance_res) {
      return std::unexpected(instance_res.error());
    }
    m.vkcore.instance = instance_res.value();

    if (!SDL_Vulkan_CreateSurface(window.getHandle(), m.vkcore.instance, nullptr, &m.vkcore.surface)) {
      return std::unexpected("Failed to create Vulkan surface from SDL window.");
    }

    auto device_res = initDevice(createInfo);
    if (!device_res) {
      return std::unexpected(device_res.error());
    }
    auto& uniqueQueueFamilies = device_res.value();

    m.vkcore.transferPool.queue = m.vkcore.queues.transfer;

    auto swapchain_res =
        setup::createSwapchain(m.vkcore.device, m.vkcore.device, m.window->getRenderSize(), m.vkcore.surface, m.vkcore.queues, nullptr);
    if (!swapchain_res) {
      return std::unexpected(swapchain_res.error());
    }
    m.vkcore.swapchain = std::move(swapchain_res.value());

    auto pools_res = initCommandPools(uniqueQueueFamilies);
    if (!pools_res) {
      return std::unexpected(pools_res.error());
    }

    auto sync_res = initSync();
    if (!sync_res) {
      return std::unexpected(sync_res.error());
    }

    return {};
  }

  std::expected<void, std::string> Renderer::initCommandPools(const std::set<uint32_t>& uniqueQueueFamilies) {
    setup::QueueIndices queueIndices{
        .graphics = m.vkcore.queues.graphics.index,
        .present = m.vkcore.queues.present.index,
        .compute = m.vkcore.queues.compute.index,
        .transfer = m.vkcore.queues.transfer.index,
    };
    VkCommandPoolCreateInfo poolCreateInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
        .queueFamilyIndex = 0,
    };

    auto makePool = [&](VkCommandPool& p1, VkCommandPool& p2) {
      VK_CHECK(vkCreateCommandPool(m.vkcore.device, &poolCreateInfo, nullptr, &p1), "Failed to create first command pool.");
      VK_CHECK(vkCreateCommandPool(m.vkcore.device, &poolCreateInfo, nullptr, &p2), "Failed to create second command pool.");
    };

    auto& pools1 = m.vkcore.perFrame[0].pools;
    auto& pools2 = m.vkcore.perFrame[1].pools;

    for (uint32_t familyIndex : uniqueQueueFamilies) {
      poolCreateInfo.queueFamilyIndex = familyIndex;

      if (familyIndex == queueIndices.graphics) {
        pools1.graphics.queue = m.vkcore.queues.graphics;
        pools2.graphics.queue = m.vkcore.queues.graphics;
        makePool(pools1.graphics.pool, pools2.graphics.pool);
        if (queueIndices.graphics == queueIndices.compute) {
          pools1.compute = pools1.graphics;
          pools2.compute = pools2.graphics;
        }
      } else if (familyIndex == queueIndices.compute) {
        pools1.compute.queue = m.vkcore.queues.compute;
        pools2.compute.queue = m.vkcore.queues.compute;
        makePool(pools1.compute.pool, pools2.compute.pool);
      }
    }

    poolCreateInfo.queueFamilyIndex = queueIndices.transfer;
    VK_CHECK(vkCreateCommandPool(m.vkcore.device, &poolCreateInfo, nullptr, &m.vkcore.transferPool.pool),
             "Failed to create transfer command pool.");

    VK_ASSERT(pools1.graphics.pool != nullptr, "Graphics command pool was not created.");
    VK_ASSERT(pools1.compute.pool != nullptr, "Compute command pool was not created.");
    VK_ASSERT(pools2.graphics.pool != nullptr, "Graphics command pool was not created.");
    VK_ASSERT(pools2.compute.pool != nullptr, "Compute command pool was not created.");

    return {};
  }

  std::expected<void, std::string> Renderer::initSync() {
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      auto& perFrame = m.vkcore.perFrame[i];
      VkFenceCreateInfo fenceCreateInfo{
          .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
          .flags = VkFenceCreateFlagBits::VK_FENCE_CREATE_SIGNALED_BIT,
      };
      VK_CHECK(vkCreateFence(m.vkcore.device, &fenceCreateInfo, nullptr, &perFrame.inFlightFence), "Failed to create fence1.");

      VkSemaphoreCreateInfo semaphoreCreateInfo{
          .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      };
      VK_CHECK(vkCreateSemaphore(m.vkcore.device, &semaphoreCreateInfo, nullptr, &perFrame.imageAvailableSemaphore),
               "Failed to create image available semaphore");
    }

    VkSemaphoreTypeCreateInfo timelineSemaphoreTypeInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
        .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
        .initialValue = 0,
    };
    VkSemaphoreCreateInfo timelineSemaphoreCreateInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = &timelineSemaphoreTypeInfo,
    };

    VK_CHECK(vkCreateSemaphore(m.vkcore.device, &timelineSemaphoreCreateInfo, nullptr, &m.vkcore.mainSemaphore.semaphore),
             "Failed to create timeline semaphore.");
    VK_CHECK(vkCreateSemaphore(m.vkcore.device, &timelineSemaphoreCreateInfo, nullptr, &m.vkcore.transferSemaphore.semaphore),
             "Failed to create timeline semaphore.");

    return {};
  }
} // namespace kt::vkh
