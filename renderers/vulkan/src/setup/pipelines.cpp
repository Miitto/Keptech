#include "setup.hpp"

#include "helpers/conversions.hpp"
#include "keptech/vulkan/helpers/formatting.hpp"
#include "keptech/vulkan/helpers/pipeline.hpp"
#include "keptech/vulkan/structs.hpp"
#include "macros.hpp"
#include <Volk/volk.h>
#include <keptech/components/lights.hpp>
#include <keptech/rendering/structs.hpp>
#include <keptech/shaders/shader.h>

namespace shaders {
  namespace {
#include "shaders/keptech/basic.h"
#include "shaders/keptech/bloomCombine.h"
#include "shaders/keptech/deferred.h"
#include "shaders/keptech/downsample.h"
#include "shaders/keptech/lightCombine.h"
#include "shaders/keptech/pointLight.h"
#include "shaders/keptech/pointLightShadows.h"
#include "shaders/keptech/ssao.h"
#include "shaders/keptech/ssaoBlur.h"
#include "shaders/keptech/upsample.h"
  } // namespace
} // namespace shaders

namespace kt::vkh::setup {

  namespace {
    struct Shader {
      VkShaderModule module;
      std::vector<VkPipelineShaderStageCreateInfo> stages;
    };

    std::expected<Shader, std::string> getShader(const shaders::Shader& shader, const VkDevice device) {
      VK_ASSERT(!shader.code.empty(), "Shader code is empty.");
      VK_ASSERT(!shader.stages.empty(), "Shader stages are empty.");
      VkShaderModule shaderModule = nullptr;
      VkShaderModuleCreateInfo shaderModuleCreateInfo{
          .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
          .codeSize = shader.code.size() * sizeof(uint8_t),
          .pCode = reinterpret_cast<const uint32_t*>(shader.code.data()),
      };
      VK_MAKE(vkCreateShaderModule(device, &shaderModuleCreateInfo, nullptr, &shaderModule),
              "Failed to create shader module for deferred pipeline.");
      VK_ASSERT(shaderModule != nullptr, "Shader module creation returned null.");

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
      uint32_t location = 0;
      for (auto& param : shader.vertexLayout) {
        uint32_t voffset = 0;
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

    std::expected<Pipeline, std::string> createPipeline(const shaders::Shader& shader, GraphicsPipelineConfig&& pc,
                                                        PipelineLayoutConfig&& plc, const VkDevice device) {
      VKH_MAKE(vkShader, getShader(shader, device), "Failed to create shader for pipeline.");
      VK_ASSERT(vkShader.stages.size() == shader.stages.size(), "Shader stages size mismatch between shader and pipeline config.");
      pc.shaders = vkShader.stages;

      auto vkConfig = pc.build();
      auto vkLayoutInfo = plc.build();

      const void* const colorAttchmentFormats = reinterpret_cast<const void* const>(
          static_cast<const VkPipelineRenderingCreateInfo* const>(vkConfig.pNext)->pColorAttachmentFormats);
      const void* const vertexInputBindings = reinterpret_cast<const void* const>(vkConfig.pVertexInputState->pVertexBindingDescriptions);
      const void* const vertexInputAttributes =
          reinterpret_cast<const void* const>(vkConfig.pVertexInputState->pVertexAttributeDescriptions);
      const void* const shaderStages = reinterpret_cast<const void* const>(vkConfig.pStages);

      VkPipelineLayout vkLayout = nullptr;
      VK_MAKE(vkCreatePipelineLayout(device, &vkLayoutInfo, nullptr, &vkLayout), "Failed to create pipeline layout.");

      VkPipeline vkPipeline = nullptr;
      vkConfig.layout = vkLayout;
      VK_MAKE(vkCreateGraphicsPipelines(device, nullptr, 1, &vkConfig, nullptr, &vkPipeline), "Failed to create graphics pipeline.");

      vkDestroyShaderModule(device, vkShader.module, nullptr);

      return Pipeline{
          .layout = vkLayout,
          .pipeline = vkPipeline,
      };
    }

    std::expected<Pipeline, std::string> createPipeline(const shaders::Shader& shader, PipelineLayoutConfig&& plc, const VkDevice device) {
      VKH_MAKE(vkShader, getShader(shader, device), "Failed to create shader for pipeline.");
      VK_ASSERT(vkShader.stages.size() == shader.stages.size(), "Shader stages size mismatch between shader and pipeline config.");

      VK_ASSERT(shader.stages.size() == 1, "Compute Shader has more than one entry, don't know which to use.");

      VkPipelineShaderStageCreateInfo shaderStageInfo = {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = from(shader.stages[0].stage),
          .module = vkShader.module,
          .pName = shader.stages[0].name,
      };

      auto vkLayoutInfo = plc.build();

      VkPipelineLayout vkLayout = nullptr;
      VK_MAKE(vkCreatePipelineLayout(device, &vkLayoutInfo, nullptr, &vkLayout), "Failed to create pipeline layout.");

      VkComputePipelineCreateInfo pipelineCreateInfo{
          .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
          .stage = shaderStageInfo,
          .layout = vkLayout,
      };

      VkPipeline vkPipeline = nullptr;
      VK_CHECK(vkCreateComputePipelines(device, nullptr, 1, &pipelineCreateInfo, nullptr, &vkPipeline),
               "Failed to create compute pipeline for shader.");

      vkDestroyShaderModule(device, vkShader.module, nullptr);

      return Pipeline{
          .layout = vkLayout,
          .pipeline = vkPipeline,
      };
    }
  } // namespace
  namespace {
    std::expected<Pipeline, std::string> createBasicPipeline(const Renderer::VulkanCore& vkcore, const Formats& formats,
                                                             const VkDescriptorSetLayout globalLayout) {
      VK_DEBUG("Creating basic pipeline");
      return createPipeline(::shaders::basic,
                            GraphicsPipelineConfig{
                                .rendering = {.colorAttachmentFormats = {formats.swapchain}},
                                .vertexInput = getVertexInputFromShader(::shaders::basic, {}),
                            },
                            PipelineLayoutConfig{.setLayouts = {},
                                                 .pushConstantRanges = {{
                                                     .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                                     .offset = 0,
                                                     .size = sizeof(glm::mat4) * 2,
                                                 }}},
                            vkcore.device.logical);
    }

    std::expected<Pipeline, std::string> createDeferredPipeline(const Renderer::VulkanCore& vkcore, const Formats& formats,
                                                                const VkDescriptorSetLayout globalLayout) {
      VK_DEBUG("Creating deferred pipeline");

      return createPipeline(::shaders::deferred,
                            GraphicsPipelineConfig{
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
                                .vertexInput = getVertexInputFromShader(::shaders::deferred, {}),
                                .rasterizer =
                                    {
                                        .polygonMode = VK_POLYGON_MODE_FILL,
                                        .cullMode = VK_CULL_MODE_BACK_BIT,
                                        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
                                        .lineWidth = 1.f,
                                    },
                                .depthStencilState =
                                    {
                                        .depthBoundsTestEnable = VK_TRUE,
                                        .depthTestEnable = VK_TRUE,
                                        .depthCompareOp = VK_COMPARE_OP_LESS,
                                        .depthWriteEnable = VK_TRUE,
                                        .minDepthBounds = 0.f,
                                        .maxDepthBounds = 1.f,
                                    },
                            },
                            PipelineLayoutConfig{
                                .setLayouts = {globalLayout},
                                .pushConstantRanges = {{
                                    .stageFlags = VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                    .offset = 0,
                                    .size = sizeof(VkDeviceAddress) * 7 + sizeof(uint32_t) * 5,
                                }},
                            },
                            vkcore.device.logical);
    }

    std::expected<Pipeline, std::string> createPointLightShadowsPipeline(const Renderer::VulkanCore& vkcore, const Formats& formats,
                                                                         const VkDescriptorSetLayout globalLayout) {
      VK_DEBUG("Creating point light shadows pipeline");

      return createPipeline(::shaders::pointLightShadows,
                            GraphicsPipelineConfig{
                                .rendering =
                                    {
                                        .colorAttachmentFormats = {},
                                        .depthAttachmentFormat = formats.render.depth,
                                    },
                                .vertexInput = getVertexInputFromShader(::shaders::pointLightShadows, {}),
                                .rasterizer =
                                    {
                                        .polygonMode = VK_POLYGON_MODE_FILL,
                                        .cullMode = VK_CULL_MODE_FRONT_BIT,
                                        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
                                        .lineWidth = 1.f,
                                    },
                                .depthStencilState =
                                    {
                                        .depthBoundsTestEnable = VK_FALSE,
                                        .depthTestEnable = VK_TRUE,
                                        .depthCompareOp = VK_COMPARE_OP_LESS,
                                        .depthWriteEnable = VK_TRUE,
                                        .minDepthBounds = 0.f,
                                        .maxDepthBounds = 1.f,
                                    },
                            },
                            PipelineLayoutConfig{
                                .setLayouts = {globalLayout},
                                .pushConstantRanges = {{
                                    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                    .offset = 0,
                                    .size = sizeof(VkDeviceAddress) * 3 + sizeof(uint32_t) * 2,
                                }},
                            },
                            vkcore.device.logical);
    }

    std::expected<Pipeline, std::string> createDeferredPointLightPipeline(const Renderer::VulkanCore& vkcore, const Formats& formats,
                                                                          const VkDescriptorSetLayout globalLayout) {
      VK_DEBUG("Creating deferred point light pipeline");

      return createPipeline(::shaders::pointLight,
                            GraphicsPipelineConfig{.rendering =
                                                       {
                                                           .colorAttachmentFormats =
                                                               {
                                                                   formats.render.hdr,
                                                                   formats.render.hdr,
                                                               },
                                                       },
                                                   .rasterizer =
                                                       {
                                                           .polygonMode = VK_POLYGON_MODE_FILL,
                                                           .cullMode = VK_CULL_MODE_FRONT_BIT,
                                                           .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
                                                           .lineWidth = 1.f,
                                                       },
                                                   .blendAttachments =
                                                       {
                                                           VkPipelineColorBlendAttachmentState{
                                                               .blendEnable = VK_TRUE,
                                                               .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
                                                               .dstColorBlendFactor = VK_BLEND_FACTOR_ONE,
                                                               .colorBlendOp = VK_BLEND_OP_ADD,
                                                               .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                                                               .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                                                               .alphaBlendOp = VK_BLEND_OP_ADD,
                                                               .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                                                                 VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
                                                           },
                                                           VkPipelineColorBlendAttachmentState{
                                                               .blendEnable = VK_TRUE,
                                                               .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
                                                               .dstColorBlendFactor = VK_BLEND_FACTOR_ONE,
                                                               .colorBlendOp = VK_BLEND_OP_ADD,
                                                               .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                                                               .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                                                               .alphaBlendOp = VK_BLEND_OP_ADD,
                                                               .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                                                                 VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
                                                           },
                                                       }},
                            PipelineLayoutConfig{
                                .setLayouts = {globalLayout},
                                .pushConstantRanges = {{
                                    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                    .offset = 0,
                                    .size = sizeof(VkDeviceAddress),
                                }},
                            },
                            vkcore.device.logical);
    }

    std::expected<Pipeline, std::string> createSsaoPipeline(const Renderer::VulkanCore& vkcore, const Formats& formats,
                                                            const VkDescriptorSetLayout globalLayout,
                                                            const VkDescriptorSetLayout staticLayout) {
      VK_DEBUG("Creating SSAO pipeline");

      return createPipeline(::shaders::ssao,
                            PipelineLayoutConfig{
                                .setLayouts = {globalLayout, staticLayout},
                                .pushConstantRanges = {},
                            },
                            vkcore.device.logical);
    }

    std::expected<Pipeline, std::string> createSsaoBlurPipeline(const Renderer::VulkanCore& vkcore, const Formats& formats,
                                                                const VkDescriptorSetLayout globalLayout) {
      VK_DEBUG("Creating SSAO blur pipeline");

      return createPipeline(::shaders::ssaoBlur,
                            GraphicsPipelineConfig{
                                .rendering =
                                    {
                                        .colorAttachmentFormats =
                                            {
                                                VK_FORMAT_R8_UNORM,
                                            },
                                    },
                            },
                            PipelineLayoutConfig{
                                .setLayouts = {globalLayout},
                                .pushConstantRanges = {{
                                    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                    .offset = 0,
                                    .size = sizeof(glm::vec2),
                                }},
                            },
                            vkcore.device.logical);
    }

    std::expected<Pipeline, std::string> createLightCombinePipeline(const Renderer::VulkanCore& vkcore, const Formats& formats,
                                                                    const VkDescriptorSetLayout globalLayout) {
      VK_DEBUG("Creating light combine pipeline");

      return createPipeline(::shaders::lightCombine,
                            GraphicsPipelineConfig{
                                .rendering =
                                    {
                                        .colorAttachmentFormats =
                                            {
                                                formats.render.hdr,
                                            },
                                    },
                            },
                            PipelineLayoutConfig{
                                .setLayouts = {globalLayout},
                            },
                            vkcore.device.logical);
    }

    std::expected<Pipeline, std::string> createBloomDownsamplePipeline(const Renderer::VulkanCore& vkcore, const Formats& formats,
                                                                       const VkDescriptorSetLayout globalLayout) {
      VK_DEBUG("Creating bloom downsample pipeline");

      return createPipeline(::shaders::downsample,
                            GraphicsPipelineConfig{
                                .rendering =
                                    {
                                        .colorAttachmentFormats =
                                            {
                                                formats.render.hdr,
                                            },
                                    },
                            },
                            PipelineLayoutConfig{
                                .setLayouts = {globalLayout},
                                .pushConstantRanges = {{
                                    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                    .offset = 0,
                                    .size = sizeof(glm::vec2) + sizeof(uint32_t),
                                }},
                            },
                            vkcore.device.logical);
    }

    std::expected<Pipeline, std::string> createBloomUpsamplePipeline(const Renderer::VulkanCore& vkcore, const Formats& formats,
                                                                     const VkDescriptorSetLayout globalLayout) {
      VK_DEBUG("Creating bloom upsample pipeline");

      return createPipeline(::shaders::upsample,
                            GraphicsPipelineConfig{
                                .rendering =
                                    {
                                        .colorAttachmentFormats =
                                            {
                                                formats.render.hdr,
                                            },
                                    },
                                .blendAttachments =
                                    {
                                        VkPipelineColorBlendAttachmentState{
                                            .blendEnable = VK_TRUE,
                                            .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
                                            .dstColorBlendFactor = VK_BLEND_FACTOR_ONE,
                                            .colorBlendOp = VK_BLEND_OP_ADD,
                                            .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                                            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                                            .alphaBlendOp = VK_BLEND_OP_ADD,
                                            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
                                        },
                                    },
                            },
                            PipelineLayoutConfig{
                                .setLayouts = {globalLayout},
                                .pushConstantRanges = {{
                                    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                    .offset = 0,
                                    .size = sizeof(float) + sizeof(uint32_t),
                                }},
                            },
                            vkcore.device.logical);
    }

    std::expected<Pipeline, std::string> createBloomCombinePipeline(const Renderer::VulkanCore& vkcore, const Formats& formats,
                                                                    const VkDescriptorSetLayout globalLayout) {
      VK_DEBUG("Creating bloom combine pipeline");

      return createPipeline(::shaders::bloomCombine,
                            GraphicsPipelineConfig{
                                .rendering =
                                    {
                                        .colorAttachmentFormats =
                                            {
                                                formats.swapchain,
                                            },
                                    },
                            },
                            PipelineLayoutConfig{
                                .setLayouts = {globalLayout},
                                .pushConstantRanges = {},
                            },
                            vkcore.device.logical);
    }

  } // namespace

  std::expected<Renderer::Pipelines, std::string> createPipelines(const Renderer::VulkanCore& vkcore, const Formats& formats,
                                                                  const VkDescriptorSetLayout globalLayout,
                                                                  const VkDescriptorSetLayout staticLayout) {
    VKH_MAKE(basic, createBasicPipeline(vkcore, formats, globalLayout), "Failed to create basic pipeline.");
    VKH_MAKE(deferred, createDeferredPipeline(vkcore, formats, globalLayout), "Failed to create deferred pipeline.");
    VKH_MAKE(pointLightShadows, createPointLightShadowsPipeline(vkcore, formats, globalLayout),
             "Failed to create point light shadows pipeline.");
    VKH_MAKE(deferredPointLight, createDeferredPointLightPipeline(vkcore, formats, globalLayout),
             "Failed to create deferred point light pipeline.");
    VKH_MAKE(ssao, createSsaoPipeline(vkcore, formats, globalLayout, staticLayout), "Failed to create SSAO pipeline.");
    VKH_MAKE(ssaoBlur, createSsaoBlurPipeline(vkcore, formats, globalLayout), "Failed to create SSAO blur pipeline.");
    VKH_MAKE(lightCombine, createLightCombinePipeline(vkcore, formats, globalLayout), "Failed to create light combine pipeline.");
    VKH_MAKE(bloomDownsample, createBloomDownsamplePipeline(vkcore, formats, globalLayout), "Failed to create bloom downsample pipeline.");
    VKH_MAKE(bloomUpsample, createBloomUpsamplePipeline(vkcore, formats, globalLayout), "Failed to create bloom upsample pipeline.");
    VKH_MAKE(bloomCombine, createBloomCombinePipeline(vkcore, formats, globalLayout), "Failed to create bloom combine pipeline.");

    return Renderer::Pipelines{
        .basic = basic,
        .deferred = deferred,
        .pointLightShadows = pointLightShadows,
        .deferredPointLight = deferredPointLight,
        .ssao = ssao,
        .ssaoBlur = ssaoBlur,
        .deferredCombine = lightCombine,
        .bloomDownsample = bloomDownsample,
        .bloomUpsample = bloomUpsample,
        .bloomCombine = bloomCombine,
    };
  }
} // namespace kt::vkh::setup
