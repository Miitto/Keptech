#include "setup.hpp"

#include "buffers.hpp"
#include "gpuObjects.hpp"
#include "keptech/components/camera.hpp"
#include "keptech/maths/maths.hpp"
#include "keptech/render/constants.hpp"
#include "keptech/rendering/mesh.hpp"
#include "renderer.hpp"
#include "wrappers/buffer.hpp"
#include "wrappers/bufferCreateInfo.hpp"

#undef VKH_MAKE

#define VKH_MAKE(_NAME, _EXPR, _ERROR)                                                                                                     \
  auto _NAME##_res = _EXPR;                                                                                                                \
  if (!_NAME##_res.isOk()) {                                                                                                               \
    VK_ERROR(_ERROR ": {}", _NAME##_res.error());                                                                                          \
    return std::unexpected(_ERROR);                                                                                                        \
  }                                                                                                                                        \
  auto&(_NAME) = _NAME##_res.value();

namespace {
  constexpr VmaAllocationCreateFlags hostWriteFlags =
      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
  constexpr VmaAllocationCreateFlags hostWriteOrTransferFlags =
      hostWriteFlags | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT;

  constexpr size_t INITIAL_VERTEX_COUNT = 1000;
  constexpr size_t INITIAL_INDEX_COUNT = 1000;
  constexpr size_t INITIAL_MATERIAL_COUNT = 100;
  constexpr size_t INITIAL_OBJECT_COUNT = 100;
  constexpr size_t INITIAL_POINT_LIGHT_COUNT = 2;
  constexpr size_t INITIAL_SHADOW_MATRIX_COUNT = (INITIAL_POINT_LIGHT_COUNT * 6);
  constexpr size_t INITIAL_MESHLET_COUNT = 100;
  constexpr size_t INITIAL_MESHLET_VERTEX_COUNT = INITIAL_MESHLET_COUNT * kt::constants::VERTICES_PER_MESHLET;
  constexpr size_t INITIAL_MESHLET_PRIMITIVE_COUNT = INITIAL_MESHLET_COUNT * kt::constants::PRIMITIVES_PER_MESHLET;
} // namespace

namespace kt::rdr {
  namespace {
    template <typename T>
    kt::Result<Buffer, VkResult, VK_SUCCESS> createBufferT(const size_t elementCount, const std::string& name,
                                                           const VkBufferUsageFlags usage = 0, const bool allowTransfer = false,
                                                           const bool roundToAlignment = false) {
      size_t perElementSize = roundToAlignment ? maths::roundToAlignment(sizeof(T), limits::minUniformBufferOffsetAlignment) : sizeof(T);
      size_t size = perElementSize * elementCount;

      return Buffer::create(
          BufferCreateInfo(size,
                           usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                               (allowTransfer ? (VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT) : 0),
                           allowTransfer ? hostWriteOrTransferFlags : hostWriteFlags, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, name.c_str()));
    }
  } // namespace

  std::expected<void, std::string> Renderer::initBuffers() {
    VKH_MAKE(
        camera,
        createBufferT<components::Camera::Uniforms>(MAX_FRAMES_IN_FLIGHT, "Camera Buffer", VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, false, true),
        "Failed to create camera uniform buffer.");
    VKH_MAKE(addresses,
             createBufferT<BufferPointers>(MAX_FRAMES_IN_FLIGHT, "Buffer Addresses", VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, false, true),
             "Failed to create buffer addresses buffer.");
    VKH_MAKE(ssaoKernel,
             createBufferT<glm::vec4>(constants::SSAO_KERNEL_SIZE, "SSAO Kernel Buffer", VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, true),
             "Failed to create SSAO kernel buffer.");
    VKH_MAKE(vertexPosBuffer,
             createBufferT<glm::vec3>(INITIAL_VERTEX_COUNT, "Vertex Position Buffer", VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, true),
             "Failed to create vertex position buffer.");
    VKH_MAKE(vertexAttribBuffer,
             createBufferT<VertexAttribs>(INITIAL_VERTEX_COUNT, "Vertex Attrib Buffer", VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, true),
             "Failed to create vertex buffer.");
    VKH_MAKE(indexBuffer, createBufferT<uint32_t>(INITIAL_INDEX_COUNT, "Index Buffer", VK_BUFFER_USAGE_INDEX_BUFFER_BIT, true),
             "Failed to create index buffer.");
    VKH_MAKE(materialBuffer, createBufferT<GpuMaterial>(INITIAL_MATERIAL_COUNT, "Material Buffer", 0, true),
             "Failed to create material buffer.");
    VKH_MAKE(meshletBuffer, createBufferT<Meshlet>(INITIAL_MESHLET_COUNT, "Meshlet Buffer", 0, true), "Failed to create meshlet buffer.");
    VKH_MAKE(meshletVertexBuffer, createBufferT<uint32_t>(INITIAL_MESHLET_VERTEX_COUNT, "Meshlet Vertex Buffer", 0, true),
             "Failed to create meshlet vertex buffer.");
    VKH_MAKE(meshletPrimitiveBuffer, createBufferT<uint8_t>(INITIAL_MESHLET_PRIMITIVE_COUNT, "Meshlet Primitive Buffer", 0, true),
             "Failed to create meshlet primitive buffer.");

    std::array<PerFrameBuffers, MAX_FRAMES_IN_FLIGHT> perFrameBuffers;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
      VKH_MAKE(objectBuffer, createBufferT<GpuObject>(INITIAL_OBJECT_COUNT, "Object Buffer"), "Failed to create object buffer.");
      VKH_MAKE(pointLightBuffer, createBufferT<GpuPointLight>(INITIAL_POINT_LIGHT_COUNT, "Point Light Buffer"),
               "Failed to create point light buffer.");
      VKH_MAKE(shadowMatrixBuffer, createBufferT<glm::mat4>(INITIAL_SHADOW_MATRIX_COUNT, "Shadow Matrix Buffer"),
               "Failed to create shadow matrix buffer.");

      perFrameBuffers[i] = PerFrameBuffers{
          .objects = std::move(objectBuffer),
          .pointLights = std::move(pointLightBuffer),
          .shadowMatrices = std::move(shadowMatrixBuffer),
      };
    }

    GpuMaterial defaultMaterial{
        .albedo = ~0u,
        .bump = ~0u,
        .emissive = ~0u,
        .metRough = ~0u,
        .albedoFactor = glm::vec4(1.0f),
        .emissiveFactor = glm::vec3(0.0f),
        .ao = ~0u,
        .metFactor = 1.0f,
        .roughFactor = 1.0f,
        .specFactor = 1.0f,
        .alphaCutoff = 0.0f,
    };

    SubdivBuffer<GpuMaterial> gpuMaterials{std::move(materialBuffer)};
    gpuMaterials.write(defaultMaterial);

    m.buffers.camera = std::move(camera);
    m.buffers.addresses = std::move(addresses);
    m.buffers.ssaoKernel = std::move(ssaoKernel);
    m.buffers.indices = std::move(indexBuffer);
    m.buffers.vertexPositions = std::move(vertexPosBuffer);
    m.buffers.vertexAttribs = std::move(vertexAttribBuffer);
    m.buffers.meshlets = std::move(meshletBuffer);
    m.buffers.meshletVertices = std::move(meshletVertexBuffer);
    m.buffers.meshletTriangles = std::move(meshletPrimitiveBuffer);
    m.buffers.materials = std::move(gpuMaterials);
    m.buffers.perFrame = std::move(perFrameBuffers);

    return {};
  }
} // namespace kt::rdr
