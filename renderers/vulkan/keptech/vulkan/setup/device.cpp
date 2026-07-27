#include "keptech/vulkan/helpers/physicalDevice.hpp"
#include "macros.hpp"
#include "renderer.hpp"
#include "setup.hpp"
#include <SDL3/SDL_vulkan.h>
#include <algorithm>
#include <expected>
#include <keptech/components/camera.hpp>
#include <ranges>
#include <set>
#include <utility>

namespace kt::vkh {
  using namespace kt::vkh;

  constexpr std::array REQUIRED_DEVICE_EXTENSIONS = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
      VK_EXT_MESH_SHADER_EXTENSION_NAME,
      VK_KHR_COMPUTE_SHADER_DERIVATIVES_EXTENSION_NAME,
#ifdef KT_USE_DESCRIPTOR_HEAP
      // Renderdoc does not support this extension!
      VK_EXT_DESCRIPTOR_HEAP_EXTENSION_NAME,
#endif
#ifdef KT_PROFILE
      VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME,
#endif
  };

  // TODO: Actually respect the RendererCreateInfo::capabilities.

  std::expected<void, std::string> Renderer::initPhysicalDevice(const RendererCreateInfo& createInfo) {
    VKH_MAKE(selector, kt::vkh::PhysicalDeviceSelector::create(m.vkcore.instance), "Failed to create physical device selector.");

    VK_DEBUG("Physical Devices in System:");
    for (const auto& device : selector.getDevices()) {
      VK_DEBUG("  - {}", device.properties.deviceName);
    }

    selector.requireVersion(1, 4, 0);
    if (selector.getDevices().empty()) {
      return std::unexpected("No physical devices found that support Vulkan 1.4.");
    }
    selector.requireExtensions(REQUIRED_DEVICE_EXTENSIONS);
    if (selector.getDevices().empty()) {
      return std::unexpected("No physical devices found that support the required engine extensions.");
    }
    selector.requireQueueFamily(VkQueueFlagBits::VK_QUEUE_GRAPHICS_BIT | VkQueueFlagBits::VK_QUEUE_COMPUTE_BIT);

    selector.scoreDevices([](auto& specs) {
      constexpr uint32_t DEDICATED_GPU_BONUS = 1000;

      uint32_t score = 1;

      if (specs.properties.deviceType == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        score += DEDICATED_GPU_BONUS;
      }

      return score;
    });

    m.vkcore.device.physical = selector.select();
    if (m.vkcore.device.physical == VK_NULL_HANDLE) {
      return std::unexpected("No suitable physical devices found.");
    }

    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(m.vkcore.device.physical, &properties);
    VK_INFO("Selected physical device: {}", properties.deviceName);
    return {};
  }

  namespace {
    std::expected<setup::QueueIndices, std::string> findQueues(VkPhysicalDevice physDevice, VkSurfaceKHR surface) {
      uint32_t queueFamilyCount = 0;
      vkGetPhysicalDeviceQueueFamilyProperties(physDevice, &queueFamilyCount, nullptr);
      std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
      vkGetPhysicalDeviceQueueFamilyProperties(physDevice, &queueFamilyCount, queueFamilies.data());

      setup::QueueIndices queues{
          .graphics = UINT32_MAX,
          .present = UINT32_MAX,
          .compute = UINT32_MAX,
          .transfer = UINT32_MAX,
      };

      bool foundCombinedGraphicsPresent = false;
      bool foundDedicatedTransfer = false;
      bool foundDedicatedCompute = false;

      for (const auto& [indexT, props] : std::views::enumerate(queueFamilies)) {
        uint32_t index = static_cast<uint32_t>(indexT);
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
  } // namespace

  std::expected<void, std::string> Renderer::initLogicalDevice(const RendererCreateInfo& createInfo,
                                                               const std::set<uint32_t>& uniqueQueueFamilies) {
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

    VK_MAKE(vkCreateDevice(m.vkcore.device, &deviceCreateInfo, nullptr, &m.vkcore.device.logical), "Failed to create logical device.");

    volkLoadDevice(m.vkcore.device.logical);

    return {};
  }

  namespace {
    void getQueues(VkDevice& device, setup::QueueIndices& queueIndices, const std::set<uint32_t>& uniqueQueueFamilies, Queues& out) {
      std::vector<Queue> queues{};
      for (uint32_t familyIndex : uniqueQueueFamilies) {
        VkQueue vkQueue{};
        vkGetDeviceQueue(device, familyIndex, 0, &vkQueue);
        Queue queue{.index = familyIndex, .queue = vkQueue};
        queues.push_back(queue);
      }

      out.graphics = *std::ranges::find_if(queues, [&](const Queue& q) { return q.index == queueIndices.graphics; });
      out.present = *std::ranges::find_if(queues, [&](const Queue& q) { return q.index == queueIndices.present; });
      out.compute = *std::ranges::find_if(queues, [&](const Queue& q) { return q.index == queueIndices.compute; });
      out.transfer = *std::ranges::find_if(queues, [&](const Queue& q) { return q.index == queueIndices.transfer; });
    }
  } // namespace

  std::expected<std::set<uint32_t>, std::string> Renderer::initDevice(const RendererCreateInfo& createInfo) {
    auto phys_res = initPhysicalDevice(createInfo);
    if (!phys_res) {
      return std::unexpected(phys_res.error());
    }

    auto queues_res = findQueues(m.vkcore.device.physical, m.vkcore.surface);
    if (!queues_res) {
      return std::unexpected(queues_res.error());
    }
    setup::QueueIndices& queueIndices = queues_res.value();
    std::set<uint32_t> uniqueQueueFamilies = {queueIndices.graphics, queueIndices.present, queueIndices.compute, queueIndices.transfer};

    auto logic_res = initLogicalDevice(createInfo, uniqueQueueFamilies);
    if (!logic_res) {
      return std::unexpected(logic_res.error());
    }
    getQueues(m.vkcore.device.logical, queueIndices, uniqueQueueFamilies, m.vkcore.queues);

    VmaVulkanFunctions vulkanFunctions{
        .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
        .vkGetDeviceProcAddr = vkGetDeviceProcAddr,
        .vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties,
        .vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties,
        .vkAllocateMemory = vkAllocateMemory,
        .vkFreeMemory = vkFreeMemory,
        .vkMapMemory = vkMapMemory,
        .vkUnmapMemory = vkUnmapMemory,
        .vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges,
        .vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges,
        .vkBindBufferMemory = vkBindBufferMemory,
        .vkBindImageMemory = vkBindImageMemory,
        .vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements,
        .vkGetImageMemoryRequirements = vkGetImageMemoryRequirements,
        .vkCreateBuffer = vkCreateBuffer,
        .vkDestroyBuffer = vkDestroyBuffer,
        .vkCreateImage = vkCreateImage,
        .vkDestroyImage = vkDestroyImage,
        .vkCmdCopyBuffer = vkCmdCopyBuffer,
    };

    VmaAllocatorCreateInfo allocInfo{
        .flags = VmaAllocatorCreateFlagBits::VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice = m.vkcore.device,
        .device = m.vkcore.device,
        .pVulkanFunctions = &vulkanFunctions,
        .instance = m.vkcore.instance,
    };
    VK_CHECK(vmaCreateAllocator(&allocInfo, &m.vkcore.device.allocator), "Failed to create VMA allocator.");

    return {std::move(uniqueQueueFamilies)};
  }
} // namespace kt::vkh
