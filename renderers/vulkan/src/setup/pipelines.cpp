#include "setup.hpp"

#include "helpers/conversions.hpp"
#include "keptech/vulkan/helpers/formatting.hpp"
#include "keptech/vulkan/helpers/pipeline.hpp"
#include "keptech/vulkan/structs.hpp"
#include "macros.hpp"
#include <keptech/components/lights.hpp>
#include <keptech/rendering/structs.hpp>
#include <keptech/shaders/shader.h>
#include <vulkan/vulkan.h>

namespace shaders {
  namespace {
#include "shaders/keptech/basic.h"
#include "shaders/keptech/deferred.h"
#include "shaders/keptech/lightCombine.h"
#include "shaders/keptech/pointLight.h"
  } // namespace
} // namespace shaders

namespace kt::vkh::setup {

  namespace {

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

    std::expected<Pipeline, std::string> createPipeline(const shaders::Shader& shader, GraphicsPipelineConfig&& pc,
                                                        PipelineLayoutConfig&& plc, const VkDevice device) {
      VKH_MAKE(vkShader, getShader(shader, device), "Failed to create shader for pipeline.");
      pc.shaders = vkShader.stages;

      auto vkConfig = pc.build();
      auto vkLayoutInfo = plc.build();

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
                                    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                    .offset = 0,
                                    .size = sizeof(glm::mat4) + sizeof(VkDeviceAddress),
                                }},
                            },
                            vkcore.device.logical);
    }

    std::expected<Pipeline, std::string> createDeferredPointLightPipeline(const Renderer::VulkanCore& vkcore, const Formats& formats,
                                                                          const VkDescriptorSetLayout globalLayout) {
      VK_DEBUG("Creating deferred point light pipeline");

      return createPipeline(::shaders::pointLight,
                            GraphicsPipelineConfig{
                                .rendering =
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
                            },
                            PipelineLayoutConfig{
                                .setLayouts = {globalLayout},
                                .pushConstantRanges = {{
                                    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                    .offset = 0,
                                    .size = sizeof(rendering::PointLight),
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
  } // namespace

  std::expected<Renderer::Pipelines, std::string> createPipelines(const Renderer::VulkanCore& vkcore, const Formats& formats,
                                                                  const VkDescriptorSetLayout globalLayout) {
    VKH_MAKE(basic, createBasicPipeline(vkcore, formats, globalLayout), "Failed to create basic pipeline.");
    VKH_MAKE(deferred, createDeferredPipeline(vkcore, formats, globalLayout), "Failed to create deferred pipeline.");
    VKH_MAKE(deferredPointLight, createDeferredPointLightPipeline(vkcore, formats, globalLayout),
             "Failed to create deferred point light pipeline.");
    VKH_MAKE(lightCombine, createLightCombinePipeline(vkcore, formats, globalLayout), "Failed to create light combine pipeline.");

    return Renderer::Pipelines{
        .basic = basic,
        .deferred = deferred,
        .deferredPointLight = deferredPointLight,
        .deferredCombine = lightCombine,
    };
  }
} // namespace kt::vkh::setup
