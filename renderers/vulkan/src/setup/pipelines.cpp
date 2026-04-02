#include "keptech/vulkan/helpers/formatting.hpp"
#include "keptech/vulkan/helpers/pipeline.hpp"
#include "keptech/vulkan/structs.hpp"
#include "macros.hpp"
#include "setup.hpp"
#include <keptech/shaders/shader.h>
#include <vulkan/vulkan.h>

namespace shaders {
  namespace {
#include "shaders/keptech/deferred.h"
#include "shaders/keptech/lightCombine.h"
#include "shaders/keptech/pointLight.h"
  } // namespace
} // namespace shaders

namespace kt::vkh::setup {

  namespace {
    constexpr std::array GBUFFER_ALBEDO_FORMATS = {VK_FORMAT_B8G8R8A8_SRGB};
    constexpr std::array GBUFFER_NORMAL_FORMATS = {VK_FORMAT_A2B10G10R10_SNORM_PACK32, VK_FORMAT_R16G16B16_SFLOAT,
                                                   VK_FORMAT_R16G16B16A16_SFLOAT};
    constexpr std::array GBUFFER_EMISSIVE_FORMATS = {VK_FORMAT_B10G11R11_UFLOAT_PACK32};
    constexpr std::array GBUFFER_METROUGH_FORMATS = {VK_FORMAT_R8G8_UNORM, VK_FORMAT_R8G8B8_UNORM, VK_FORMAT_R8G8B8A8_UNORM};
    constexpr std::array GBUFFER_DEPTH_FORMATS = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM};
    constexpr std::array HDR_FORMATS = {VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT};

    constexpr std::array TEXTYRE_ALBEDO_FORMATS = {VK_FORMAT_BC7_SRGB_BLOCK, VK_FORMAT_BC1_RGBA_SRGB_BLOCK, VK_FORMAT_B8G8R8A8_SRGB};
    constexpr std::array TEXTURE_NORMAL_FORMATS = {VK_FORMAT_BC5_SNORM_BLOCK, VK_FORMAT_A2B10G10R10_SNORM_PACK32,
                                                   VK_FORMAT_R16G16B16_SFLOAT, VK_FORMAT_R16G16B16A16_SFLOAT};
    constexpr std::array TEXTURE_METROUGH_FORMATS = {VK_FORMAT_BC5_UNORM_BLOCK, VK_FORMAT_R8G8_UNORM, VK_FORMAT_R8G8B8_UNORM,
                                                     VK_FORMAT_R8G8B8A8_UNORM};
    constexpr std::array TEXTURE_EMISSIVE_FORMATS = {VK_FORMAT_B10G11R11_UFLOAT_PACK32};

    std::expected<Formats, std::string> findFormats(const Renderer::VulkanCore& vkcore) {
      auto findFormat = [&](std::span<const VkFormat> candidates, VkFormatFeatureFlags features) -> VkFormat {
        for (auto& format : candidates) {
          VkFormatProperties props;
          vkGetPhysicalDeviceFormatProperties(vkcore.device.physical, format, &props);
          if ((props.optimalTilingFeatures & features) == features) {
            return format;
          }
        }
        return VK_FORMAT_UNDEFINED;
      };
      auto findColorAttachmentFormat = [&](std::span<const VkFormat> candidates) {
        return findFormat(candidates, VK_FORMAT_FEATURE_2_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_BIT);
      };
      auto findDepthAttachmentFormat = [&](std::span<const VkFormat> candidates) {
        return findFormat(candidates, VK_FORMAT_FEATURE_2_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_BIT);
      };
      auto findTextureFormat = [&](std::span<const VkFormat> candidates) {
        return findFormat(candidates, VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_BIT);
      };

      Formats f{
          .render =
              {
                  .albedo = findColorAttachmentFormat(GBUFFER_ALBEDO_FORMATS),
                  .normal = findColorAttachmentFormat(GBUFFER_NORMAL_FORMATS),
                  .emissive = findColorAttachmentFormat(GBUFFER_EMISSIVE_FORMATS),
                  .metRought = findColorAttachmentFormat(GBUFFER_METROUGH_FORMATS),
                  .depth = findDepthAttachmentFormat(GBUFFER_DEPTH_FORMATS),
                  .hdr = findColorAttachmentFormat(HDR_FORMATS),
              },
          .texture =
              {
                  .albedo = findTextureFormat(TEXTYRE_ALBEDO_FORMATS),
                  .normal = findTextureFormat(TEXTURE_NORMAL_FORMATS),
                  .metRough = findTextureFormat(TEXTURE_METROUGH_FORMATS),
                  .emissive = findTextureFormat(TEXTURE_EMISSIVE_FORMATS),
              },
          .swapchain = vkcore.swapchain.config().format.format,
      };

#define CHECK_FORMAT(format)                                                                                                               \
  if (!(format)) {                                                                                                                         \
    return std::unexpected("Failed to find suitable " #format " format.");                                                                 \
  }
      CHECK_FORMAT(f.render.albedo);
      CHECK_FORMAT(f.render.normal);
      CHECK_FORMAT(f.render.emissive);
      CHECK_FORMAT(f.render.metRought);
      CHECK_FORMAT(f.render.depth);
      CHECK_FORMAT(f.render.hdr);
      CHECK_FORMAT(f.texture.albedo);
      CHECK_FORMAT(f.texture.normal);
      CHECK_FORMAT(f.texture.metRough);
      CHECK_FORMAT(f.texture.emissive);
      CHECK_FORMAT(f.swapchain);
      CHECK_FORMAT(f.render.albedo);
      CHECK_FORMAT(f.render.normal);
      CHECK_FORMAT(f.render.emissive);
      CHECK_FORMAT(f.render.metRought);

      VK_DEBUG("Selected formats:");
      VK_DEBUG("  Albedo: {}", f.render.albedo);
      VK_DEBUG("  Normal: {}", f.render.normal);
      VK_DEBUG("  Emissive: {}", f.render.emissive);
      VK_DEBUG("  Metallic-Roughness: {}", f.render.metRought);
      VK_DEBUG("  Depth: {}", f.render.depth);
      VK_DEBUG("  HDR: {}", f.render.hdr);
      VK_DEBUG("  Texture Albedo: {}", f.texture.albedo);
      VK_DEBUG("  Texture Normal: {}", f.texture.normal);
      VK_DEBUG("  Texture Metallic-Roughness: {}", f.texture.metRough);
      VK_DEBUG("  Texture Emissive: {}", f.texture.emissive);
      VK_DEBUG("  Swapchain: {}", f.swapchain);

      return f;
    }

    VkShaderStageFlagBits from(shaders::ShaderStages stage) {
      switch (stage) {
      case shaders::ShaderStages::Vertex:
        return VK_SHADER_STAGE_VERTEX_BIT;
      case shaders::ShaderStages::Fragment:
        return VK_SHADER_STAGE_FRAGMENT_BIT;
      case shaders::ShaderStages::Compute:
        return VK_SHADER_STAGE_COMPUTE_BIT;
      default:
        VK_CRITICAL("Unsupported shader stage: {}", static_cast<int>(stage));
        std::abort();
      }
    }

    struct Shader {
      VkShaderModule module;
      std::vector<VkPipelineShaderStageCreateInfo> stages;
    };

    std::expected<Shader, std::string> getShader(const shaders::Shader& shader, const VkDevice device) {
      VkShaderModule shaderModule = nullptr;
      VkShaderModuleCreateInfo shaderModuleCreateInfo{
          .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
          .codeSize = ::shaders::deferred.code.size() * sizeof(uint8_t),
          .pCode = reinterpret_cast<uint32_t*>(::shaders::deferred.code.data()),
      };
      VK_MAKE(vkCreateShaderModule(device, &shaderModuleCreateInfo, nullptr, &shaderModule),
              "Failed to create shader module for deferred pipeline.");

      std::vector<VkPipelineShaderStageCreateInfo> stages(shader.stages.size());
      for (size_t i = 0; i < shader.stages.size(); ++i) {
        stages[i] = VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = from(shader.stages[i].stage),
            .module = VK_NULL_HANDLE, // Placeholder, should be set when creating the pipeline
            .pName = shader.stages[i].name,
        };
      }

      return std::move(Shader{.module = shaderModule, .stages = std::move(stages)});
    }

    VkFormat from(shaders::DataType type) {
      using T = shaders::DataType;
#define F(_F) return VkFormat::VK_FORMAT_##_F
      switch (type) {
      case T::F32:
        F(R32_SFLOAT);
      case T::F32_2:
        F(R32G32_SFLOAT);
      case T::F32_3:
        F(R32G32B32_SFLOAT);
      case T::F32_4:
        F(R32G32B32A32_SFLOAT);
      case T::U8:
        F(R8_UINT);
      case T::U8_2:
        F(R8G8_UINT);
      case T::U8_3:
        F(R8G8B8_UINT);
      case T::U8_4:
        F(R8G8B8A8_UINT);
      case T::U16:
        F(R16_UINT);
      case T::U16_2:
        F(R16G16_UINT);
      case T::U16_3:
        F(R16G16B16_UINT);
      case T::U16_4:
        F(R16G16B16A16_UINT);
      case T::U32:
        F(R32_UINT);
      case T::U32_2:
        F(R32G32_UINT);
      case T::U32_3:
        F(R32G32B32_UINT);
      case T::U32_4:
        F(R32G32B32A32_UINT);
      case shaders::DataType::None:
      case shaders::DataType::Void:
        F(UNDEFINED);
      case shaders::DataType::Bool:
        F(R8_UINT);
      case shaders::DataType::F16:
        F(R16_SFLOAT);
      case shaders::DataType::F64:
        F(R64_SFLOAT);
      case shaders::DataType::F16_2:
        F(R16G16_SFLOAT);
      case shaders::DataType::F64_2:
        F(R64G64_SFLOAT);
      case shaders::DataType::F16_3:
        F(R16G16B16_SFLOAT);
      case shaders::DataType::F64_3:
        F(R64G64B64_SFLOAT);
      case shaders::DataType::F16_4:
        F(R16G16B16A16_SFLOAT);
      case shaders::DataType::F64_4:
        F(R64G64B64A64_SFLOAT);
      case shaders::DataType::I8:
        F(R8_SINT);
      case shaders::DataType::I16:
        F(R16_SINT);
      case shaders::DataType::I32:
        F(R32_SINT);
      case shaders::DataType::I64:
        F(R64_SINT);
      case shaders::DataType::I8_2:
        F(R8G8_SINT);
      case shaders::DataType::I16_2:
        F(R16G16_SINT);
      case shaders::DataType::I32_2:
        F(R32G32_SINT);
      case shaders::DataType::I64_2:
        F(R64G64_SINT);
      case shaders::DataType::I8_3:
        F(R8G8B8_SINT);
      case shaders::DataType::I16_3:
        F(R16G16B16_SINT);
      case shaders::DataType::I32_3:
        F(R32G32B32_SINT);
      case shaders::DataType::I64_3:
        F(R64G64B64_SINT);
      case shaders::DataType::I8_4:
        F(R8G8B8A8_SINT);
      case shaders::DataType::I16_4:
        F(R16G16B16A16_SINT);
      case shaders::DataType::I32_4:
        F(R32G32B32A32_SINT);
      case shaders::DataType::I64_4:
        F(R64G64B64A64_SINT);
      case shaders::DataType::U64:
        F(R64_UINT);
      case shaders::DataType::U64_2:
        F(R64G64_UINT);
      case shaders::DataType::U64_3:
        F(R64G64B64_UINT);
      case shaders::DataType::U64_4:
        F(R64G64B64A64_UINT);
      case shaders::DataType::F32_4x4:
        F(R32G32B32A32_SFLOAT);
      case shaders::DataType::Sampler2D:
        F(UNDEFINED);
      }
    }

    uint32_t getSize(shaders::DataType type) {
      using T = shaders::DataType;
      switch (type) {
      case T::F32:
        return 4;
      case T::F32_2:
        return 8;
      case T::F32_3:
        return 12;
      case T::F32_4:
        return 16;
      case T::U8:
        return 1;
      case T::U8_2:
        return 2;
      case T::U8_3:
        return 3;
      case T::U8_4:
        return 4;
      case T::U16:
        return 2;
      case T::U16_2:
        return 4;
      case T::U16_3:
        return 6;
      case T::U16_4:
        return 8;
      case T::U32:
        return 4;
      case T::U32_2:
        return 8;
      case T::U32_3:
        return 12;
      case T::U32_4:
        return 16;
      case shaders::DataType::None:
      case shaders::DataType::Void:
        return 0;
      case shaders::DataType::Bool:
        return 1;
      case shaders::DataType::F16:
        return 2;
      case shaders::DataType::F64:
        return 8;
      case shaders::DataType::F16_2:
        return 4;
      case shaders::DataType::F64_2:
        return 16;
      case shaders::DataType::F16_3:
        return 6;
      case shaders::DataType::F64_3:
        return 24;
      case shaders::DataType::F16_4:
        return 8;
      case shaders::DataType::F64_4:
        return 32;
      case shaders::DataType::I8:
        return 1;
      case shaders::DataType::I16:
        return 2;
      case shaders::DataType::I32:
        return 4;
      case shaders::DataType::I64:
        return 8;
      case shaders::DataType::I8_2:
        return 2;
      case shaders::DataType::I16_2:
        return 4;
      case shaders::DataType::I32_2:
        return 8;
      case shaders::DataType::I64_2:
        return 16;
      case shaders::DataType::I8_3:
        return 3;
      case shaders::DataType::I16_3:
        return 6;
      case shaders::DataType::I32_3:
        return 12;
      case shaders::DataType::I64_3:
        return 24;
      case shaders::DataType::I8_4:
        return 4;
      case shaders::DataType::I16_4:
        return 8;
      case shaders::DataType::I32_4:
        return 16;
      case shaders::DataType::I64_4:
        return 32;
      case shaders::DataType::U64:
        return 8;
      case shaders::DataType::U64_2:
        return 16;
      case shaders::DataType::U64_3:
        return 24;
      case shaders::DataType::U64_4:
        return 32;
      case shaders::DataType::F32_4x4:
        return 64;
      case shaders::DataType::Sampler2D:
        return 0;
      }
    }

    VertexInput getVertexInputFromShader(const shaders::Shader& shader, std::vector<uint32_t> instanceBindings) {
      std::vector<VkVertexInputAttributeDescription> vertexAttributes;
      uint32_t binding = 0;
      for (auto& param : shader.vertexLayout) {
        uint32_t voffset = 0;
        uint32_t location = 0;
        for (auto& type : param) {
          VkVertexInputAttributeDescription attrDesc{
              .location = location++,
              .binding = binding,
              .format = from(type),
              .offset = voffset,
          };
          vertexAttributes.push_back(attrDesc);
          voffset += getSize(type);
        }
        ++binding;
      }
      std::vector<VkVertexInputBindingDescription> vertexBindings;
      std::ranges::sort(instanceBindings);
      uint32_t currentBinding = 0;
      for (auto& param : shader.vertexLayout) {
        size_t bindingStride = 0;
        for (auto& type : param) {
          bindingStride += getSize(type);
        }

        auto isInstance = instanceBindings.end() != std::ranges::find(instanceBindings, static_cast<uint32_t>(currentBinding));

        VkVertexInputBindingDescription bindingDesc{
            .binding = static_cast<uint32_t>(currentBinding++),
            .stride = static_cast<uint32_t>(bindingStride),
            .inputRate = isInstance ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX,
        };
        vertexBindings.push_back(bindingDesc);
      }

      return {
          .bindings = vertexBindings,
          .attributes = vertexAttributes,
      };
    }

  } // namespace

  std::expected<Pipeline, std::string> createDeferredPipeline(const Renderer::VulkanCore& vkcore, const Formats& formats) {
    VKH_MAKE(shader, getShader(::shaders::deferred, vkcore.device.logical), "Failed to create shader for deferred pipeline.");

    GraphicsPipelineConfig config{
        .rendering =
            {
                .colorAttachmentFormats =
                    {
                        formats.render.albedo,
                        formats.render.normal,
                        formats.render.emissive,
                        formats.render.metRought,
                    },
                .depthAttachmentFormat = formats.render.depth,
            },
        .shaders = shader.stages,
        .vertexInput = getVertexInputFromShader(::shaders::deferred, {}),
        .inputAssembly = {.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST},
        .depthStencilState =
            {
                .depthBoundsTestEnable = VK_TRUE,
                .depthTestEnable = VK_TRUE,
                .depthCompareOp = VK_COMPARE_OP_GREATER,
            },
        .blendAttachments =
            {
                VkPipelineColorBlendAttachmentState{.blendEnable = VK_FALSE},
                VkPipelineColorBlendAttachmentState{.blendEnable = VK_FALSE},
                VkPipelineColorBlendAttachmentState{.blendEnable = VK_FALSE},
                VkPipelineColorBlendAttachmentState{.blendEnable = VK_FALSE},
            },
        .blending = {.logicOpEnable = VK_FALSE},
        .dynamicState = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR},

    };

    PipelineLayoutConfig layoutConfig{};

    auto vkLayoutInfo = layoutConfig.build();
    auto vkConfig = config.build();

    Pipeline pipeline{};
    VK_MAKE(vkCreatePipelineLayout(vkcore.device.logical, &vkLayoutInfo, nullptr, &pipeline.layout),
            "Failed to create pipeline layout for deferred pipeline.");
    vkConfig.layout = pipeline.layout;
    VK_MAKE(vkCreateGraphicsPipelines(vkcore.device.logical, nullptr, 1, &vkConfig, nullptr, &pipeline.pipeline),
            "Failed to create graphics pipeline for deferred pipeline.");

    vkDestroyShaderModule(vkcore.device.logical, shader.module, nullptr);

    return pipeline;
  } // namespace

  std::expected<Renderer::Pipelines, std::string> createPipelines(const Renderer::VulkanCore& vkcore) {
    VKH_MAKE(formats, findFormats(vkcore), "Failed to find suitable formats for renderer.");

    VKH_MAKE(deferred, createDeferredPipeline(vkcore, formats), "Failed to create deferred pipeline.");

    return Renderer::Pipelines{
        .deferred = deferred,
    };
  }
} // namespace kt::vkh::setup
