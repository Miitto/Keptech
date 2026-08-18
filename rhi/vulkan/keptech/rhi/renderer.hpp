#pragma once

#include "keptech/rhi/buffer.hpp"
#include "keptech/rhi/gltf/data.hpp"
#include "keptech/rhi/gltf/scene.hpp"
#include "keptech/rhi/image.hpp"
#include "keptech/rhi/material.hpp"
#include "keptech/rhi/mesh.hpp"
#include "keptech/rhi/passes/geometry.hpp"
#include "keptech/rhi/pipeline.hpp"
#include "keptech/rhi/renderer/buffers.hpp"
#include "keptech/rhi/renderer/core.hpp"
#include "keptech/rhi/renderer/structs.hpp"
#include <expected>

#ifdef KT_PROFILE
#include <tracy/TracyVulkan.hpp>
#endif

namespace kt {
  struct RendererCreateInfo;
  class Window;

  namespace shaders {
    struct Shader;
  }

  namespace maths {
    class Frustum;
  }
} // namespace kt

namespace kt::rhi {
  class BufferCreateInfo;
  class ImageCreateInfo;
  class RenderGraph;
  class RenderGraphBuilder;

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

  struct RendererStats {
#ifndef NDEBUG
    size_t drawCalls = 0;
    size_t vertFragDrawCalls = 0;
    size_t meshDrawCalls = 0;
    size_t computeDispatches = 0;
    size_t indexCount = 0;
    size_t triangleCount = 0;
    size_t meshletCount = 0;
    size_t pipelineSwitches = 0;
    size_t renderPasses = 0;

    void reset();
#endif
  };

  struct Passes {
    GeometryPass geometry{};
  };

  struct Members {
    const Window* window;
    VulkanCore vkcore;
    Samplers samplers;

    VkDescriptorPool imGuiDescriptorPool;

    Formats formats;

    Buffers buffers;

    DescriptorPoolSet<MAX_FRAMES_IN_FLIGHT> globalDescriptorSets;

    Frame frameInfo{};

    Indices indices{};

    std::vector<LoadedImage> loadedTextures{};
    std::vector<Buffer> loadedBuffers{};

    Passes passes{};

    RendererStats stats{};

#ifdef KT_PROFILE
    TracyVkCtx tracyGraphicsContext;
    TracyVkCtx tracyComputeContext;
#endif
  };

  class RHI {
  public:
    friend class RenderGraph;
    using Members = kt::rhi::Members;

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;
    ~Renderer();

#pragma region Getters
    static Renderer& get();

    const Members& getMembers() const;
    Members& getMembers();
    const Device& getDevice() const;
    const Device* operator->() const;

    uint8_t getFrameIndex() const;
    uint8_t getLastFrameIndex() const;

    [[nodiscard]] VkDescriptorSetLayout getGlobalDescriptorSetLayout() const;
    [[nodiscard]] VkDescriptorSet getGlobalDescriptorSet() const;

    [[nodiscard]] const Buffers& getBuffers() const;

    std::array<VkBuffer, 2> getVertexBuffers() const;

    [[nodiscard]] VkBuffer getIndexBuffer() const;
    [[nodiscard]] VkBuffer getMeshletBuffer() const;
    [[nodiscard]] VkBuffer getMeshletVertexBuffer() const;
    [[nodiscard]] VkBuffer getMeshletTriangleBuffer() const;
    [[nodiscard]] VkBuffer getMaterialBuffer() const;

    [[nodiscard]] VkSampler getLinearRepeatSampler() const;
    [[nodiscard]] VkSampler getLinearClampSampler() const;
    [[nodiscard]] VkSampler getNearestRepeatSampler() const;
    [[nodiscard]] VkSampler getNearestClampSampler() const;

    /// Returns the VkFormat of the swapchain.
    [[nodiscard]] VkFormat backbufferFormat() const;
    /// Returns the preferred formats for different types of images that can be rendered to. This is useful for creating images that will be
    /// used as render targets.
    [[nodiscard]] const Formats& getFormats() const { return m.formats; }
#pragma endregion

#pragma region Render Passes
    void addGeometryPass(RenderGraphBuilder& builder, bool clearColorBuffers = false);
#pragma endregion

    [[nodiscard]] Result<Buffer, VkResult, VK_SUCCESS> createBuffer(const BufferCreateInfo& info) const;
    [[nodiscard]] Result<Image, VkResult, VK_SUCCESS> createImage(const ImageCreateInfo& info) const;

    [[nodiscard]] Result<Shader, VkResult, VK_SUCCESS> createShader(const shaders::Shader& info) const;
    [[nodiscard]] Result<VkPipelineLayout, VkResult, VK_SUCCESS> createPipelineLayout(const VkPipelineLayoutCreateInfo& info) const;
    [[nodiscard]] Result<Pipeline, VkResult, VK_SUCCESS> createPipeline(const VkGraphicsPipelineCreateInfo& info) const;

    /// Loads a glTF mesh from the specified path. Returns a gltf::Scene on success, or an error message on failure.
    std::expected<gltf::Scene, std::string> loadMesh(std::string_view path);

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

    // Util
  public:
    /// Returns true if the renderer can render to the specified VkFormat. This is useful for determining if a particular format is
    /// supported for use as a render target.
    [[nodiscard]] bool canRenderToFormat(VkFormat format) const;

    /// Initializes the internal ImGui handle for the provided image.
    void loadImGuiImageHandle(Image& texture);

    /// Sets the resolution and formats properties for the provided RenderGraphBuilder. This is used to ensure that the render graph is
    /// aware of the swapchain format and size, as well as the active resolution.
    /// Is called internally during engine starup.
    void setRenderGraphProps(RenderGraphBuilder& builder) const;

    /// Called at the start of each frame. This function handles waiting for the previous frame with the same index to finish, acquiring the
    /// next swapchain image, and updating per frame resources (such as texture descriptors) with enqueued updates.
    /// Is called internally by the engine during the render loop.
    void newFrame();

    /// Called before rendering each frame. This function handles starting the ImGui frame, and any other per-frame setup that needs to be
    /// done before rendering. Is called internally by the engine during the render loop.
    maths::Frustum startFrame();

    /// @brief Registers a draw call for statistics tracking. Will be shown in the renderer debug UI.
    /// @note Is called by CommandBuffer::draw and CommandBuffer::drawIndexed internally.
    /// @param indexCount The number of indices in the draw call.
    /// @param triangleCount The number of triangles in the draw call.
    void registerDrawCall(size_t indexCount, size_t triangleCount);

    /// Registeres a mesh shader draw call for statistic tracking. Will be shown in the renderer debug UI.
    /// @param meshletCount The number of meshlets in the draw call.
    /// @param triangleCount The number of triangles in the draw call.
    /// @param indexCount The number of indices in the draw call.
    void registerMeshletDrawCall(size_t meshletCount, size_t triangleCount, size_t indexCount);

    /// Registers a compute dispatch for statistics tracking. Will be shown in the renderer debug UI.
    /// Is called by CommandBuffer::dispatch and CommandBuffer::dispatchIndirect internally, so you don't need to call this manually unless
    /// you are not using the CommandBuffer wrapper.
    void registerComputeDispatch();

    /// Registers a pipeline switch for statistics tracking. Will be shown in the renderer debug UI.
    /// Is called by CommandBuffer::bindPipeline and CommandBuffer::bindComputePipeline internally, so you don't need to call this manually
    /// unless you are not using the CommandBuffer wrapper.
    void registerPipelineSwitch();

    /// Registers a render pass for statistics tracking. Will be shown in the renderer debug UI.
    /// Is called by CommandBuffer::beginRendering internally, so you don't need to call this manually unless you are not using the
    /// CommandBuffer wrapper.
    void registerRenderPass();

    static bool isInit() { return isInitialized; }
    std::expected<void, std::string> static init(const RendererCreateInfo& createInfo, const Window& window);

  private:
    Renderer() = default;

#pragma region Initialization
    std::expected<void, std::string> initInternal(const RendererCreateInfo& createInfo, const Window& window);
    std::expected<void, std::string> initVulkanCore(const RendererCreateInfo& createInfo, const Window& window);
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
#pragma endregion

    PerFrameBuffers& fBufs() { return m.buffers.perFrame[m.frameInfo.index]; }

    void renderImGui(VkCommandBuffer graphicsCmd);
    void endFrame(CommandBuffer graphicsCmd);
    void present();

    /// @note Preprocessed out in release builds.
    void debugUi();

    // Util
    void updateTextureDescriptors();
    void updateBufferPointers() const;

    void imGuiNewFrame() const;
    void shutdownImGui();

    std::expected<void, std::string> recreateSwapchain();

  private:
    Members m;

    static Renderer singleton;
    static bool isInitialized;
  };

} // namespace kt::rhi
