#include "setup.hpp"

#include "helpers/conversions.hpp"
#include "keptech/vulkan/helpers/formatting.hpp"
#include "keptech/vulkan/helpers/pipeline.hpp"
#include "keptech/vulkan/structs.hpp"
#include "macros.hpp"
#include <keptech/shaders/shader.h>
#include <vulkan/vulkan.h>

namespace shaders {
  namespace {
#include "shaders/keptech/basic.h"
#include "shaders/keptech/deferred.h"
  } // namespace
} // namespace shaders

namespace kt::vkh::setup {

  // Formats
  namespace {
    constexpr std::array GBUFFER_ALBEDO_FORMATS = {VK_FORMAT_B8G8R8A8_SRGB};
    constexpr std::array GBUFFER_NORMAL_FORMATS = {VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_FORMAT_R16G16B16_SFLOAT,
                                                   VK_FORMAT_R16G16B16A16_SFLOAT};
    constexpr std::array GBUFFER_EMISSIVE_FORMATS = {VK_FORMAT_B10G11R11_UFLOAT_PACK32};
    constexpr std::array GBUFFER_METROUGH_FORMATS = {VK_FORMAT_R8G8_UNORM, VK_FORMAT_R8G8B8_UNORM, VK_FORMAT_R8G8B8A8_UNORM};
    constexpr std::array GBUFFER_DEPTH_FORMATS = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM};
    constexpr std::array HDR_FORMATS = {VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT};

    constexpr std::array TEXTYRE_ALBEDO_FORMATS = {VK_FORMAT_BC7_SRGB_BLOCK, VK_FORMAT_BC1_RGBA_SRGB_BLOCK, VK_FORMAT_B8G8R8A8_SRGB};
    constexpr std::array TEXTURE_NORMAL_FORMATS = {VK_FORMAT_BC5_UNORM_BLOCK, VK_FORMAT_A2B10G10R10_UNORM_PACK32,
                                                   VK_FORMAT_R16G16B16_SFLOAT, VK_FORMAT_R16G16B16A16_SFLOAT};
    constexpr std::array TEXTURE_METROUGH_FORMATS = {VK_FORMAT_BC5_UNORM_BLOCK, VK_FORMAT_R8G8_UNORM, VK_FORMAT_R8G8B8_UNORM,
                                                     VK_FORMAT_R8G8B8A8_UNORM};
    constexpr std::array TEXTURE_EMISSIVE_FORMATS = {VK_FORMAT_B10G11R11_UFLOAT_PACK32};
  } // namespace

  // Util
  namespace {
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

    struct Shader {
      VkShaderModule module;
      std::vector<VkPipelineShaderStageCreateInfo> stages;
    };

    std::expected<Shader, std::string> getShader(const shaders::Shader& shader, const VkDevice device) {
      VkShaderModule shaderModule = nullptr;
      VkShaderModuleCreateInfo shaderModuleCreateInfo{
          .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
          .codeSize = shader.code.size() * sizeof(uint8_t),
          .pCode = reinterpret_cast<const uint32_t*>(shader.code.data()),
      };
      VK_MAKE(vkCreateShaderModule(device, &shaderModuleCreateInfo, nullptr, &shaderModule),
              "Failed to create shader module for deferred pipeline.");

      std::vector<VkPipelineShaderStageCreateInfo> stages(shader.stages.size());
      for (size_t i = 0; i < shader.stages.size(); ++i) {
        stages[i] = VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = from(shader.stages[i].stage),
            .module = shaderModule,
            .pName = shader.stages[i].name,
        };
      }

      return std::move(Shader{.module = shaderModule, .stages = std::move(stages)});
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
  std::expected<Pipeline, std::string> createBasicPipeline(const Renderer::VulkanCore& vkcore, const Formats& formats,
                                                           const VkDescriptorSetLayout globalLayout) {
    VKH_MAKE(shader, getShader(::shaders::basic, vkcore.device.logical), "Failed to create shader for basic pipeline.");

    GraphicsPipelineConfig config{
        .rendering = {.colorAttachmentFormats = {formats.swapchain}},
        .shaders = shader.stages,
        .vertexInput = getVertexInputFromShader(::shaders::basic, {}),
    };

    PipelineLayoutConfig layoutConfig{
        .setLayouts = {},
        .pushConstantRanges = {{
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset = 0,
            .size = sizeof(glm::mat4) * 2,
        }},
    };

    auto vkLayoutInfo = layoutConfig.build();

    VkPipelineLayout vkLayout = nullptr;
    VK_MAKE(vkCreatePipelineLayout(vkcore.device.logical, &vkLayoutInfo, nullptr, &vkLayout),
            "Failed to create pipeline layout for basic pipeline.");

    auto vkConfig = config.build();
    vkConfig.layout = vkLayout;
    VkPipeline vkPipeline = nullptr;
    VK_MAKE(vkCreateGraphicsPipelines(vkcore.device.logical, nullptr, 1, &vkConfig, nullptr, &vkPipeline),
            "Failed to create graphics pipeline for basic pipeline.");

    vkDestroyShaderModule(vkcore.device.logical, shader.module, nullptr);

    return Pipeline{
        .layout = vkLayout,
        .pipeline = vkPipeline,
    };
  }

  std::expected<Pipeline, std::string> createDeferredPipeline(const Renderer::VulkanCore& vkcore, const Formats& formats,
                                                              const VkDescriptorSetLayout globalLayout) {
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

    PipelineLayoutConfig layoutConfig{
        .setLayouts = {globalLayout},
        .pushConstantRanges = {{
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset = 0,
            .size = sizeof(VkDeviceAddress) * 2 + sizeof(uint32_t),
        }},
    };

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

  std::expected<Renderer::Pipelines, std::string> createPipelines(const Renderer::VulkanCore& vkcore,
                                                                  const VkDescriptorSetLayout globalLayout) {
    VKH_MAKE(formats, findFormats(vkcore), "Failed to find suitable formats for renderer.");

    VKH_MAKE(basic, createBasicPipeline(vkcore, formats, globalLayout), "Failed to create basic pipeline.");
    VKH_MAKE(deferred, createDeferredPipeline(vkcore, formats, globalLayout), "Failed to create deferred pipeline.");

    return Renderer::Pipelines{
        .basic = basic,
        .deferred = deferred,
    };
  }
} // namespace kt::vkh::setup
