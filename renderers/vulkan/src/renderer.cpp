#include "keptech/vulkan/renderer.hpp"
#include "keptech/core/rendering/pipeline.hpp"
#include "macros.hpp"
#include "vulkan/vulkan.hpp"

#include <imgui/backends/imgui_impl_sdl3.h>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <imgui/imgui.h>
#include <keptech/core/renderer.hpp>
#include <keptech/core/window.hpp>
#include <set>

namespace keptech::vkh {
  static_assert(core::renderer::CRenderer<Renderer>,
                "Renderer must satisfy CRenderer concept");

  namespace {
    vk::ShaderStageFlagBits from(core::rendering::ShaderStages stages) {
      switch (stages) {
      case core::rendering::ShaderStages::Vertex:
        return vk::ShaderStageFlagBits::eVertex;
      case core::rendering::ShaderStages::Fragment:
        return vk::ShaderStageFlagBits::eFragment;
      case core::rendering::ShaderStages::Compute:
        return vk::ShaderStageFlagBits::eCompute;
      }
    }

    vk::ShaderStageFlags
    from(core::Bitflag<core::rendering::ShaderStages> stages) {
      using S = core::rendering::ShaderStages;
      vk::ShaderStageFlags flags = {};
      if (stages.has(S::Vertex)) {
        flags = flags | vk::ShaderStageFlagBits::eVertex;
      }
      if (stages.has(S::Fragment)) {
        flags = flags | vk::ShaderStageFlagBits::eFragment;
      }
      if (stages.has(S::Compute)) {
        flags = flags | vk::ShaderStageFlagBits::eCompute;
      }

      return flags;
    }

    vk::Format from(core::rendering::Format format, vk::Format defaultFormat) {
      switch (format) {
      case core::rendering::Format::RGB8:
        return vk::Format::eR8G8B8Unorm;
      case core::rendering::Format::RGBA8:
        return vk::Format::eR8G8B8A8Unorm;
      case core::rendering::Format::Default:
        return defaultFormat;
      default:
        return vk::Format::eUndefined;
      }
    }

    vk::PrimitiveTopology from(core::rendering::Topology topology) {
      switch (topology) {
      case core::rendering::Topology::TriangleList:
        return vk::PrimitiveTopology::eTriangleList;
      case core::rendering::Topology::TriangleStrip:
        return vk::PrimitiveTopology::eTriangleStrip;
      case core::rendering::Topology::LineList:
        return vk::PrimitiveTopology::eLineList;
      case core::rendering::Topology::LineStrip:
        return vk::PrimitiveTopology::eLineStrip;
      case core::rendering::Topology::PointList:
        return vk::PrimitiveTopology::ePointList;
      default:
        return vk::PrimitiveTopology::eTriangleList;
      }
    }

    vk::PolygonMode from(core::rendering::PolygonMode mode) {
      switch (mode) {
      case core::rendering::PolygonMode::Fill:
        return vk::PolygonMode::eFill;
      case core::rendering::PolygonMode::Line:
        return vk::PolygonMode::eLine;
      case core::rendering::PolygonMode::Point:
        return vk::PolygonMode::ePoint;
      default:
        return vk::PolygonMode::eFill;
      }
    }

    vk::CullModeFlags from(core::rendering::CullMode mode) {
      switch (mode) {
      case core::rendering::CullMode::None:
        return vk::CullModeFlagBits::eNone;
      case core::rendering::CullMode::Front:
        return vk::CullModeFlagBits::eFront;
      case core::rendering::CullMode::Back:
        return vk::CullModeFlagBits::eBack;
      case core::rendering::CullMode::FrontAndBack:
        return vk::CullModeFlagBits::eFrontAndBack;
      default:
        return vk::CullModeFlagBits::eNone;
      }
    }

    vk::FrontFace from(core::rendering::FrontFace face) {
      switch (face) {
      case core::rendering::FrontFace::Clockwise:
        return vk::FrontFace::eClockwise;
      case core::rendering::FrontFace::CounterClockwise:
        return vk::FrontFace::eCounterClockwise;
      default:
        return vk::FrontFace::eClockwise;
      }
    }

    vk::BlendFactor from(core::rendering::BlendFactor factor) {
      switch (factor) {
      case core::rendering::BlendFactor::Zero:
        return vk::BlendFactor::eZero;
      case core::rendering::BlendFactor::One:
        return vk::BlendFactor::eOne;
      case core::rendering::BlendFactor::SrcAlpha:
        return vk::BlendFactor::eSrcAlpha;
      case core::rendering::BlendFactor::OneMinusSrcAlpha:
        return vk::BlendFactor::eOneMinusSrcAlpha;
      default:
        return vk::BlendFactor::eOne;
      }
    }
  } // namespace

  void Renderer::Pools::resetAll() {
    std::set<vk::raii::CommandPool*> unique{
        &graphics.get()->pool,
        &present.get()->pool,
        &compute.get()->pool,
    };
    for (auto& pool : unique) {
      pool->reset();
    }
  }

  Renderer::ObjectLists
  Renderer::buildRenderObjectLists(const maths::Frustum& frustum) {
    ObjectLists lists;

    auto& ecs = ecs::ECS::get();

    for (auto& entity : entities) {
      auto transform = ecs.getComponentRef<components::Transform>(entity);
      auto renderObj =
          ecs.getComponentRef<core::rendering::RenderObject>(entity);

      auto meshP = loadedMeshes.get(renderObj.mesh);
      if (!meshP) {
        VK_WARN("RenderObject has invalid mesh handle, skipping");
        continue;
      }

      auto materialP = loadedMaterials.get(renderObj.material);
      if (!materialP) {
        VK_WARN("RenderObject has invalid material handle, skipping");
        continue;
      }

      auto& mesh = *meshP;
      auto& material = *materialP;

      transform.recalculateGlobalTransform();

      // TODO: Frustum cull

      struct VkRenderObject ro{
          .transform = transform.global,
          .material = &material,
          .mesh = &mesh,
      };

      switch (material.stage) {
      case Material::Stage::Deferred:
        lists.deferred.push_back(ro);
        break;
      case Material::Stage::Forward:
        lists.forward.push_back(ro);
        break;
      case Material::Stage::Transparent:
        lists.transparent.push_back(ro);
        break;
      }
    }
    return lists;
  }

  void Renderer::newFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();

    ImGui::NewFrame();
  }

  Renderer::Frame Renderer::startFrame() {
    checkSwapchain();
    checkCompletedCommandBuffers();

    auto& sync = vkcore.frameResources[nextFrameIndex].syncObjects;
    auto nextImageRes = vkcore.swapchain.getNextImage(
        vkcore.device, sync.drawingFence, sync.presentCompleteSemaphore);

    if (!nextImageRes) {
      VK_CRITICAL("Failed to acquire next swapchain image: {}",
                  nextImageRes.error());
      abort();
    }
    auto [imageIndex, swapchainState] = nextImageRes.value();

    if (swapchainState == vkh::Swapchain::State::OutOfDate ||
        swapchainState == vkh::Swapchain::State::Suboptimal) {
      auto res = recreateSwapchain();
      if (!res) {
        VK_CRITICAL("Failed to recreate swapchain: {}", res.error());
        abort();
      }
      // Try again
      return startFrame();
    }

    Frame frameInfo{
        .index = nextFrameIndex,
        .imageIndex = static_cast<uint8_t>(imageIndex),
        .syncObjects =
            std::ref(vkcore.frameResources[nextFrameIndex].syncObjects),
        .pools = std::ref(vkcore.frameResources[nextFrameIndex].pools),
    };

    frameInfo.pools.get().resetAll();
    submittedCommandBuffers[nextFrameIndex].clear();

    this->nextFrameIndex = (this->nextFrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;

    return frameInfo;
  }

  void Renderer::setupGraphicsCommandBuffer(
      const Frame& info, const vk::raii::CommandBuffer& graphicsCmdBuffer) {
    graphicsCmdBuffer.setViewport(
        0,
        vk::Viewport{
            .x = 0.0f,
            .y = 0.0f,
            .width = static_cast<float>(vkcore.swapchain.config().extent.width),
            .height =
                static_cast<float>(vkcore.swapchain.config().extent.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        });

    graphicsCmdBuffer.setScissor(0,
                                 vk::Rect2D{
                                     .offset = {.x = 0, .y = 0},
                                     .extent = vkcore.swapchain.config().extent,
                                 });
  }

  void Renderer::presentFrame(const Frame& info) {
    auto& sync = info.syncObjects.get();

    uint32_t imageIndex = info.imageIndex;

    vk::PresentInfoKHR presentInfo{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &*sync.renderCompleteSemaphore,
        .swapchainCount = 1,
        .pSwapchains = &*vkcore.swapchain.getSwapchain(),
        .pImageIndices = &imageIndex,
    };

    auto result = vkcore.queues.present.queue->presentKHR(presentInfo);
    if (result == vk::Result::eErrorOutOfDateKHR ||
        result == vk::Result::eSuboptimalKHR) {
      auto res = recreateSwapchain();
      if (!res) {
        VK_CRITICAL("Failed to recreate swapchain: {}", res.error());
        abort();
      }
    }
  }

  void Renderer::endFrame() { ImGui::EndFrame(); }

  Renderer::~Renderer() {
    if (moveGuard.moved()) {
      return;
    }

    vkcore.device.logical.waitIdle();

    checkCompletedCommandBuffers();

    loadedMeshes.reset();
    loadedMaterials.reset();

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    allocator.destroy();
    vkcore.device.logical.waitIdle();

    VK_INFO("Vulkan renderer shut down cleanly");
  }

  std::expected<void, std::string> Renderer::recreateSwapchain() {

    VKH_MAKE(newSwapchain,
             setup::createSwapchain(vkcore.device.physical,
                                    window->getRenderSize(),
                                    vkcore.device.logical, vkcore.surface,
                                    vkcore.queues, &*vkcore.swapchain),
             "Failed to recreate swapchain");

    std::optional<OldSwapchain> oldSwapchain = OldSwapchain{
        .swapchain = std::move(vkcore.swapchain),
        .frameIndex = nextFrameIndex,
    };

    vkcore.swapchain = std::move(newSwapchain);

    return {};
  }

  void Renderer::checkSwapchain() {
    if (!vkcore.oldSwapchain.has_value()) {
      return;
    }

    auto& oldSwapchain = vkcore.oldSwapchain.value();

    if (vkcore.device.logical.waitForFences(
            {vkcore.frameResources[oldSwapchain.frameIndex]
                 .syncObjects.drawingFence},
            VK_TRUE, UINT64_MAX) == vk::Result::eSuccess) {
      vkcore.oldSwapchain.reset();
    }
  }

  std::expected<core::rendering::Mesh::Handle, std::string>
  Renderer::meshFromData(
      const std::string& name,
      std::span<const core::rendering::Mesh::Vertex> vertices,
      std::span<const uint32_t> indices,
      std::vector<vkh::Mesh::Submesh> submeshes, bool backgroundLoad) {
    Pools& pools =
        vkcore.frameResources[nextFrameIndex].pools; // Use current frame pools

    auto res = vkh::Mesh::fromData(vkcore.device.logical, allocator,
                                   backgroundLoad ? vkcore.transferPool
                                                  : *pools.graphics,
                                   vertices, indices, std::move(submeshes));

    if (!res) {
      return std::unexpected(res.error());
    }

    if (backgroundLoad)
      ongoingCommandBuffers.push_back(std::move(res.value().second));
    else {
      auto waitRes = vkcore.device.logical.waitForFences(
          *res.value().second.fence, VK_TRUE, UINT64_MAX);
      res.value().second.buffer.destroy(allocator);
      if (waitRes != vk::Result::eSuccess) {
        return std::unexpected("Failed to wait for mesh upload fence");
      }
    }

    auto handle = loadedMeshes.emplace(std::move(res.value().first));

    core::rendering::Mesh::Handle meshHandle(
        handle, [this, name]() { unloadMesh(name); });

    return meshHandle;
  }

  void Renderer::unloadMesh(const std::string& name) {
    auto found = meshNameMap.find(name);
    if (found != meshNameMap.end()) {
      loadedMeshes.erase(found->second.get());
      meshNameMap.erase(found);
    }
  }

  std::optional<core::rendering::Mesh::Handle>
  Renderer::getMesh(const std::string& name) {
    auto found = meshNameMap.find(name);
    if (found != meshNameMap.end()) {
      core::SlotMapWeakHandle weakHandle = found->second;
      if (!weakHandle.valid()) {
        meshNameMap.erase(found);
        return std::nullopt;
      }
      auto handle = core::rendering::Mesh::Handle(
          weakHandle, [this, name]() { unloadMesh(name); });
      return handle;
    }
    return std::nullopt;
  }

  std::expected<Renderer::MaterialHandle, std::string>
  Renderer::createMaterial(const Material::CreateInfo& createInfo) {
    GraphicsPipelineConfig config;

    std::vector<Shader> shaderModules;

    std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;

    for (auto& shaderInfo : createInfo.pipelineConfig.shaders) {
      VKH_MAKE(shaderModule,
               Shader::create(vkcore.device.logical, shaderInfo.code,
                              shaderInfo.size),
               "Failed to create shader module");

      shaderModules.push_back(std::move(shaderModule));

      auto& shder = shaderModules.back();

      for (auto& stage : shaderInfo.stages) {
        vk::PipelineShaderStageCreateInfo stageInfo{
            .stage = from(stage.stage),
            .module = shder.get(),
            .pName = stage.name.data(),
        };

        shaderStages.push_back(stageInfo);
      }
    }

    config.shaders = shaderStages;

    for (auto& colorFormat :
         createInfo.pipelineConfig.attachments.colorFormats) {
      config.rendering.colorAttachmentFormats.push_back(
          from(colorFormat, getSwapchainImageFormat()));
    }

    config.rendering.depthAttachmentFormat =
        from(createInfo.pipelineConfig.attachments.depthFormat,
             vk::Format::eD16Unorm);
    config.rendering.stencilAttachmentFormat =
        from(createInfo.pipelineConfig.attachments.stencilFormat,
             vk::Format::eUndefined);

    // Input Assembly
    config.inputAssembly.topology = from(createInfo.pipelineConfig.topology);

    // Rasterizer
    config.rasterizer.polygonMode =
        from(createInfo.pipelineConfig.rasterizer.polygonMode);
    config.rasterizer.cullMode =
        from(createInfo.pipelineConfig.rasterizer.cullMode);
    config.rasterizer.frontFace =
        from(createInfo.pipelineConfig.rasterizer.frontFace);

    // Blending
    for (auto& colorFormat :
         createInfo.pipelineConfig.attachments.colorFormats) {
      config.blendAttachments.push_back(vk::PipelineColorBlendAttachmentState{
          .blendEnable = createInfo.pipelineConfig.blend.enableBlending,
          .srcColorBlendFactor = from(createInfo.pipelineConfig.blend.src),
          .dstColorBlendFactor = from(createInfo.pipelineConfig.blend.dst),
          .colorWriteMask =
              vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
              vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
      });
    }

    // Layout
    for (auto& pushConstant :
         createInfo.pipelineConfig.layout.pushConstantRanges) {
      vk::PushConstantRange range{
          .stageFlags = from(pushConstant.stages),
          .offset = pushConstant.offset,
          .size = pushConstant.size,
      };
      config.layout.pushConstantRanges.push_back(range);
    }

    auto vkLayoutInfo = config.layout.build();

    VK_MAKE(pipelineLayout,
            vkcore.device.logical.createPipelineLayout(vkLayoutInfo),
            "Failed to create pipeline layout");

    auto vkConfig = config.build();
    vkConfig.layout = *pipelineLayout;

    VK_MAKE(pipeline,
            vkcore.device.logical.createGraphicsPipeline(nullptr, vkConfig),
            "Failed to create graphics pipeline");

    Material mat{
        .pipeline = std::move(pipeline),
        .pipelineLayout = std::move(pipelineLayout),
    };
    mat.stage = createInfo.stage;

    auto handle = loadedMaterials.emplace(std::move(mat));
    return MaterialHandle(handle, loadedMaterials);
  }

  std::expected<Shader, std::string>
  Renderer::createShader(const unsigned char* const code, size_t size) {
    return Shader::create(vkcore.device.logical, code, size);
  }
} // namespace keptech::vkh
