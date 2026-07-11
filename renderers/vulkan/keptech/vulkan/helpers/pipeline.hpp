#pragma once

#include <Volk/volk.h>
#include <optional>
#include <span>
#include <vector>

namespace kt::vkh {
  struct VertexInput {
    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attributes;
  };

  class DynamicStateInfo {
    std::vector<VkDynamicState> dynamicStates;
    VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo;

  public:
    ~DynamicStateInfo() = default;
    constexpr DynamicStateInfo(std::initializer_list<VkDynamicState> args) noexcept
        : dynamicStates{args}, dynamicStateCreateInfo{
                                   VkPipelineDynamicStateCreateInfo{
                                       .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                                       .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
                                       .pDynamicStates = dynamicStates.data(),
                                   },
                               } {}

    DynamicStateInfo(const DynamicStateInfo& o)
        : dynamicStates{o.dynamicStates}, dynamicStateCreateInfo{
                                              VkPipelineDynamicStateCreateInfo{
                                                  .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                                                  .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
                                                  .pDynamicStates = dynamicStates.data(),
                                              },
                                          } {}
    DynamicStateInfo(DynamicStateInfo&& o) noexcept
        : dynamicStates{std::move(o.dynamicStates)}, dynamicStateCreateInfo{
                                                         VkPipelineDynamicStateCreateInfo{
                                                             .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                                                             .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
                                                             .pDynamicStates = dynamicStates.data(),
                                                         },
                                                     } {}
    DynamicStateInfo& operator=(const DynamicStateInfo& o) {
      dynamicStates = o.dynamicStates;
      dynamicStateCreateInfo = VkPipelineDynamicStateCreateInfo{
          .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
          .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
          .pDynamicStates = dynamicStates.data(),
      };
      return *this;
    }
    DynamicStateInfo& operator=(DynamicStateInfo&& o) noexcept {
      dynamicStates = std::move(o.dynamicStates);
      dynamicStateCreateInfo = VkPipelineDynamicStateCreateInfo{
          .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
          .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
          .pDynamicStates = dynamicStates.data(),
      };
      return *this;
    }

    operator VkPipelineDynamicStateCreateInfo*() noexcept { return &dynamicStateCreateInfo; }
  };

  struct PipelineLayoutConfig {
    std::vector<VkDescriptorSetLayout> setLayouts = {};
    std::vector<VkPushConstantRange> pushConstantRanges = {};

    VkPipelineLayoutCreateInfo build() noexcept;
  };

  struct RenderingConfig {
    void* pNext = nullptr;
    uint32_t viewMask = 0;
    std::vector<VkFormat> colorAttachmentFormats{};
    VkFormat depthAttachmentFormat = VkFormat::VK_FORMAT_UNDEFINED;
    VkFormat stencilAttachmentFormat = VkFormat::VK_FORMAT_UNDEFINED;

    VkPipelineRenderingCreateInfo build() noexcept;
  };

  struct GraphicsPipelineConfig {
    GraphicsPipelineConfig& colorAttachments(std::initializer_list<VkFormat> formats) noexcept {
      rendering.colorAttachmentFormats = formats;
      return *this;
    }
    GraphicsPipelineConfig& depthAttachment(VkFormat format) noexcept {
      rendering.depthAttachmentFormat = format;
      return *this;
    }
    GraphicsPipelineConfig& stencilAttachment(VkFormat format) noexcept {
      rendering.stencilAttachmentFormat = format;
      return *this;
    }
    GraphicsPipelineConfig& vertexInput(VertexInput input) noexcept {
      _vertexInput = std::move(input);
      return *this;
    }
    GraphicsPipelineConfig& shaders(std::span<VkPipelineShaderStageCreateInfo> stages) noexcept {
      _shaders = stages;
      return *this;
    }
    GraphicsPipelineConfig& cullMode(VkCullModeFlags cullMode) noexcept {
      rasterizer.cullMode = cullMode;
      return *this;
    }
    GraphicsPipelineConfig& frontFace(VkFrontFace frontFace) noexcept {
      rasterizer.frontFace = frontFace;
      return *this;
    }
    GraphicsPipelineConfig& polygonMode(VkPolygonMode polygonMode) noexcept {
      rasterizer.polygonMode = polygonMode;
      return *this;
    }
    GraphicsPipelineConfig& depthTest(bool enable = true) noexcept {
      depthStencilState.depthTestEnable = enable ? VK_TRUE : VK_FALSE;
      return *this;
    }
    GraphicsPipelineConfig& depthWrite(bool enable = true) noexcept {
      depthStencilState.depthWriteEnable = enable ? VK_TRUE : VK_FALSE;
      return *this;
    }
    GraphicsPipelineConfig& depthCompareOp(VkCompareOp compareOp) noexcept {
      depthStencilState.depthCompareOp = compareOp;
      return *this;
    }
    GraphicsPipelineConfig& noBlending() noexcept {
      blendAttachments.clear();
      for (auto& c : rendering.colorAttachmentFormats) {
        blendAttachments.push_back(VkPipelineColorBlendAttachmentState{
            .blendEnable = VK_FALSE,
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT});
      }
      return *this;
    }
    GraphicsPipelineConfig& additiveBlending() noexcept {
      blendAttachments.clear();
      for (auto& c : rendering.colorAttachmentFormats) {
        blendAttachments.push_back(VkPipelineColorBlendAttachmentState{
            .blendEnable = VK_TRUE,
            .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ONE,
            .colorBlendOp = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
            .alphaBlendOp = VK_BLEND_OP_ADD,
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT});
      }
      return *this;
    }

    GraphicsPipelineConfig& layout(VkPipelineLayout layout) noexcept {
      _layout = layout;
      return *this;
    }

    RenderingConfig rendering{};
    std::span<VkPipelineShaderStageCreateInfo> _shaders = {};
    VertexInput _vertexInput{};
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VkPrimitiveTopology::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };
    VkPipelineViewportStateCreateInfo viewport = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };
    VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VkPolygonMode::VK_POLYGON_MODE_FILL,
        .cullMode = VkCullModeFlagBits::VK_CULL_MODE_NONE,
        .frontFace = VkFrontFace::VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .depthBiasConstantFactor = 0.0f,
        .depthBiasSlopeFactor = 1.0f,
        .lineWidth = 1.0f,
    };
    VkPipelineMultisampleStateCreateInfo multisampling = {.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                                                          .rasterizationSamples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT,
                                                          .sampleShadingEnable = VK_FALSE,
                                                          .minSampleShading = 1.0f};
    VkPipelineDepthStencilStateCreateInfo depthStencilState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_FALSE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VkCompareOp::VK_COMPARE_OP_LESS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
        .minDepthBounds = 0.0f,
        .maxDepthBounds = 1.0f,
    };
    std::vector<VkPipelineColorBlendAttachmentState> blendAttachments = {};
    VkPipelineColorBlendStateCreateInfo blending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
    };
    DynamicStateInfo dynamicState = {VkDynamicState::VK_DYNAMIC_STATE_VIEWPORT, VkDynamicState::VK_DYNAMIC_STATE_SCISSOR};

    VkPipelineLayout _layout = nullptr;

    std::optional<VkPipelineVertexInputStateCreateInfo> _internalVertexInputInfo = std::nullopt;

    std::optional<VkPipelineRenderingCreateInfo> _internalRenderingInfo = std::nullopt;

    VkGraphicsPipelineCreateInfo build() noexcept;
  };
} // namespace kt::vkh
