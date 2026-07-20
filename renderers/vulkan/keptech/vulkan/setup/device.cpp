#include "keptech/vulkan/helpers/physicalDevice.hpp"
#include "macros.hpp"
#include "setup.hpp"
#include <SDL3/SDL_vulkan.h>
#include <algorithm>
#include <expected>
#include <keptech/components/camera.hpp>
#include <ranges>
#include <set>

namespace kt::vkh::setup {
  using namespace kt::vkh;

  constexpr std::array REQUIRED_DEVICE_EXTENSIONS = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
      VK_EXT_MESH_SHADER_EXTENSION_NAME,
      VK_KHR_COMPUTE_SHADER_DERIVATIVES_EXTENSION_NAME,
      VK_EXT_DESCRIPTOR_HEAP_EXTENSION_NAME,
#ifdef KT_PROFILE
      VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME,
#endif
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

    auto physDevice = selector.select();
    if (physDevice == VK_NULL_HANDLE) {
      return std::unexpected("No suitable physical devices found.");
    }

    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physDevice, &properties);
    VK_INFO("Selected physical device: {}", properties.deviceName);

    return physDevice;
  }

  std::expected<QueueIndices, std::string> findQueues(VkPhysicalDevice physDevice, VkSurfaceKHR surface) {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physDevice, &queueFamilyCount, queueFamilies.data());

    QueueIndices queues{
        .graphics = UINT32_MAX,
        .present = UINT32_MAX,
        .compute = UINT32_MAX,
        .transfer = UINT32_MAX,
    };

    bool foundCombinedGraphicsPresent = false;
    bool foundDedicatedTransfer = false;
    bool foundDedicatedCompute = false;

    for (const auto& [index, props] : std::views::enumerate(queueFamilies)) {
      bool isGraphics = props.queueFlags & VkQueueFlagBits::VK_QUEUE_GRAPHICS_BIT;
      bool isCompute = props.queueFlags & VkQueueFlagBits::VK_QUEUE_COMPUTE_BIT;
      bool isTransfer = props.queueFlags & VkQueueFlagBits::VK_QUEUE_TRANSFER_BIT;
      VkBool32 supportsPresent = false;
      vkGetPhysicalDeviceSurfaceSupportKHR(physDevice, index, surface, &supportsPresent);

      if (isGraphics && supportsPresent && !foundCombinedGraphicsPresent) {
        queues.graphics = index;
        queues.present = index;
        foundCombinedGraphicsPresent = true;
        VK_DEBUG("Found combined graphics/present queue at index {}", index);
      } else if (isCompute && !isGraphics && !foundDedicatedCompute) {
        queues.compute = index;
        foundDedicatedCompute = true;
        VK_DEBUG("Found dedicated compute queue at index {}", index);
      } else if (isTransfer && !isGraphics && !isCompute && !foundDedicatedTransfer) {
        queues.transfer = index;
        foundDedicatedTransfer = true;
        VK_DEBUG("Found dedicated transfer queue at index {}", index);
      }
      // A compute transfer queue is preferable if we can't have a truly dedicated one
      else if (isTransfer && isCompute && !isGraphics && !foundDedicatedTransfer) {
        queues.transfer = index;
        VK_DEBUG("Found compute/transfer queue at index {}, using it as transfer queue", index);
      } else {
        if (isGraphics && queues.graphics == UINT32_MAX) {
          queues.graphics = index;
        }
        if (supportsPresent && queues.present == UINT32_MAX) {
          queues.present = index;
        }
        if (isCompute && queues.compute == UINT32_MAX) {
          queues.compute = index;
        }
        if (isTransfer && queues.transfer == UINT32_MAX) {
          queues.transfer = index;
        }
      }
    }

    VK_ASSERT(queues.graphics != UINT32_MAX, "Failed to find graphics queue.");
    VK_ASSERT(queues.present != UINT32_MAX, "Failed to find present queue.");
    VK_ASSERT(queues.compute != UINT32_MAX, "Failed to find compute queue.");
    VK_ASSERT(queues.transfer != UINT32_MAX, "Failed to find transfer queue.");

    return queues;
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

    VkPhysicalDeviceMeshShaderFeaturesEXT meshShaderFeatures{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT,
        .taskShader = true,
        .meshShader = true,
    };

    VkPhysicalDeviceComputeShaderDerivativesFeaturesKHR computeDerivativesFeatures{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COMPUTE_SHADER_DERIVATIVES_FEATURES_KHR,
        .pNext = &meshShaderFeatures,
        .computeDerivativeGroupQuads = true,
        .computeDerivativeGroupLinear = true,
    };
    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT extendedDynamicStateFeatures{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT,
        .pNext = &computeDerivativesFeatures,
        .extendedDynamicState = true,
    };
    VkPhysicalDeviceVulkan13Features vulkan13Features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = &extendedDynamicStateFeatures,
        .shaderDemoteToHelperInvocation = true,
        .synchronization2 = true,
        .dynamicRendering = true,
    };
    VkPhysicalDeviceVulkan12Features vulkan12Features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = &vulkan13Features,
        .shaderFloat16 = true,
        .shaderInt8 = true,
        .descriptorIndexing = true,
        .descriptorBindingUniformBufferUpdateAfterBind = true,
        .descriptorBindingSampledImageUpdateAfterBind = true,
        .descriptorBindingStorageImageUpdateAfterBind = true,
        .descriptorBindingStorageBufferUpdateAfterBind = true,
        .descriptorBindingUniformTexelBufferUpdateAfterBind = true,
        .descriptorBindingStorageTexelBufferUpdateAfterBind = true,
        .descriptorBindingPartiallyBound = true,
        .runtimeDescriptorArray = true,
        .scalarBlockLayout = true,
        .hostQueryReset = true,
        .timelineSemaphore = true,
        .bufferDeviceAddress = true,
        .shaderOutputLayer = true,
    };
    VkPhysicalDeviceVulkan11Features vulkan11Features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
        .pNext = &vulkan12Features,
        .uniformAndStorageBuffer16BitAccess = true,
        .shaderDrawParameters = true,
    };
    VkPhysicalDeviceFeatures2 deviceFeatures{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &vulkan11Features,
        .features =
            {
                .geometryShader = true,
                .multiDrawIndirect = true,
                .samplerAnisotropy = true,
                .shaderInt64 = true,
            },
    };

    VkDeviceCreateInfo deviceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &deviceFeatures,
        .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfo.size()),
        .pQueueCreateInfos = queueCreateInfo.data(),
        .enabledExtensionCount = static_cast<uint32_t>(REQUIRED_DEVICE_EXTENSIONS.size()),
        .ppEnabledExtensionNames = REQUIRED_DEVICE_EXTENSIONS.data(),
    };

    VkDevice device{};
    VK_MAKE(vkCreateDevice(physDevice, &deviceCreateInfo, nullptr, &device), "Failed to create logical device.");

    volkLoadDevice(device);

    return device;
  }

  std::expected<Queues, std::string> getQueues(VkDevice& device, QueueIndices& queueIndices,
                                               const std::set<uint32_t>& uniqueQueueFamilies) {
    std::vector<Queue> queues{};
    for (uint32_t familyIndex : uniqueQueueFamilies) {
      VkQueue vkQueue{};
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
