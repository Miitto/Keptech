#pragma once

#include "keptech/vulkan/helpers/physicalDeviceSelector.hpp"
#include "keptech/vulkan/helpers/queueFinder.hpp"
#include "keptech/vulkan/structs.hpp"
#include "macros.hpp"
#include "setup.hpp"
#include <SDL3/SDL_vulkan.h>
#include <algorithm>
#include <expected>
#include <keptech/components/camera.hpp>
#include <set>

namespace kt::vkh::setup {
  using namespace kt::vkh;
  constexpr std::array<const char*, 2> REQUIRED_DEVICE_EXTENSIONS = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
      VK_KHR_SPIRV_1_4_EXTENSION_NAME,
  };

  std::expected<VkPhysicalDevice, std::string> createPhysicalDevice(VkInstance instance, VkSurfaceKHR surface) {
    VKH_MAKE(selector, kt::vkh::PhysicalDeviceSelector::create(instance), "Failed to create physical device selector.");

    selector.requireVersion(1, 4, 0);
    selector.requireExtensions(REQUIRED_DEVICE_EXTENSIONS);
    selector.requireQueueFamily(VkQueueFlagBits::VK_QUEUE_GRAPHICS_BIT | VkQueueFlagBits::VK_QUEUE_COMPUTE_BIT);

    selector.scoreDevices([](auto& specs) {
      constexpr uint32_t DEDICATED_GPU_BONUS = 1000;

      uint32_t score = 1;

      if (specs.properties.deviceType == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        score += DEDICATED_GPU_BONUS;
      }

      return score;
    });

    auto devices = selector.select();
    if (devices.empty()) {
      return std::unexpected("No suitable physical devices found.");
    }
    auto& physDevice = devices.front();

    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physDevice, &properties);
    VK_INFO("Selected physical device: {}", properties.deviceName);

    return physDevice;
  }

  std::expected<QueueIndices, std::string> findQueues(VkPhysicalDevice physDevice, VkSurfaceKHR surface) {
    QueueFinder finder{physDevice};

    auto getGraphicsPresentQueues = [&]() -> std::expected<QueueIndices, std::string> {
      // Prefer a graphics + present
      auto combinedFinder = finder.findCombined({
          {QueueFinder::QueueType{
              .type = QueueFinder::QueueTypeFlags::Graphics,
          }},
          {QueueFinder::QueueType{
              .type = QueueFinder::QueueTypeFlags::Present,
              .params =
                  QueueFinder::QueueTypeParams{
                      .presentQueue =
                          {
                              .device = physDevice,
                              .surface = surface,
                          },
                  },
          }},
      });

      if (combinedFinder.hasQueue()) {
        auto& queue = combinedFinder.first();
        return QueueIndices{.graphics = queue.index, .present = queue.index};
      }

      QueueIndices ind{};

      auto graphicsFinder = finder.findType(QueueFinder::QueueType{.type = QueueFinder::QueueTypeFlags::Graphics});

      if (graphicsFinder.hasQueue()) {
        auto queue = graphicsFinder.first();
        ind.graphics = queue.index;
      } else {
        return std::unexpected("No graphics queue family found");
      }

      auto presentFinder = finder.findType(
          QueueFinder::QueueType{.type = QueueFinder::QueueTypeFlags::Present,
                                 .params = QueueFinder::QueueTypeParams{.presentQueue = {.device = physDevice, .surface = surface}}});

      if (presentFinder.hasQueue()) {
        auto presentFamily = presentFinder.first();
        ind.present = presentFamily.index;
      } else {
        return std::unexpected("No present queue family found");
      }

      return ind;
    };

    VKH_MAKE(queueIndices, getGraphicsPresentQueues(), "Failed to get core queues");

    auto computeFinder = finder.findType({.type = QueueFinder::QueueTypeFlags::Compute});
    if (!computeFinder.hasQueue())
      return std::unexpected("No compute queue found");

    queueIndices.compute = computeFinder.first().index;

    auto transferFinder = finder.findType({.type = QueueFinder::QueueTypeFlags::Transfer})
                              .filterTypes(QueueFinder::QueueTypeFlags::Graphics | QueueFinder::QueueTypeFlags::Compute);
    if (transferFinder.hasQueue()) {
      queueIndices.transfer = transferFinder.first().index;
    } else {
      queueIndices.transfer = queueIndices.graphics;
    }

    return queueIndices;
  }

  std::expected<VkDevice, std::string> createDevice(VkPhysicalDevice physDevice, const std::set<uint32_t>& uniqueQueueFamilies) {

    constexpr float priority = 1.f;

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfo{};
    queueCreateInfo.reserve(uniqueQueueFamilies.size());
    for (auto familyIndex : uniqueQueueFamilies) {
      queueCreateInfo.push_back({
          .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
          .queueFamilyIndex = familyIndex,
          .queueCount = 1,
          .pQueuePriorities = &priority,
      });
    }

    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT extendedDynamicStateFeatures{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT,
        .extendedDynamicState = true,
    };
    VkPhysicalDeviceVulkan13Features vulkan13Features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = &extendedDynamicStateFeatures,
        .synchronization2 = true,
        .dynamicRendering = true,
    };
    VkPhysicalDeviceVulkan12Features vulkan12Features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = &vulkan13Features,
        .descriptorIndexing = true,
        .descriptorBindingUniformBufferUpdateAfterBind = true,
        .descriptorBindingSampledImageUpdateAfterBind = true,
        .descriptorBindingPartiallyBound = true,
        .runtimeDescriptorArray = true,
        .timelineSemaphore = true,
        .bufferDeviceAddress = true,
    };
    VkPhysicalDeviceVulkan11Features vulkan11Features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
        .pNext = &vulkan12Features,
        .shaderDrawParameters = true,
    };
    VkPhysicalDeviceFeatures2 deviceFeatures{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &vulkan11Features,
    };

    VkDeviceCreateInfo deviceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &deviceFeatures,
        .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfo.size()),
        .pQueueCreateInfos = queueCreateInfo.data(),
        .enabledExtensionCount = static_cast<uint32_t>(REQUIRED_DEVICE_EXTENSIONS.size()),
        .ppEnabledExtensionNames = REQUIRED_DEVICE_EXTENSIONS.data(),
    };

    VkDevice device;
    VK_MAKE(vkCreateDevice(physDevice, &deviceCreateInfo, nullptr, &device), "Failed to create logical device.");

    return device;
  }

  std::expected<Queues, std::string> getQueues(VkDevice& device, QueueIndices& queueIndices,
                                               const std::set<uint32_t>& uniqueQueueFamilies) {
    std::vector<Queue> queues{};
    for (uint32_t familyIndex : uniqueQueueFamilies) {
      VkQueue vkQueue;
      vkGetDeviceQueue(device, familyIndex, 0, &vkQueue);
      Queue queue{.index = familyIndex, .queue = vkQueue};
      queues.push_back(queue);
    }

    Queue& graphicsQueue = *std::ranges::find_if(queues, [&](const Queue& q) { return q.index == queueIndices.graphics; });
    Queue& presentQueue = *std::ranges::find_if(queues, [&](const Queue& q) { return q.index == queueIndices.present; });
    Queue& computeQueue = *std::ranges::find_if(queues, [&](const Queue& q) { return q.index == queueIndices.compute; });
    Queue& transferQueue = *std::ranges::find_if(queues, [&](const Queue& q) { return q.index == queueIndices.transfer; });

    return Queues{
        .graphics = graphicsQueue,
        .present = presentQueue,
        .compute = computeQueue,
        .transfer = transferQueue,
    };
  }
} // namespace kt::vkh::setup
