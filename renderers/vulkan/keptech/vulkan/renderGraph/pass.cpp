#include "pass.hpp"

#include "builder.hpp"
#include "helpers/formatting.hpp"
#include "vk-logger.hpp"
#include <algorithm>

namespace kt::vkh {
  static constexpr Bitflag<QueueType> COMPUTE_QUEUES = QueueType::Compute | QueueType::AsyncCompute;

#define LOG(...) VK_DEBUG(__VA_ARGS__)

  RenderTextureResource& RenderPassBuilder::addAttachmentInput(const std::string& name) {
    LOG("Adding attachment input '{}' to pass '{}'", name, this->name);
    auto& res = graph.getTextureResource(name);
    res.addImageUsage(VK_IMAGE_USAGE_SAMPLED_BIT).addQueue(queue).readInPass(id);
    attachmentInputs.push_back(&res);
    return res;
  }

  RenderTextureResource& RenderPassBuilder::addHistoryInput(const std::string& name) {
    LOG("Adding history input '{}' to pass '{}'", name, this->name);
    auto& res = graph.getTextureResource(name);
    res.addImageUsage(VK_IMAGE_USAGE_SAMPLED_BIT).addQueue(queue);
    historyInputs.push_back(&res);
    return res;
  }

  RenderBufferResource& RenderPassBuilder::addGenericBufferInput(const std::string& name, VkPipelineStageFlags2 stages,
                                                                 VkAccessFlags2 access, VkBufferUsageFlags usage) {
    LOG("Adding generic buffer input '{}' to pass '{}'", name, this->name);
    auto& res = graph.getBufferResource(name);

    res.addBufferUsage(usage).addQueue(queue).readInPass(id);

    AccessedBufferResource acc{.buffer = &res};
    acc.layout = VK_IMAGE_LAYOUT_GENERAL;
    acc.access = access;
    acc.stages = stages;

    genericBuffers.push_back(acc);

    return res;
  }

  RenderBufferResource& RenderPassBuilder::addVertexBufferInput(const std::string& name) {
    LOG("Adding vertex buffer input '{}' to pass '{}'", name, this->name);
    return addGenericBufferInput(name, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
                                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
  }

  RenderBufferResource& RenderPassBuilder::addIndexBufferInput(const std::string& name) {
    LOG("Adding index buffer input '{}' to pass '{}'", name, this->name);
    return addGenericBufferInput(name, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_ACCESS_INDEX_READ_BIT, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
  }

  RenderBufferResource& RenderPassBuilder::addIndirectBufferInput(const std::string& name) {
    LOG("Adding indirect buffer input '{}' to pass '{}'", name, this->name);
    return addGenericBufferInput(name, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
                                 VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
  }

  RenderBufferResource& RenderPassBuilder::addUniformInput(const std::string& name, VkPipelineStageFlags2 stages) {
    LOG("Adding uniform buffer input '{}' to pass '{}'", name, this->name);
    if (stages == 0) {
      if (COMPUTE_QUEUES.has(queue)) {
        stages = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
      } else {
        stages = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
      }
    }
    return addGenericBufferInput(name, stages, VK_ACCESS_UNIFORM_READ_BIT, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
  }

  RenderBufferResource& RenderPassBuilder::addStorageReadOnlyInput(const std::string& name, VkPipelineStageFlags2 stages) {
    LOG("Adding storage buffer input '{}' to pass '{}'", name, this->name);
    if (stages == 0) {
      if (COMPUTE_QUEUES.has(queue)) {
        stages = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
      } else {
        stages = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
      }
    }
    return addGenericBufferInput(name, stages, VK_ACCESS_2_SHADER_STORAGE_READ_BIT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
  }

  RenderBufferResource& RenderPassBuilder::addStorageOutput(const std::string& name, const BufferInfo& info, const std::string& input) {
    LOG("Adding storage buffer output '{}' to pass '{}' from '{}'", name, this->name, input.empty() ? "nothing" : input.c_str());
    auto& res = graph.getBufferResource(name);
    res.setBufferInfo(info).addBufferUsage(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT).addQueue(queue).writtenInPass(id);
    storageOutputs.push_back(&res);

    if (!input.empty()) {
      auto& inputRes = graph.getBufferResource(input);
      VK_REQUIRE(inputRes.getBufferInfo().size == info.size, "Storage buffer input '{}' and output '{}' must have the same size. {} vs {}",
                 input, name, inputRes.getBufferInfo().size, info.size);
      inputRes.addBufferUsage(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT).addQueue(queue).readInPass(id);
      storageInputs.push_back(&inputRes);
    } else {
      storageInputs.push_back(nullptr);
    }

    return res;
  }

  RenderBufferResource& RenderPassBuilder::addTransferOutput(const std::string& name, const BufferInfo& info) {
    LOG("Adding transfer buffer output '{}' to pass '{}'", name, this->name);
    auto& res = graph.getBufferResource(name);
    res.setBufferInfo(info).addBufferUsage(VK_BUFFER_USAGE_TRANSFER_DST_BIT).addQueue(queue).writtenInPass(id);
    transferOutputs.push_back(&res);
    return res;
  }

  RenderTextureResource& RenderPassBuilder::addTextureInput(const std::string& name, VkPipelineStageFlags2 stages) {
    LOG("Adding texture input '{}' to pass '{}'", name, this->name);
    auto& res = graph.getTextureResource(name);
    res.addImageUsage(VK_IMAGE_USAGE_SAMPLED_BIT).addQueue(queue).readInPass(id);

    auto it = std::ranges::find_if(genericTexutre, [&](const AccessedTextureResource& acc) { return acc.texture == &res; });

    if (it != genericTexutre.end()) {
      return *it->texture;
    }

    AccessedTextureResource acc{.texture = &res};
    acc.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    acc.access = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;

    if (stages != 0) {
      acc.stages = stages;
    } else {
      if (COMPUTE_QUEUES.has(queue)) {
        acc.stages = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
      } else {
        acc.stages = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
      }
    }

    genericTexutre.push_back(acc);

    return res;
  }

  RenderTextureResource& RenderPassBuilder::addColorOutput(const std::string& name, const AttachmentInfo& info, const std::string& input) {
    LOG("Adding color output '{}' to pass '{}' from '{}'", name, this->name, input.empty() ? "nothing" : input.c_str());
    auto& res = graph.getTextureResource(name);
    res.setAttachmentInfo(info).addImageUsage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT).addQueue(queue).writtenInPass(id);

    if (info.mipLevels > 1)
      res.addImageUsage(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

    colorOutputs.push_back(&res);

    if (!input.empty()) {
      auto& inputRes = graph.getTextureResource(input);
      VK_REQUIRE(inputRes.getAttachmentInfo().format == info.format, "Color input '{}' and output '{}' must have the same format. {} vs {}",
                 input, name, inputRes.getAttachmentInfo().format, info.format);
      VK_REQUIRE(inputRes.getAttachmentInfo().mipLevels == info.mipLevels,
                 "Color input '{}' and output '{}' must have the same number of mip levels. {} vs {}", input, name,
                 inputRes.getAttachmentInfo().mipLevels, info.mipLevels);
      VK_REQUIRE(inputRes.getAttachmentInfo().samples == info.samples,
                 "Color input '{}' and output '{}' must have the same number of samples. {} vs {}", input, name,
                 inputRes.getAttachmentInfo().samples, info.samples);
      VK_REQUIRE(inputRes.getAttachmentInfo().layers == info.layers,
                 "Color input '{}' and output '{}' must have the same number of array layers. {} vs {}", input, name,
                 inputRes.getAttachmentInfo().layers, info.layers);
      VK_REQUIRE(inputRes.getAttachmentInfo().sizeType == info.sizeType,
                 "Color input '{}' and output '{}' must have the same size type. {} vs {}", input, name,
                 inputRes.getAttachmentInfo().sizeType, info.sizeType);
      VK_REQUIRE(inputRes.getAttachmentInfo().size == info.size,
                 "Color input '{}' and output '{}' must have the same size. ({}, {}, {}) vs ({}, {}, {})", input, name,
                 inputRes.getAttachmentInfo().size.x, inputRes.getAttachmentInfo().size.y, inputRes.getAttachmentInfo().size.z, info.size.x,
                 info.size.y, info.size.z);
      inputRes.addImageUsage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT).addQueue(queue).readInPass(id);
      colorInputs.push_back(&inputRes);
    } else {
      colorInputs.push_back(nullptr);
    }

    return res;
  }

  RenderTextureResource& RenderPassBuilder::addStorageImageOutput(const std::string& name, const AttachmentInfo& info,
                                                                  const std::string& input) {
    LOG("Adding storage image output '{}' to pass '{}' from '{}'", name, this->name, input.empty() ? "nothing" : input.c_str());
    auto& res = graph.getTextureResource(name);
    res.setAttachmentInfo(info).addImageUsage(VK_IMAGE_USAGE_STORAGE_BIT).addQueue(queue).writtenInPass(id);

    if (info.mipLevels > 1)
      res.addImageUsage(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

    storageImageOutputs.push_back(&res);

    if (!input.empty()) {
      auto& inputRes = graph.getTextureResource(input);
      VK_REQUIRE(inputRes.getAttachmentInfo().format == info.format,
                 "Storage image input '{}' and output '{}' must have the same format. {} vs {}", input, name,
                 inputRes.getAttachmentInfo().format, info.format);
      VK_REQUIRE(inputRes.getAttachmentInfo().mipLevels == info.mipLevels,
                 "Storage image input '{}' and output '{}' must have the same number of mip levels. {} vs {}", input, name,
                 inputRes.getAttachmentInfo().mipLevels, info.mipLevels);
      VK_REQUIRE(inputRes.getAttachmentInfo().samples == info.samples,
                 "Storage image input '{}' and output '{}' must have the same number of samples. {} vs {}", input, name,
                 inputRes.getAttachmentInfo().samples, info.samples);
      VK_REQUIRE(inputRes.getAttachmentInfo().layers == info.layers,
                 "Storage image input '{}' and output '{}' must have the same number of array layers. {} vs {}", input, name,
                 inputRes.getAttachmentInfo().layers, info.layers);
      VK_REQUIRE(inputRes.getAttachmentInfo().sizeType == info.sizeType,
                 "Storage image input '{}' and output '{}' must have the same size type. {} vs {}", input, name,
                 inputRes.getAttachmentInfo().sizeType, info.sizeType);
      VK_REQUIRE(inputRes.getAttachmentInfo().size == info.size,
                 "Storage image input '{}' and output '{}' must have the same size. ({}, {}, {}) vs ({}, {}, {})", input, name,
                 inputRes.getAttachmentInfo().size.x, inputRes.getAttachmentInfo().size.y, inputRes.getAttachmentInfo().size.z, info.size.x,
                 info.size.y, info.size.z);
      inputRes.addImageUsage(VK_IMAGE_USAGE_STORAGE_BIT).addQueue(queue).readInPass(id);
      storageImageInputs.push_back(&inputRes);
    } else {
      storageImageInputs.push_back(nullptr);
    }

    return res;
  }

  RenderTextureResource& RenderPassBuilder::setDepthStencilInput(const std::string& name) {
    LOG("Setting depth-stencil input '{}' for pass '{}'", name, this->name);
    auto& res = graph.getTextureResource(name);
    res.addImageUsage(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT).addQueue(queue).readInPass(id);
    depthStencilInput = &res;
    return res;
  }

  RenderTextureResource& RenderPassBuilder::setDepthStencilOutput(const std::string& name, const AttachmentInfo& info) {
    LOG("Setting depth-stencil output '{}' for pass '{}'", name, this->name);
    auto& res = graph.getTextureResource(name);
    res.setAttachmentInfo(info).addImageUsage(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT).addQueue(queue).writtenInPass(id);
    depthStencilOutput = &res;
    return res;
  }
} // namespace kt::vkh