#pragma once

#include "keptech/rendering/gltf/data.hpp"
#include "keptech/vulkan/buffers.hpp"
#include "keptech/vulkan/constants.hpp"
#include "keptech/vulkan/core.hpp"
#include "keptech/vulkan/passes/geometry.hpp"
#include "keptech/vulkan/wrappers/buffer.hpp"
#include "keptech/vulkan/wrappers/image.hpp"
#include "keptech/vulkan/wrappers/pipeline.hpp"
#include "renderGraph/graph.hpp"
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

  class RenderGraph;

  struct LoadedImage {
    VkImage image;
    VkImageView view;
    VmaAllocation alloc;
  };

  struct Samplers {
    VkSampler linearRepeat;
    VkSampler linearClamp;
    VkSampler nearestRepeat;
    VkSampler nearestClamp;

    void destroy(const VkDevice device);
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
    const core::window::Window* window;
    VulkanCore vkcore;
    Samplers samplers;

    VkDescriptorPool imGuiDescriptorPool;

    Formats formats;

    Buffers buffers;

    DescriptorPoolSet<MAX_FRAMES_IN_FLIGHT> globalDescriptorSets;

    Frame frameInfo{};

    Indices indices{};
    uint32_t nextMeshIndex = 0;

    std::vector<LoadedImage> loadedTextures{};
    std::vector<Buffer> loadedBuffers{};

#ifdef KT_PROFILE
    TracyVkCtx tracyGraphicsContext;
    TracyVkCtx tracyComputeContext;
#endif
  };

  class Renderer {
    // Structs
  public:
    friend class RenderGraph;
    using Members = kt::vkh::Members;
    // Creation and destruction
  public:
    static Renderer& get() { return singleton; }

    const Members& getMembers() const { return m; }
    Members& getMembers() { return m; }

    [[nodiscard]] VkDescriptorSetLayout getGlobalDescriptorSetLayout() const { return m.globalDescriptorSets.layout; }
    [[nodiscard]] VkDescriptorSet getGlobalDescriptorSet() const { return m.globalDescriptorSets.sets[m.frameInfo.index]; }

    [[nodiscard]] const Buffers& getBuffers() const { return m.buffers; }

    std::array<VkBuffer, 2> getVertexBuffers() const { return {m.buffers.vertexPositions, m.buffers.vertexAttribs}; }

    [[nodiscard]] VkBuffer getIndexBuffer() const { return m.buffers.indices; }
    [[nodiscard]] VkBuffer getMeshletBuffer() const { return m.buffers.meshlets; }
    [[nodiscard]] VkBuffer getMeshletVertexBuffer() const { return m.buffers.meshletVertices; }
    [[nodiscard]] VkBuffer getMeshletTriangleBuffer() const { return m.buffers.meshletTriangles; }
    [[nodiscard]] VkBuffer getMaterialBuffer() const { return m.buffers.materials; }

    [[nodiscard]] VkSampler getLinearRepeatSampler() const { return m.samplers.linearRepeat; }
    [[nodiscard]] VkSampler getLinearClampSampler() const { return m.samplers.linearClamp; }
    [[nodiscard]] VkSampler getNearestRepeatSampler() const { return m.samplers.nearestRepeat; }
    [[nodiscard]] VkSampler getNearestClampSampler() const { return m.samplers.nearestClamp; }

    [[nodiscard]] Result<Buffer, VkResult, VK_SUCCESS> createBuffer(const BufferCreateInfo& info) const {
      return m.vkcore.device.createBuffer(info);
    }
    [[nodiscard]] Result<Image, VkResult, VK_SUCCESS> createImage(const ImageCreateInfo& info) const {
      return m.vkcore.device.createImage(info);
    }

    [[nodiscard]] Result<Shader, VkResult, VK_SUCCESS> createShader(const shaders::Shader& info) const {
      return m.vkcore.device.createShader(info);
    }
    [[nodiscard]] Result<VkPipelineLayout, VkResult, VK_SUCCESS> createPipelineLayout(const VkPipelineLayoutCreateInfo& info) const {
      return m.vkcore.device.createPipelineLayout(info);
    }
    [[nodiscard]] Result<Pipeline, VkResult, VK_SUCCESS> createPipeline(const VkGraphicsPipelineCreateInfo& info) const {
      return m.vkcore.device.createPipeline(info);
    }

    /// Loads a glTF mesh from the specified path. Returns a gltf::Scene on success, or an error message on failure.
    std::expected<gltf::Scene, std::string> loadMesh(std::string_view path);

    /// Initializes the internal ImGui handle for the provided image.
    void loadImGuiImageHandle(Image& texture);

    template <typename T> struct UploadResult {
      std::vector<T> resources;
      std::vector<Buffer> stagingBuffers;
    };
    std::expected<UploadResult<Mesh>, std::string> uploadMeshes(const std::vector<gltf::MeshData>& meshes,
                                                                const std::vector<Material>& materials, const VkCommandBuffer transferCmd);
    std::expected<UploadResult<Image>, std::string> createImages(const gltf::Data& gltfData, const VkCommandBuffer transferCmd);
    std::expected<UploadResult<Material>, std::string> createMaterials(const gltf::Data& data, const std::vector<Image>& textures,
                                                                       const VkCommandBuffer transferCmd);

    /// Assigns the image a slot in the global texture descriptor set and queues the descriptor update.
    void loadImage(Image& image);

    void setScene(Scene& s) { scene = &s; }

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;
    ~Renderer();

    // Util
  public:
    /// Returns true if the renderer can render to the specified VkFormat. This is useful for determining if a particular format is
    /// supported for use as a render target.
    [[nodiscard]] bool canRenderToFormat(VkFormat format) const;
    /// Returns the VkFormat of the swapchain.
    [[nodiscard]] VkFormat backbufferFormat() const;

    PerFrameBuffers& fBufs() { return m.buffers.perFrame[m.frameInfo.index]; }

    /// Sets the resolution and formats properties for the provided RenderGraphBuilder. This is used to ensure that the render graph is
    /// aware of the swapchain format and size, as well as the active resolution.
    /// Is called internally during engine starup.
    void setRenderGraphProps(RenderGraphBuilder& builder) const;

    /// Returns the preferred formats for different types of images that can be rendered to. This is useful for creating images that will be
    /// used as render targets.
    [[nodiscard]] const Formats& getFormats() const { return m.formats; }

    /// Called at the start of each frame. This function handles waiting for the previous frame with the same index to finish, acquiring the
    /// next swapchain image, and updating per frame resources (such as texture descriptors) with enqueued updates.
    /// Is called internally by the engine during the render loop.
    void newFrame();

    /// Called before rendering each frame. This function handles starting the ImGui frame, and any other per-frame setup that needs to be
    /// done before rendering. Is called internally by the engine during the render loop.
    void startFrame();

    std::expected<void, std::string> static init(const RendererCreateInfo& createInfo, const core::window::Window& window);

  private:
    Renderer() = default;

    std::expected<void, std::string> initInternal(const RendererCreateInfo& createInfo, const core::window::Window& window);
    std::expected<void, std::string> initVulkanCore(const RendererCreateInfo& createInfo, const core::window::Window& window);
    std::expected<std::set<uint32_t>, std::string> initDevice(const RendererCreateInfo& createInfo);
    std::expected<void, std::string> initPhysicalDevice(const RendererCreateInfo& createInfo);
    std::expected<void, std::string> initLogicalDevice(const RendererCreateInfo& createInfo, const std::set<uint32_t>& uniqueQueueFamilies);
    std::expected<void, std::string> initCommandPools(const std::set<uint32_t>& uniqueQueueFamilies);
    std::expected<void, std::string> initSync();
    std::expected<void, std::string> initSamplers();
    std::expected<void, std::string> initImGui();
    std::expected<void, std::string> initDescriptors();
    std::expected<void, std::string> initBuffers();
    std::expected<void, std::string> initFormats();
    void writeDescriptors();

    void renderImGui(VkCommandBuffer graphicsCmd);
    void endFrame(CommandBuffer graphicsCmd);
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

    static Renderer singleton;
    static bool isInitialized;
  };

} // namespace kt::vkh
