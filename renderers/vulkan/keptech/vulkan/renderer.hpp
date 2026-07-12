#pragma once

#include "keptech/rendering/gltf/data.hpp"
#include "keptech/vulkan/buffers.hpp"
#include "keptech/vulkan/core.hpp"
#include "keptech/vulkan/helpers/owned.hpp"
#include "keptech/vulkan/passes/geometry.hpp"
#include "keptech/vulkan/pipelines.hpp"
#include "keptech/vulkan/wrappers/buffer.hpp"
#include "keptech/vulkan/wrappers/image.hpp"
#include "types.hpp"
#include <Volk/volk.h>
#include <expected>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <keptech/components/transform.hpp>
#include <keptech/core/base.hpp>
#include <keptech/core/moveGuard.hpp>
#include <keptech/core/scene.hpp>
#include <keptech/core/slotmap.hpp>
#include <keptech/maths/frustum.hpp>
#include <keptech/maths/transform.hpp>
#include <keptech/rendering/gltf/scene.hpp>
#include <keptech/rendering/mesh.hpp>
#include <keptech/rendering/renderer.hpp>
#include <keptech/shaders/shader.h>
#include <keptech/vulkan/structs.hpp>
#include <meshoptimizer.h>
#include <string>
#include <vk_mem_alloc.h>

#ifdef KT_PROFILE
#include <tracy/TracyVulkan.hpp>
#endif

namespace kt::vkh {

  struct LightBuffer {
    using T = Owned<Image>;
    T diffuse;
    T specular;
    T ssaoResult;
    T ssaoNoise;
    T ssaoBlur;
    T combined;
  };

  struct Samplers {
    VkSampler linearRepeat;
    VkSampler linearClamp;
    VkSampler nearestRepeat;
    VkSampler nearestClamp;

    void destroy(const VkDevice device);
  };

  struct RenderTargets {
    passes::geometry::Target gBuffer;
    LightBuffer lights;
    glm::ivec2 framebufferSize;
  };

  struct StaticDescriptors {
    VkDescriptorPool pool;
    VkDescriptorSetLayout layout;
    VkDescriptorSet set;
  };

  struct Frame {
    constexpr static uint8_t INVALID_INDEX = 255;

    uint8_t index = 0;
    uint8_t nextIndex = 1;
    uint8_t imageIndex = INVALID_INDEX;
    uint64_t deferredTimelineSubmit = 0;
    uint64_t ssaoTimelineSubmit = 0;
    size_t objectsRendered = 0;
    PerFrame* perFrame = nullptr;
    bool suboptimalSwapchain = false;
  };

  struct Indices {
    SamplerHandle nextSamplerIndex = 0;
    ImageHandle nextCombinedImageIndex = 0;
    uint32_t nextSampledImageIndex = 0;
    uint32_t nextStorageImageIndex = 0;
    uint32_t nextUniformTexelBufferIndex = 0;
    uint32_t nextStorageTexelBufferIndex = 0;
    uint32_t nextUniformBufferIndex = 0;
    uint32_t nextStorageBufferIndex = 0;
  };

  struct Members {
    MoveGuard moveGuard{};

    const core::window::Window* window;
    VulkanCore vkcore;
    Samplers samplers;

    VkDescriptorPool imGuiDescriptorPool;

    Formats formats;
    RenderTargets renderTargets;

    Buffers buffers;
    Layouts layouts;
    Pipelines pipelines;

    DescriptorPoolSet<MAX_FRAMES_IN_FLIGHT> globalDescriptorSets;
    StaticDescriptors staticDescriptors;

    Frame frameInfo{};

    Indices indices{};
    uint32_t nextMeshIndex = 0;

    std::vector<Image> loadedTextures{};
    std::vector<Buffer> loadedBuffers{};

#ifdef KT_PROFILE
    TracyVkCtx tracyGraphicsContext;
    TracyVkCtx tracyComputeContext;
#endif
  };

  class Renderer {
    // Structs
  public:
    using Members = kt::vkh::Members;
    // Creation and destruction
  public:
    std::expected<Renderer, std::string> static create(const RendererCreateInfo& createInfo, const core::window::Window& window);

    std::expected<gltf::Scene, std::string> loadMesh(std::string_view path);

    void loadImGuiImageHandle(Image& texture);

    template <typename T> struct UploadResult {
      std::vector<T> resources;
      std::vector<Buffer> stagingBuffers;
    };
    std::expected<UploadResult<Mesh>, std::string> uploadMeshes(const std::vector<gltf::MeshData>& meshes,
                                                                const std::vector<rendering::Material>& materials,
                                                                const VkCommandBuffer transferCmd);
    std::expected<UploadResult<Image>, std::string> createImages(const gltf::Data& gltfData, const VkCommandBuffer transferCmd);
    std::expected<UploadResult<rendering::Material>, std::string>
    createMaterials(const gltf::Data& data, const std::vector<Image>& textures, const VkCommandBuffer transferCmd);

    void loadImage(Image& image);

    void setScene(Scene& scene) { this->scene = &scene; }

    Renderer() = delete;
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) noexcept;
    Renderer& operator=(Renderer&&) noexcept;
    ~Renderer();

    // Util
  public:
    [[nodiscard]] bool canRenderToFormat(VkFormat format) const;
    [[nodiscard]] VkFormat backbufferFormat() const;
    [[nodiscard]] bool hasMoved() const noexcept { return m.moveGuard.moved(); }

    PerFrameBuffers& fBufs() { return m.buffers.perFrame[m.frameInfo.index]; }

    // Render
  public:
    void newFrame();
    void render();

  private:
    Renderer(Members&& m) : m(std::move(m)) { m.frameInfo.perFrame = &this->m.vkcore.perFrame[0]; }

    void startFrame();
    void renderImGui(VkCommandBuffer graphicsCmd);
    void endFrame(VkCommandBuffer graphicsCmd);
    void present();

    void debugUi();

    // Util
    void updateTextureDescriptors();
    void updateBufferPointers() const;

    void imGuiNewFrame() const;
    void shutdownImGui();

    std::expected<void, std::string> recreateSwapchain();

  private:
    Members m;
    Scene* scene = nullptr;
  };

} // namespace kt::vkh
