#include "helpers/pipeline.hpp"
#include "setup.hpp"

#include "core.hpp"
#include "keptech/render/structs.hpp"
#include "keptech/render/wrappers/pipeline.hpp"
#include "macros.hpp"
#include "pipelines.hpp"
#include <Volk/volk.h>
#include <keptech/components/lights.hpp>
#include <keptech/rendering/structs.hpp>
#include <keptech/shaders/shader.h>

#include "shaders/keptech/basic.h"
#include "shaders/keptech/lightCombine.h"
#include "shaders/keptech/mesh_shader.h"
#include "shaders/keptech/pointLight.h"
#include "shaders/keptech/pointLightShadows.h"
#include "shaders/keptech/ssao.h"
#include "shaders/keptech/ssaoBlur.h"

namespace kt::vkh::setup {
  namespace {
    struct Configs {
      GraphicsPipelineConfig basic;
      GraphicsPipelineConfig meshShader;
      GraphicsPipelineConfig pointLightShadows;
      GraphicsPipelineConfig deferredPointLight;
      GraphicsPipelineConfig ssaoBlur;
      GraphicsPipelineConfig hdrNoBlend;

      PipelineLayoutConfig meshShaderLayout;
      PipelineLayoutConfig pointLightShadowsLayout;
      PipelineLayoutConfig pointLightLayout;
      PipelineLayoutConfig ssaoLayout;
      PipelineLayoutConfig ssaoBlurLayout;
      PipelineLayoutConfig lightCombineLayout;
    };
    Configs createConfigs(const Formats& formats);

    struct LayoutConfigs;
    LayoutConfigs createLayoutConfigs(const VkDescriptorSetLayout globalLayout, const VkDescriptorSetLayout staticLayout);

    std::expected<Layouts, std::string> createLayouts(const VkDevice device, const VkDescriptorSetLayout globalLayout,
                                                      const VkDescriptorSetLayout staticLayout);

    struct Shaders {
      Shader basic;
      Shader meshShader;
      Shader pointLightShadows;
      Shader pointLight;
      Shader ssao;
      Shader ssaoBlur;
      Shader lightCombine;
    };
    std::expected<Shaders, std::string> createShaders(const VkDevice device);
    void destroyShaders(const VkDevice device, Shaders& shaders);

  } // namespace

  std::expected<Pipelines, std::string> createPipelines(const VulkanCore& vkcore, const Formats& formats, const Layouts& layouts) {
    VKH_MAKE(shaders, createShaders(vkcore.device), "Failed to create shaders.");
    auto configs = createConfigs(formats);

    auto basic = configs.basic.shaders(shaders.basic).layout(layouts.onlyGlobals);
    auto mesh_shader = configs.meshShader.shaders(shaders.meshShader).layout(layouts.meshShaderLayout);
    auto pointLightShadows = configs.pointLightShadows.shaders(shaders.pointLightShadows).layout(layouts.pointLightShadowsLayout);
    auto deferredPointLight = configs.deferredPointLight.shaders(shaders.pointLight).layout(layouts.pointLightLayout);
    auto ssaoBlur = configs.ssaoBlur.shaders(shaders.ssaoBlur).layout(layouts.ssaoBlurLayout);
    auto lightCombine = configs.hdrNoBlend.shaders(shaders.lightCombine).layout(layouts.onlyGlobals);

    VKH_MAKE(
        graphics,
        Pipeline::createGraphics<6>(vkcore.device, {basic, mesh_shader, pointLightShadows, deferredPointLight, ssaoBlur, lightCombine}),
        "Failed to create basic graphics pipeline.");

    VKH_MAKE(ssao, Pipeline::createCompute(vkcore.device, shaders.ssao, layouts.onlyGlobals), "Failed to create SSAO compute pipeline.");

    destroyShaders(vkcore.device, shaders);

    return Pipelines{
        .basic = graphics[0],
        .mesh_shader = graphics[1],
        .pointLightShadows = graphics[2],
        .deferredPointLight = graphics[3],
        .ssao = ssao,
        .ssaoBlur = graphics[4],
        .deferredCombine = graphics[5],
    };
  }

  namespace {
    struct LayoutConfigs {
      PipelineLayoutConfig onlyGlobals;
      PipelineLayoutConfig meshShaderLayout;
      PipelineLayoutConfig pointLightShadowsLayout;
      PipelineLayoutConfig pointLightLayout;
      PipelineLayoutConfig ssaoBlurLayout;
    };
  } // namespace

  std::expected<Layouts, std::string> createLayouts(const VkDevice device, const VkDescriptorSetLayout globalLayout,
                                                    const VkDescriptorSetLayout staticLayout) {
    auto configs = createLayoutConfigs(globalLayout, staticLayout);

    VkPipelineLayoutCreateInfo blankInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    };
    VkPipelineLayout empty = nullptr;
    VK_MAKE(vkCreatePipelineLayout(device, &blankInfo, nullptr, &empty), "Failed to create empty pipeline layout.");

    VKH_MAKE(onlyGlobals, Pipeline::createLayout(device, configs.onlyGlobals), "Failed to create onlyGlobals pipeline layout.");
    VKH_MAKE(meshShaderLayout, Pipeline::createLayout(device, configs.meshShaderLayout), "Failed to create meshShader pipeline layout.");
    VKH_MAKE(pointLightShadowsLayout, Pipeline::createLayout(device, configs.pointLightShadowsLayout),
             "Failed to create pointLightShadows pipeline layout.");
    VKH_MAKE(pointLightLayout, Pipeline::createLayout(device, configs.pointLightLayout), "Failed to create pointLight pipeline layout.");
    VKH_MAKE(ssaoBlurLayout, Pipeline::createLayout(device, configs.ssaoBlurLayout), "Failed to create ssaoBlur pipeline layout.");

    return Layouts{
        .empty = empty,
        .onlyGlobals = onlyGlobals,
        .meshShaderLayout = meshShaderLayout,
        .pointLightShadowsLayout = pointLightShadowsLayout,
        .pointLightLayout = pointLightLayout,
        .ssaoBlurLayout = ssaoBlurLayout,
    };
  }

  namespace {
    std::expected<Shaders, std::string> createShaders(const VkDevice device) {
      VKH_MAKE(basic, Shader::create(device, ::shaders::basic), "Failed to create basic shader.");
      VKH_MAKE(meshShader, Shader::create(device, ::shaders::mesh_shader), "Failed to create mesh shader.");
      VKH_MAKE(pointLightShadows, Shader::create(device, ::shaders::pointLightShadows), "Failed to create point light shadows shader.");
      VKH_MAKE(pointLight, Shader::create(device, ::shaders::pointLight), "Failed to create point light shader.");
      VKH_MAKE(ssao, Shader::create(device, ::shaders::ssao), "Failed to create SSAO shader.");
      VKH_MAKE(ssaoBlur, Shader::create(device, ::shaders::ssaoBlur), "Failed to create SSAO blur shader.");
      VKH_MAKE(lightCombine, Shader::create(device, ::shaders::lightCombine), "Failed to create light combine shader.");

      return Shaders{
          .basic = basic,
          .meshShader = meshShader,
          .pointLightShadows = pointLightShadows,
          .pointLight = pointLight,
          .ssao = ssao,
          .ssaoBlur = ssaoBlur,
          .lightCombine = lightCombine,
      };
    }

    void destroyShaders(const VkDevice device, Shaders& shaders) {
      shaders.basic.destroy(device);
      shaders.meshShader.destroy(device);
      shaders.pointLightShadows.destroy(device);
      shaders.pointLight.destroy(device);
      shaders.ssao.destroy(device);
      shaders.ssaoBlur.destroy(device);
      shaders.lightCombine.destroy(device);
    }

    Configs createConfigs(const Formats& formats) {
      return Configs{
          .basic = GraphicsPipelineConfig{}
                       .colorAttachments({formats.swapchain})
                       .vertexInput(Shader::getVertexInput(::shaders::basic))
                       .cullMode(VK_CULL_MODE_BACK_BIT)
                       .noBlending(),
          .meshShader =
              GraphicsPipelineConfig{}
                  .colorAttachments({formats.render.albedo, formats.render.normal, formats.render.emissive, formats.render.metRought})
                  .depthAttachment(formats.render.depth)
                  .vertexInput(Shader::getVertexInput(::shaders::mesh_shader))
                  //.cullMode(VK_CULL_MODE_BACK_BIT)
                  .depthTest()
                  .noBlending(),
          .pointLightShadows = GraphicsPipelineConfig{}
                                   .depthAttachment(formats.render.depth)
                                   .vertexInput(Shader::getVertexInput(::shaders::pointLightShadows))
                                   .cullMode(VK_CULL_MODE_FRONT_BIT)
                                   .depthTest(),
          .deferredPointLight = GraphicsPipelineConfig{}
                                    .colorAttachments({formats.render.hdr, formats.render.hdr})
                                    .depthAttachment(formats.render.depth)
                                    .vertexInput(Shader::getVertexInput(::shaders::pointLight))
                                    .cullMode(VK_CULL_MODE_FRONT_BIT)
                                    .depthTest()
                                    .additiveBlending(),
          .ssaoBlur = GraphicsPipelineConfig{}.colorAttachments({VK_FORMAT_R8_UNORM}).noBlending(),
          .hdrNoBlend = GraphicsPipelineConfig{}.colorAttachments({formats.render.hdr}).noBlending(),
      };
    }

    LayoutConfigs createLayoutConfigs(const VkDescriptorSetLayout globalLayout, const VkDescriptorSetLayout staticLayout) {
      return LayoutConfigs{
          .onlyGlobals =
              PipelineLayoutConfig{
                  .setLayouts = {globalLayout, staticLayout},
                  .pushConstantRanges = {},
              },
          .meshShaderLayout =
              PipelineLayoutConfig{
                  .setLayouts = {globalLayout, staticLayout},
                  .pushConstantRanges = {{
                      .stageFlags = VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT,
                      .offset = 0,
                      .size = sizeof(glm::mat4) + (sizeof(uint32_t) * 2), // model matrix + material index + meshlet count
                  }},
              },
          .pointLightShadowsLayout =
              PipelineLayoutConfig{
                  .setLayouts = {globalLayout, staticLayout},
                  .pushConstantRanges = {{
                      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                      .offset = 0,
                      .size = sizeof(VkDeviceAddress) * 3 + sizeof(uint32_t) * 2,
                  }},
              },
          .pointLightLayout =
              PipelineLayoutConfig{
                  .setLayouts = {globalLayout, staticLayout},
                  .pushConstantRanges = {{
                      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                      .offset = 0,
                      .size = sizeof(VkDeviceAddress),
                  }},
              },
          .ssaoBlurLayout =
              PipelineLayoutConfig{
                  .setLayouts = {globalLayout, staticLayout},
                  .pushConstantRanges = {{
                      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                      .offset = 0,
                      .size = sizeof(glm::vec2),
                  }},
              },
      };
    }
  } // namespace
} // namespace kt::vkh::setup
