#pragma once

#include "keptech/vulkan/renderer.hpp"

#include "keptech/vulkan/helpers/physicalDeviceSelector.hpp"
#include "keptech/vulkan/helpers/queueFinder.hpp"
#include "keptech/vulkan/structs.hpp"
#include "macros.hpp"
#include "vulkan/vulkan.hpp"
#include <SDL3/SDL_vulkan.h>
#include <algorithm>
#include <expected>
#include <keptech/core/components/camera.hpp>
#include <set>

namespace keptech::vkh::setup {
  using namespace keptech::vkh;
  constexpr std::array<const char*, 3> REQUIRED_DEVICE_EXTENSIONS = {
      vk::KHRSwapchainExtensionName,
      vk::KHRSpirv14ExtensionName,
      vk::KHRCreateRenderpass2ExtensionName,
  };

  std::expected<vk::raii::PhysicalDevice, std::string>
  createPhysicalDevice(vk::raii::Instance& instance,
                       vk::raii::SurfaceKHR& surface) {
    VKH_MAKE(selector, keptech::vkh::PhysicalDeviceSelector::create(instance),
             "Failed to create physical device selector.");

    selector.requireVersion(1, 4, 0);
    selector.requireExtensions(REQUIRED_DEVICE_EXTENSIONS);
    selector.requireQueueFamily(vk::QueueFlagBits::eGraphics |
                                vk::QueueFlagBits::eCompute);

    selector.scoreDevices([](auto& specs) {
      constexpr uint32_t DEDICATED_GPU_BONUS = 1000;

      uint32_t score = 1;

      if (specs.properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
        score += DEDICATED_GPU_BONUS;
      }

      return score;
    });

    auto devices = selector.select();
    if (devices.empty()) {
      return std::unexpected("No suitable physical devices found.");
    }
    auto& physDevice = devices.front();

    VK_INFO("Selected physical device: {}",
            physDevice.getProperties().deviceName.data());

    return std::move(physDevice);
  }

  struct QueueIndices {
    uint32_t graphics;
    uint32_t present;
    uint32_t compute = std::numeric_limits<uint32_t>::max();
    uint32_t transfer = std::numeric_limits<uint32_t>::max();
  };

  std::expected<QueueIndices, std::string>
  findQueues(vk::raii::PhysicalDevice& physDevice,
             vk::raii::SurfaceKHR& surface) {
    QueueFinder finder{physDevice};

    auto getGraphicsPresentQueues =
        [&]() -> std::expected<QueueIndices, std::string> {
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

      auto graphicsFinder = finder.findType(QueueFinder::QueueType{
          .type = QueueFinder::QueueTypeFlags::Graphics});

      if (graphicsFinder.hasQueue()) {
        auto queue = graphicsFinder.first();
        ind.graphics = queue.index;
      } else {
        return std::unexpected("No graphics queue family found");
      }

      auto presentFinder = finder.findType(QueueFinder::QueueType{
          .type = QueueFinder::QueueTypeFlags::Present,
          .params = QueueFinder::QueueTypeParams{
              .presentQueue = {.device = physDevice, .surface = surface}}});

      if (presentFinder.hasQueue()) {
        auto presentFamily = presentFinder.first();
        ind.present = presentFamily.index;
      } else {
        return std::unexpected("No present queue family found");
      }

      return ind;
    };

    VKH_MAKE(queueIndices, getGraphicsPresentQueues(),
             "Failed to get core queues");

    auto computeFinder =
        finder.findType({.type = QueueFinder::QueueTypeFlags::Compute});
    if (!computeFinder.hasQueue())
      return std::unexpected("No compute queue found");

    queueIndices.compute = computeFinder.first().index;

    auto transferFinder =
        finder.findType({.type = QueueFinder::QueueTypeFlags::Transfer})
            .filterTypes(QueueFinder::QueueTypeFlags::Graphics |
                         QueueFinder::QueueTypeFlags::Compute);
    if (transferFinder.hasQueue()) {
      queueIndices.transfer = transferFinder.first().index;
    } else {
      queueIndices.transfer = queueIndices.graphics;
    }

    return queueIndices;
  }

  std::expected<vk::raii::Device, std::string>
  createDevice(vk::raii::PhysicalDevice& physDevice,
               const std::set<uint32_t>& uniqueQueueFamilies) {

    constexpr float priority = 1.f;

    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfo{};
    queueCreateInfo.reserve(uniqueQueueFamilies.size());
    for (auto familyIndex : uniqueQueueFamilies) {
      queueCreateInfo.push_back({
          .queueFamilyIndex = familyIndex,
          .queueCount = 1,
          .pQueuePriorities = &priority,
      });
    }

    const vk::StructureChain<
        vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features,
        vk::PhysicalDeviceVulkan12Features, vk::PhysicalDeviceVulkan13Features,
        vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
        ENGINE_DEVICE_EXTENSIONS = {
            {},
            {
                .shaderDrawParameters = true,
            },
            {
                .descriptorIndexing = true,
                .descriptorBindingUniformBufferUpdateAfterBind = true,
                .descriptorBindingSampledImageUpdateAfterBind = true,
                .descriptorBindingPartiallyBound = true,
                .bufferDeviceAddress = true,
            },
            {
                .synchronization2 = true,
                .dynamicRendering = true,
            },
            {.extendedDynamicState = true},
        };

    vk::DeviceCreateInfo deviceCreateInfo{
        .pNext =
            ENGINE_DEVICE_EXTENSIONS.get<vk::PhysicalDeviceVulkan11Features>(),
        .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfo.size()),
        .pQueueCreateInfos = queueCreateInfo.data(),
        .enabledExtensionCount =
            static_cast<uint32_t>(REQUIRED_DEVICE_EXTENSIONS.size()),
        .ppEnabledExtensionNames = REQUIRED_DEVICE_EXTENSIONS.data(),
    };

    VK_MAKE(device, physDevice.createDevice(deviceCreateInfo),
            "Failed to create logical device.");

    return std::move(device);
  }

  std::expected<Renderer::Queues, std::string>
  getQueues(vk::raii::Device& device, QueueIndices& queueIndices,
            const std::set<uint32_t>& uniqueQueueFamilies) {
    std::vector<Queue> queues{};
    for (uint32_t familyIndex : uniqueQueueFamilies) {
      vk::raii::Queue vkQueue = device.getQueue(familyIndex, 0);
      std::shared_ptr<vk::raii::Queue> queuePtr =
          std::make_shared<vk::raii::Queue>(std::move(vkQueue));
      Queue queue{.index = familyIndex, .queue = std::move(queuePtr)};
      queues.push_back(std::move(queue));
    }

    Queue& graphicsQueue = *std::ranges::find_if(queues, [&](const Queue& q) {
      return q.index == queueIndices.graphics;
    });
    Queue& presentQueue = *std::ranges::find_if(queues, [&](const Queue& q) {
      return q.index == queueIndices.present;
    });
    Queue& computeQueue = *std::ranges::find_if(queues, [&](const Queue& q) {
      return q.index == queueIndices.compute;
    });
    Queue& transferQueue = *std::ranges::find_if(queues, [&](const Queue& q) {
      return q.index == queueIndices.transfer;
    });

    return std::move(Renderer::Queues{
        .graphics = graphicsQueue,
        .present = presentQueue,
        .compute = computeQueue,
        .transfer = transferQueue,
    });
  }
} // namespace keptech::vkh::setup
