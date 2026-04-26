#pragma once

#include "keptech/rendering/gltf/data.hpp"
#include "keptech/rendering/texture.hpp"
#include "keptech/vulkan/constants.hpp"
#include "keptech/vulkan/helpers/device.hpp"
#include "keptech/vulkan/helpers/instance.hpp"
#include "keptech/vulkan/helpers/swapchain.hpp"
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
#include <string>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#ifdef KT_PROFILE
#include <tracy/TracyVulkan.hpp>
#endif

namespace kt::vkh {

  struct GBuffer {
    using T = AllocatedImage;
    T albedo;
    T normal;
    T emissive;
    T metRough;
    T depth;
    void destroy(const VmaAllocator& allocator, const VkDevice& device);
  };

  struct LightBuffer {
    using T = AllocatedImage;
    T diffuse;
    T specular;
    T ssaoResult;
    T ssaoNoise;
    T ssaoBlur;
    T combined;

    void destroy(const VmaAllocator& allocator, const VkDevice& device);
  };

  class Renderer {
    // Structs
  public:
    struct TextureUpdateInfo {
      AllocatedImage texture;
      size_t indexInDescriptorSet;
    };

    struct PerFrame {
      VkFence inFlightFence;
      VkSemaphore imageAvailableSemaphore;
      VkSemaphore deferredRenderFinishedSemaphore;
      VkSemaphore lightsFinished;
      Pools pools;
      std::vector<TextureUpdateInfo> texToUpdate;
    };

    struct Frame {
      constexpr static uint8_t INVALID_INDEX = 255;

      uint8_t index = 0;
      uint8_t nextIndex = 1;
      uint8_t imageIndex = INVALID_INDEX;
      PerFrame* perFrame = nullptr;
      bool suboptimalSwapchain = false;
    };

    template <typename T> struct SubdivBuffer {
      AddressedAllocatedBuffer buffer{};
      size_t count = 0;

      [[nodiscard]] T* end() const {
        constexpr size_t elementSize = sizeof(T);
        size_t offset = count * elementSize;

        return reinterpret_cast<T*>(buffer.mapping(offset));
      }

      void write(const std::span<const T> data) {
        memcpy(end(), data.data(), data.size() * sizeof(T));
        count += data.size();
      }

      void overwrite(const std::span<const T> data) {
        count = 0;
        write(data);
      }
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
      template <typename T> using SB = SubdivBuffer<T>;
      SB<GpuObject> objects;
      SB<GpuPointLight> pointLights;
      SB<glm::mat4> shadowMatrices;
      void destroy(VmaAllocator& allocator);
    };
    struct Buffers {
      using B = AddressedAllocatedBuffer;
      template <typename T> using SB = SubdivBuffer<T>;
      B camera;
      B ssaoKernel;
      SB<Vertex> vertices;
      SB<uint32_t> indices;
      SB<GpuMaterial> materials;
      std::array<PerFrameBuffers, MAX_FRAMES_IN_FLIGHT> perFrame;

      void destroy(VmaAllocator& allocator);
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
    };

    struct RenderTargets {
      GBuffer gBuffer;
      LightBuffer lights;
      glm::ivec2 framebufferSize;

      struct BloomMip {
        AllocatedImage image;
        glm::ivec2 size;
      };
      std::array<BloomMip, constants::BLOOM_MIP_LEVELS> bloomMips;

      void destroy(const VmaAllocator& allocator, const VkDevice& device);
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

      Frame frameInfo{};

      size_t nextTextureIndex = constants::FirstUserTexture;

      std::vector<AllocatedImage> loadedTextures{};
      std::vector<AllocatedBuffer> loadedBuffers{};

#ifdef KT_PROFILE
      TracyVkCtx tracyContext;
#endif
    };

    // Creation and destruction
  public:
    std::expected<Renderer, std::string> static create(const RendererCreateInfo& createInfo, const core::window::Window& window);

    std::expected<gltf::Scene, std::string> loadMesh(std::string_view path);

    void loadImGuiImageHandle(AllocatedImage& texture);

    template <typename T> struct UploadResult {
      std::vector<T> resources;
      std::vector<AllocatedBuffer> stagingBuffers;
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

    void updateCameraBuffer(VkCommandBuffer cmdBuf);

    struct RenderInfo {
      uint32_t indexCount;
      uint32_t firstIndex;
      int32_t vertexOffset;
    };

    std::vector<RenderInfo> updateObjectsBuffer();
    void drawDeferred(VkCommandBuffer cmdBuf, const std::vector<RenderInfo>& renderInfos);
    void submitDeferred(VkCommandBuffer cmdBuf);
    void drawLights(VkCommandBuffer cmdBuf, const std::vector<RenderInfo>& renderInfos);
    struct LightRenderInfo {
      Texture shadowMap;
    };
    std::vector<LightRenderInfo> updatePointLightsBuffer();
    void drawPointLightShadowMaps(VkCommandBuffer cmdBuf, const std::vector<LightRenderInfo>&, const std::vector<RenderInfo>& renderInfos);
    void drawPointLights(VkCommandBuffer cmdBuf, size_t lightCount);
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
    void shadowMapToRenderable(VkCommandBuffer cmdBuf, const AllocatedImage& shadowMap, bool isCube = false);
    void shadowMapBeginRendering(VkCommandBuffer cmdBuf, const AllocatedImage& shadowMap, bool isCube = false);
    void shadowMapToShaderRead(VkCommandBuffer cmdBuf, const AllocatedImage& shadowMap, bool isCube = false);
    void lightsToRenderable(VkCommandBuffer cmdBuf);
    void seperatedLightsBeginRendering(VkCommandBuffer cmdBuf);
    void seperatedLightsToShaderRead(VkCommandBuffer cmdBuf);
    void combinedLightBeginRendering(VkCommandBuffer cmdBuf);
    void combinedLightToShaderRead(VkCommandBuffer cmdBuf);
    void colorImageToRenderable(VkCommandBuffer cmdBuf, const AllocatedImage& image);
    void colorImageBeginRendering(VkCommandBuffer cmdBuf, const AllocatedImage& image, bool clear = true);
    void colorImageToShaderRead(VkCommandBuffer cmdBuf, const AllocatedImage& image);

    void imGuiNewFrame() const;
    void shutdownImGui();
    [[nodiscard]] const VkFormat& getSwapchainImageFormat() const {}

    std::expected<void, std::string> recreateSwapchain();

  private:
    Members m;
    Scene* scene = nullptr;
  };

} // namespace kt::vkh
