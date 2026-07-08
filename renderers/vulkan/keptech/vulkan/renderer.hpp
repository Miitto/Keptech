#pragma once

#include "keptech/rendering/gltf/data.hpp"
#include "keptech/rendering/texture.hpp"
#include "keptech/vulkan/constants.hpp"
#include "keptech/vulkan/helpers/owned.hpp"
#include "keptech/vulkan/wrappers/buffer.hpp"
#include "keptech/vulkan/wrappers/device.hpp"
#include "keptech/vulkan/wrappers/image.hpp"
#include "keptech/vulkan/wrappers/instance.hpp"
#include "keptech/vulkan/wrappers/swapchain.hpp"
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

  struct GBuffer {
    using T = Owned<Image>;
    T albedo;
    T normal;
    T emissive;
    T metRough;
    T depth;
  };

  struct LightBuffer {
    using T = Owned<Image>;
    T diffuse;
    T specular;
    T ssaoResult;
    T ssaoNoise;
    T ssaoBlur;
    T combined;
  };

  class Renderer {
    // Structs
  public:
    struct TextureUpdateInfo {
      Image texture;
      size_t indexInDescriptorSet;
    };

    struct PerFrame {
      VkFence inFlightFence;
      VkSemaphore imageAvailableSemaphore;
      Pools pools;
      std::vector<TextureUpdateInfo> texToUpdate;
    };

    struct Frame {
      constexpr static uint8_t INVALID_INDEX = 255;

      uint8_t index = 0;
      uint8_t nextIndex = 1;
      uint8_t imageIndex = INVALID_INDEX;
      uint64_t deferredTimelineSubmit = 0;
      uint64_t ssaoTimelineSubmit = 0;
      size_t culledDraws = 0;
      size_t culledShadowDraws = 0;
      size_t culledLights = 0;
      PerFrame* perFrame = nullptr;
      bool suboptimalSwapchain = false;
    };

    struct GpuMaterial {
      uint32_t albedo;
      uint32_t bump;
      uint32_t emissive;
      uint32_t metRough;
      glm::vec4 albedoFactor;
      glm::vec3 emissiveFactor;
      uint32_t ao;
      float metFactor;
      float roughFactor;
      float specFactor = 1.f;
      float alphaCutoff = 0.f;
    };

    struct GpuObject {
      glm::mat4 model;
      uint32_t materialIndex;
      float pad1, pad2, pad3;
    };

    struct GpuPointLight {
      glm::vec3 position;
      float radius;
      glm::vec3 color;
      uint32_t shadowMapIndex;
    };
    struct PerFrameBuffers {
      template <typename T> using SB = Owned<SubdivBuffer<T>>;
      SB<GpuObject> objects;
      SB<GpuPointLight> pointLights;
      SB<glm::mat4> shadowMatrices;
    };
    struct Buffers {
      using B = Owned<Buffer>;
      template <typename T> using SB = Owned<SubdivBuffer<T>>;
      B camera;
      B ssaoKernel;
      SB<uint32_t> indices;
      SB<glm::vec3> vertexPositions;
      SB<VertexAttribs> vertexAttribs;
      SB<Meshlet> meshlets;
      SB<uint32_t> meshletVertices;
      SB<uint32_t> meshletTriangles;
      SB<GpuMaterial> materials;
      std::array<PerFrameBuffers, MAX_FRAMES_IN_FLIGHT> perFrame;
    };

    struct Pipelines {
      Pipeline basic;
      Pipeline deferred;
      Pipeline pointLightShadows;
      Pipeline deferredPointLight;
      Pipeline ssao;
      Pipeline ssaoBlur;
      Pipeline deferredCombine;
      Pipeline bloomDownsample;
      Pipeline bloomUpsample;
      Pipeline bloomCombine;

      void destroy(const VkDevice& device);
    };

    struct Samplers {
      VkSampler linearRepeat;
      VkSampler linearClamp;
      VkSampler nearestRepeat;
      VkSampler nearestClamp;

      void destroy(const VkDevice device);
    };

    struct VulkanCore {
      Instance instance;
      VkSurfaceKHR surface;
      Device device;
      VmaAllocator allocator;
      Queues queues;
      Swapchain swapchain;
      std::array<PerFrame, MAX_FRAMES_IN_FLIGHT> perFrame;
      CommandPool transferPool;
      VkSemaphore timelineSemaphore;
      uint64_t timelineValue = 0;
    };

    struct RenderTargets {
      GBuffer gBuffer;
      LightBuffer lights;
      glm::ivec2 framebufferSize;

      struct BloomMip {
        Owned<Image> image;
        glm::ivec2 size;
      };
      std::array<BloomMip, constants::BLOOM_MIP_LEVELS> bloomMips;
    };

    struct StaticDescriptors {
      VkDescriptorPool pool;
      VkDescriptorSetLayout layout;
      VkDescriptorSet set;
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
      Pipelines pipelines;

      DescriptorPoolSet<MAX_FRAMES_IN_FLIGHT> globalDescriptorSets;
      StaticDescriptors staticDescriptors;

      Frame frameInfo{};

      size_t nextTextureIndex = 0;
      uint32_t nextMeshIndex = 0;

      std::vector<Image> loadedTextures{};
      std::vector<Buffer> loadedBuffers{};

#ifdef KT_PROFILE
      TracyVkCtx tracyGraphicsContext;
      TracyVkCtx tracyComputeContext;
#endif
    };

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
    std::expected<UploadResult<Texture>, std::string> createImages(const gltf::Data& gltfData, const VkCommandBuffer transferCmd);
    std::expected<UploadResult<rendering::Material>, std::string>
    createMaterials(const gltf::Data& data, const std::vector<Texture>& textures, const VkCommandBuffer transferCmd);

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

    kt::maths::Frustum updateCameraBuffer(VkCommandBuffer cmdBuf);

    std::vector<Submesh> updateObjectsBuffer(const kt::maths::Frustum& frustum);
    void drawDeferred(VkCommandBuffer cmdBuf, const std::vector<Submesh>& submeshes);
    void submitDeferred(VkCommandBuffer cmdBuf);
    void drawLights(VkCommandBuffer cmdBuf, VkCommandBuffer combineCmdBuf, const kt::maths::Frustum& frustum,
                    const std::vector<Submesh>& submeshes);
    struct LightRenderInfo {
      Texture shadowMap;
      uint32_t drawCount = 0;
    };
    std::vector<LightRenderInfo> updatePointLightsBuffer(const kt::maths::Frustum& frustum);
    void drawPointLightShadowMaps(VkCommandBuffer cmdBuf, const std::vector<LightRenderInfo>&, const std::vector<Submesh>& submeshes);
    void drawPointLights(VkCommandBuffer cmdBuf);
    void combineLights(VkCommandBuffer cmdBuf);
    void submitLights(VkCommandBuffer cmdBuf);
    void renderBloom(VkCommandBuffer cmdBuf);
    void renderSsao(VkCommandBuffer cmdBuf);

    void renderImGui(VkCommandBuffer graphicsCmd);
    void endFrame(VkCommandBuffer graphicsCmd);
    void present();

    void debugUi();

    // Util
    void updateTextureDescriptors();
    void setupViewportAndScissor(VkCommandBuffer cmdBuf);
    void setupCustomViewportAndScissor(VkCommandBuffer cmdBuf, const glm::ivec2& offset, const glm::ivec2& size);
    void deferredToRenderable(VkCommandBuffer cmdBuf);
    void deferredBeginRendering(VkCommandBuffer cmdBuf);
    void deferredToShaderRead(VkCommandBuffer cmdBuf);
    void shadowMapToRenderable(VkCommandBuffer cmdBuf, const Image& shadowMap, bool isCube = false);
    void shadowMapBeginRendering(VkCommandBuffer cmdBuf, const Image& shadowMap, bool isCube = false);
    void shadowMapToShaderRead(VkCommandBuffer cmdBuf, const Image& shadowMap, bool isCube = false);
    void lightsToRenderable(VkCommandBuffer cmdBuf);
    void seperatedLightsBeginRendering(VkCommandBuffer cmdBuf);
    void seperatedLightsToShaderRead(VkCommandBuffer cmdBuf);
    void combinedLightBeginRendering(VkCommandBuffer cmdBuf);
    void combinedLightToShaderRead(VkCommandBuffer cmdBuf);
    void colorImageToRenderable(VkCommandBuffer cmdBuf, const Image& image);
    void colorImageBeginRendering(VkCommandBuffer cmdBuf, const Image& image, bool clear = true);
    void colorImageToShaderRead(VkCommandBuffer cmdBuf, const Image& image);

    void imGuiNewFrame() const;
    void shutdownImGui();

    std::expected<void, std::string> recreateSwapchain();

  private:
    Members m;
    Scene* scene = nullptr;
  };

} // namespace kt::vkh
