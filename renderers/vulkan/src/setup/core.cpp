#include "keptech/core/cameras/camera.hpp"
#include "keptech/core/renderer.hpp"
#include "keptech/vulkan/helpers/descriptors.hpp"
#include "keptech/vulkan/renderer.hpp"

#include "imgui.hpp"
#include "keptech/core/window.hpp"
#include "keptech/vulkan/helpers/device.hpp"
#include "keptech/vulkan/helpers/instance.hpp"
#include "keptech/vulkan/helpers/physicalDeviceSelector.hpp"
#include "keptech/vulkan/helpers/queueFinder.hpp"
#include "keptech/vulkan/helpers/swapchain.hpp"
#include "keptech/vulkan/structs.hpp"
#include "macros.hpp"
#include "vulkan/vulkan.hpp"
#include <SDL3/SDL_vulkan.h>
#include <algorithm>
#include <expected>
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
        ENGINE_DEVICE_EXTENSIONS = {{},
                                    {.shaderDrawParameters = true},
                                    {
                                        .descriptorIndexing = true,
                                        .bufferDeviceAddress = true,
                                    },
                                    {
                                        .synchronization2 = true,
                                        .dynamicRendering = true,
                                    },
                                    {.extendedDynamicState = true}};

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

  std::expected<Swapchain, std::string>
  createSwapchain(const vk::raii::PhysicalDevice& physicalDevice,
                  glm::ivec2 framebufferSize, const vk::raii::Device& device,
                  const vk::raii::SurfaceKHR& surface,
                  const Renderer::Queues& queues,
                  std::optional<vk::raii::SwapchainKHR*> oldSwapchain) {
    VK_MAKE(surfaceCapabilities,
            physicalDevice.getSurfaceCapabilitiesKHR(surface),
            "Failed to get surfacce capabilities");

    VK_MAKE(surfaceFormats, physicalDevice.getSurfaceFormatsKHR(surface),
            "Failed to get surface formats");
    VK_MAKE(presentModes, physicalDevice.getSurfacePresentModesKHR(surface),
            "Failed to get surface present modes");

    auto format = chooseSwapSurfaceFormat(surfaceFormats);
    auto presentMode = chooseSwapPresentMode(presentModes);

    auto extent = chooseSwapExtent(framebufferSize.x, framebufferSize.y,
                                   surfaceCapabilities, true);
    auto minImageCount =
        keptech::vkh::minImageCount(surfaceCapabilities, MAX_FRAMES_IN_FLIGHT);
    auto desiredImageCount =
        keptech::vkh::desiredImageCount(surfaceCapabilities);

    SwapchainConfig swapchainConfig{.format = format,
                                    .presentMode = presentMode,
                                    .extent = extent,
                                    .minImageCount = minImageCount,
                                    .imageCount = desiredImageCount};

    VKH_MAKE(swapchain,
             Swapchain::create(device, swapchainConfig, physicalDevice, surface,
                               {.graphicsQueueIndex = queues.graphics.index,
                                .presentQueueIndex = queues.present.index},
                               oldSwapchain),
             "Failed to create swapchain");

    return std::move(swapchain);
  }

  auto createSyncObjects(const vk::raii::Device& device)
      -> std::expected<Renderer::SyncObjects, std::string> {
    VK_MAKE(presentCompleteSemaphore,
            device.createSemaphore(vk::SemaphoreCreateInfo{}),
            "Failed to create present complete semaphore");

    VK_MAKE(renderCompleteSemaphore,
            device.createSemaphore(vk::SemaphoreCreateInfo{}),
            "Failed to create render complete semaphore");

    VK_MAKE(drawingFence,
            device.createFence(vk::FenceCreateInfo{
                .flags = vk::FenceCreateFlagBits::eSignaled}),
            "Failed to create drawing fence");

    Renderer::SyncObjects syncObjects{
        .presentCompleteSemaphore = std::move(presentCompleteSemaphore),
        .renderCompleteSemaphore = std::move(renderCompleteSemaphore),
        .drawingFence = std::move(drawingFence)};

    return std::move(syncObjects);
  }

  std::expected<Renderer::CameraObjects, std::string>
  createCameraObjects(const vk::raii::Device& device,
                      vma::Allocator& allocator) {
    vk::DescriptorPoolSize poolSize{
        .type = vk::DescriptorType::eUniformBuffer,
        .descriptorCount = 1,
    };
    VK_MAKE(descPool,
            device.createDescriptorPool(
                {.maxSets = 1, .poolSizeCount = 1, .pPoolSizes = &poolSize}),
            "Faild to create camera "
            "descriptor pool.");

    DescriptorLayoutBuilder layoutBuilder;
    layoutBuilder.addBinding(0, vk::DescriptorType::eUniformBuffer,
                             vk::ShaderStageFlagBits::eAll);
    VKH_MAKE(descLayout, layoutBuilder.build(device, nullptr),
             "Failed to create camera descriptor layout.");

    VK_MAKE(descSet,
            device.allocateDescriptorSets({.descriptorPool = *descPool,
                                           .descriptorSetCount = 1,
                                           .pSetLayouts = &*descLayout}),
            "Failed to allocate camera descriptor set.");

    VKH_MAKE(uniformBuffer,
             AllocatedBuffer::create(
                 allocator,
                 {
                     .size = sizeof(core::cameras::Uniforms),
                     .usage = vk::BufferUsageFlagBits::eUniformBuffer,
                     .sharingMode = vk::SharingMode::eExclusive,
                 },
                 {
                     .flags = vma::AllocationCreateFlagBits::eMapped,
                     .usage = vma::MemoryUsage::eCpuToGpu,
                 }),
             "Failed to create camera uniform buffer.");

    DescriptorWriter descWriter{};

    descWriter.writeBuffer(0,
                           vk::DescriptorBufferInfo{
                               .buffer = uniformBuffer.buffer,
                               .offset = 0,
                               .range = sizeof(core::cameras::Uniforms),
                           },
                           DescriptorWriter::BufferType::Uniform);

    descWriter.update(device, *descSet.front());

    Renderer::CameraObjects cameraObjects{
        .layout = std::move(descLayout),
        .pool = std::move(descPool),
        .descriptorSet = std::move(descSet.front()),
        .uniformBuffer = uniformBuffer,
    };

    return std::move(cameraObjects);
  }
} // namespace keptech::vkh::setup

namespace keptech::vkh {
  using namespace keptech::vkh::setup;

  std::expected<Renderer*, std::string>
  Renderer::create(const core::renderer::CreateInfo& createInfo,
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

    VKH_MAKE(swapchain,
             createSwapchain(physDevice, window.getRenderSize(), device,
                             surface, queues, std::nullopt),
             "Failed to create swapchain.");

    Renderer::Pools pools1;
    Renderer::Pools pools2;

    std::array<Pools*, 2> poolsArray = {&pools1, &pools2};

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
        Pools& pools = *poolsArray[i];

        VK_MAKE(poolRaii, device.createCommandPool(poolCreateInfo),
                "Failed to create command pool.");

        std::shared_ptr pool = std::make_shared<CommandPool>(std::move(
            CommandPool{.pool = std::move(poolRaii), .queue = queue}));

        if (familyIndex == queueIndices.graphics) {
          pools.graphics = pool;
        }
        if (familyIndex == queueIndices.present) {
          pools.present = pool;
        }
        if (familyIndex == queueIndices.compute) {
          pools.compute = pool;
        }
      }
    }

    VK_MAKE(transferPool,
            device.createCommandPool(vk::CommandPoolCreateInfo{
                .flags = vk::CommandPoolCreateFlagBits::eTransient,
                .queueFamilyIndex = queueIndices.transfer,
            }),
            "Failed to create transfer command pool.");

    VMA_MAKE(allocator,
             vma::createAllocator(vma::AllocatorCreateInfo{
                 .flags = vma::AllocatorCreateFlagBits::eBufferDeviceAddress,
                 .physicalDevice = *physDevice,
                 .device = *device,
                 .instance = *instance,
             }),
             "Failed to create VMA allocator.");

    VKH_MAKE(sync1, createSyncObjects(device),
             "Failed to create synchronization objects.");
    VKH_MAKE(sync2, createSyncObjects(device),
             "Failed to create synchronization objects.");

    FrameResources frameResource1{.syncObjects = std::move(sync1),
                                  .pools = std::move(pools1)};
    FrameResources frameResource2{.syncObjects = std::move(sync2),
                                  .pools = std::move(pools2)};

    std::array<FrameResources, MAX_FRAMES_IN_FLIGHT> frameResources = {
        std::move(frameResource1), std::move(frameResource2)};

    CommandPool transferPoolStruct{
        .pool = std::move(transferPool),
        .queue = queues.transfer,
    };

    Renderer::VulkanCore vkcore{
        .context = std::move(context),
        .instance = std::move(instance),
        .surface = std::move(surface),
        .device = Device{.physical = std::move(physDevice),
                         .logical = std::move(device)},
        .queues = std::move(queues),
        .swapchain = std::move(swapchain),
        .frameResources = std::move(frameResources),
        .transferPool = std::move(transferPoolStruct),
    };

    VKH_MAKE(cameraObjects,
             createCameraObjects(vkcore.device.logical, allocator),
             "Failed to create camera objects.");

    VKH_MAKE(imguiObjects,
             keptech::vkh::setup::setupImGui(
                 window, vkcore.instance, vkcore.device.logical,
                 vkcore.device.physical, vkcore.queues.graphics,
                 vkcore.swapchain.config().format.format),
             "Failed to create ImGui Vulkan objects.");

    VK_DEBUG("Vulkan renderer created successfully.");

    Renderer r{window, std::move(vkcore), allocator, std::move(imguiObjects),
               std::move(cameraObjects)};

    auto& renderer = addToEcs(std::move(r));
    return &renderer;
  }
} // namespace keptech::vkh
