#pragma once

#include "keptech/core/components/camera.hpp"
#include "keptech/core/components/transform.hpp"
#include "keptech/core/rendering/mesh.hpp"
#include "keptech/core/rendering/renderer.hpp"
#include <expected>
#include <keptech/core/rendering/gltf/scene.hpp>
#include <memory>
#include <utility>

namespace keptech {
  class Scene;

  struct GBuffers {
    ImgPtr albedo;
    ImgPtr normal;
    ImgPtr emissiveAo;
    ImgPtr metallicRoughness;
    ImgPtr depth;
  };

  struct LightingBuffers {
    ImgPtr diffuse;
    ImgPtr specular;
  };

  class Renderer {
    // End user use
  public:
    void setScene(Scene* newScene) { m.scene = newScene; }

    std::expected<Mesh, std::string> loadMesh(const MeshData& data);
    std::expected<gltf::Scene, std::string> loadMesh(std::string_view path);

    inline std::expected<PipelinePtr, std::string>
    createPipeline(PipelineCreateInfo createInfo) {
      preprocessPipelineCreateInfo(createInfo, m.textureFormats, *m.backend);

      return m.backend->createPipeline(std::move(createInfo));
    }

    inline std::expected<ImgPtr, std::string>
    createImage(const IRendererBackend::ImageCreateInfo& info) {
      return m.backend->createImage(info);
    }

    inline void loadImGuiImageHandle(ImgPtr& texture) {
      m.backend->loadImGuiImageHandle(texture);
    }

    [[nodiscard]] const GBuffers& getGBuffers() const { return m.gBuffers; }
    [[nodiscard]] GBuffers& getGBuffers() { return m.gBuffers; }

    LightingBuffers& getLightingBuffers() { return m.lightingBuffers; }
    [[nodiscard]] const LightingBuffers& getLightingBuffers() const {
      return m.lightingBuffers;
    }

    ImgPtr& getCombinedLightBuffer() { return m.lightCombinedBuffer; }
    [[nodiscard]] const ImgPtr& getCombinedLightBuffer() const {
      return m.lightCombinedBuffer;
    }

#ifdef KT_ADD_RESOURCE_INFO
    [[nodiscard]] size_t getTriangleCount() const { return m.triCount; }
    [[nodiscard]] size_t getDrawCallCount() const { return m.drawCallCount; }
#endif

    struct Pipelines {
      PipelinePtr deferred;
      PipelinePtr pointLight;
      PipelinePtr combineDeferred;
    };

    Pipelines& getPipelines() { return m.pipelines; }
    [[nodiscard]] const Pipelines& getPipelines() const { return m.pipelines; }

    // In Engine use
  public:
    static std::expected<Renderer, std::string>
    create(const RendererCreateInfo& createInfo,
           const core::window::Window& window);

    void newFrame();
    void render();

    Renderer(const Renderer&) = delete;
    Renderer(Renderer&&) = default;
    Renderer& operator=(const Renderer&) = delete;
    Renderer& operator=(Renderer&&) = default;
    ~Renderer();

    struct TextureFormats {
      TextureFormat albedo = TextureFormat::RGBA8UNorm;
      TextureFormat normal = TextureFormat::RGBA16F;
      TextureFormat emissiveAo = TextureFormat::RGBA8UNorm;
      TextureFormat metallicRoughness = TextureFormat::RG8UNorm;
      TextureFormat depth = TextureFormat::Depth32F;
      TextureFormat diffuse = TextureFormat::RGBA16F;
      TextureFormat specular = TextureFormat::RGBA16F;
      TextureFormat combined = TextureFormat::RGBA16F;
    };

    [[nodiscard]] const TextureFormats& getTextureFormats() const {
      return m.textureFormats;
    }
    static inline void
    preprocessPipelineCreateInfo(PipelineCreateInfo& createInfo,
                                 const TextureFormats& formats,
                                 const IRendererBackend& backend) {
      switch (createInfo.shader.mode) {
      case shaders::RenderingMode::Deferred: {
        createInfo.attachments = AttachmentConfig{
            .colorFormats =
                {
                    formats.albedo,
                    formats.normal,
                    formats.emissiveAo,
                    formats.metallicRoughness,
                },
            .depthFormat = formats.depth,
        };
        break;
      }
      case shaders::RenderingMode::DeferredLighting: {
        createInfo.attachments = {
            .colorFormats = {formats.diffuse, formats.specular},
        };
      } break;
      case shaders::RenderingMode::Forward:
        if (createInfo.attachments.colorFormats.empty()) {
          createInfo.attachments.colorFormats.push_back(
              backend.backbufferFormat());
        }
        break;
      case shaders::RenderingMode::Custom:
        break;
      }
    }

  private:
    void initImGui();

    struct CameraData {
      components::Transform* transform = nullptr;
      components::Camera* camera = nullptr;
    };

    struct FrameData {
      CmdBufPtr graphicsCmdBuf;
      CameraData cameraData;
    };

    void drawDeferredPass(const FrameData&);
    void drawLightingPass(const FrameData&);
    void drawPointLights(const FrameData&);
    void combineDeferredPass(const FrameData&);
    void drawForwardPass(const FrameData&);
    void drawTransparentPass(const FrameData&);
    void drawUIPass(const FrameData&);

    struct TexData {
      glm::vec2 uvScale{1.f, 1.f};
      glm::vec2 uvOffset{};
      float rotation = 0.f;
      uint32_t texIndex = ~0u;
    };

    struct DeferredPushConstantData {
      uint64_t instanceAddress;
      uint32_t instanceOffset;
      uint64_t materialAddress;
    };

    struct InstanceData {
      glm::mat4 modelMatrix;
    };

    struct GBufferImageIndexData {
      uint32_t albedoIndex;
      uint32_t normalIndex;
      uint32_t emissiveAoIndex;
      uint32_t metallicRoughnessIndex;
      uint32_t depthIndex;
    };

    struct LightBufferImageIndexData {
      uint32_t diffuseIndex;
      uint32_t specularIndex;
    };

    struct PointLightPushConstantData {
      glm::vec4 positionAndRadius;
      glm::vec4 colorAndIntensity;
    };

    struct GPUTex {
      glm::vec2 uvScale{1.f, 1.f};
      glm::vec2 uvOffset{};
      float rotation = 0.f;
      uint32_t texIndex = ~0u;
    };

    struct DeferredMaterialData {
      glm::vec4 albedoColor;
      glm::vec3 emissiveColor;
      float metallic;
      GPUTex albedo;
      GPUTex bump;
      GPUTex emissive;
      GPUTex metallicRoughness;
      GPUTex ao;
      float roughness;
    };

    struct Buffers {
      BufPtr cameraStaging;
      BufPtr vertex;
      BufPtr index;
      BufPtr instance;
      BufPtr material;
      size_t vertexEnd = 0;
      size_t indexEnd = 0;
      size_t instanceEnd = 0;
      size_t materialEnd = sizeof(DeferredMaterialData);
    };

    struct Members {
      Scene* scene = nullptr;

      std::unique_ptr<IRendererBackend> backend;
      TextureFormats textureFormats;

      GBuffers gBuffers;
      LightingBuffers lightingBuffers;
      ImgPtr lightCombinedBuffer;

      Renderer::Pipelines pipelines;

      Buffers buffers;

      MaterialPtr defaultMaterial;

#ifdef KT_ADD_RESOURCE_INFO
      size_t triCount = 0;
      size_t drawCallCount = 0;
#endif
    };

    Members m;
    Renderer(Members&& m) : m(std::move(m)) {}
  };
} // namespace keptech
