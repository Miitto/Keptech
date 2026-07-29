#include "pass.hpp"

#include "builder.hpp"
#include "passInterface.hpp"
#include "vk-logger.hpp"
#include <algorithm>

namespace kt::rdr {
  static constexpr Bitflag<QueueType> COMPUTE_QUEUES = QueueType::Compute | QueueType::AsyncCompute;

#define LOG(...) VK_TRACE(__VA_ARGS__) // NOLINT

  RenderTextureResource& RenderPassBuilder::addAttachmentInput(const std::string& n) {
    LOG("Adding attachment input '{}' to pass '{}'", name, this->name);
    auto& res = graph.getTextureResource(n);
    res.addImageUsage(VK_IMAGE_USAGE_SAMPLED_BIT).addQueue(queue).readInPass(id);
    attachmentInputs.push_back(&res);
    return res;
  }

  RenderTextureResource& RenderPassBuilder::addHistoryInput(const std::string& n) {
    LOG("Adding history input '{}' to pass '{}'", name, this->name);
    auto& res = graph.getTextureResource(n);
    res.addImageUsage(VK_IMAGE_USAGE_SAMPLED_BIT).addQueue(queue);
    historyInputs.push_back(&res);
    return res;
  }

  RenderBufferResource& RenderPassBuilder::addGenericBufferInput(const std::string& n, VkPipelineStageFlags2 stages, VkAccessFlags2 access,
                                                                 VkBufferUsageFlags usage) {
    LOG("Adding generic buffer input '{}' to pass '{}'", name, this->name);
    auto& res = graph.getBufferResource(n);

    res.addBufferUsage(usage).addQueue(queue).readInPass(id);

    AccessedBufferResource acc{.buffer = &res};
    acc.layout = VK_IMAGE_LAYOUT_GENERAL;
    acc.access = access;
    acc.stages = stages;

    genericBuffers.push_back(acc);

    return res;
  }

  RenderBufferResource& RenderPassBuilder::addVertexBufferInput(const std::string& n) {
    LOG("Adding vertex buffer input '{}' to pass '{}'", name, this->name);
    return addGenericBufferInput(n, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
                                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
  }

  RenderBufferResource& RenderPassBuilder::addIndexBufferInput(const std::string& n) {
    LOG("Adding index buffer input '{}' to pass '{}'", name, this->name);
    return addGenericBufferInput(n, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_ACCESS_INDEX_READ_BIT, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
  }

  RenderBufferResource& RenderPassBuilder::addIndirectBufferInput(const std::string& n) {
    LOG("Adding indirect buffer input '{}' to pass '{}'", name, this->name);
    return addGenericBufferInput(n, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
                                 VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
  }
  void RenderPassBuilder::setupDependencies(const Renderer& renderer) {
    if (interface)
      interface->setupDependencies(*this, graph, renderer);
  }
  RenderPassBuilder& RenderPassBuilder::setName(const std::string& n) {
    name = n;
    return *this;
  }

  [[nodiscard]] std::string& RenderPassBuilder::getName() { return name; }
  [[nodiscard]] const std::string& RenderPassBuilder::getName() const { return name; }
  [[nodiscard]] PassId RenderPassBuilder::getId() const { return id; }
  [[nodiscard]] QueueType RenderPassBuilder::getQueue() const { return queue; }
  [[nodiscard]] RenderPassInterface* RenderPassBuilder::getInterface() const { return interface; }
  RenderPassBuilder& RenderPassBuilder::setInterface(RenderPassInterface* i) {
    interface = i;
    return *this;
  }
  RenderPassBuilder& RenderPassBuilder::setBuildCallback(PassExecuteCb cb) {
    this->buildCb = std::move(cb);
    return *this;
  }
  [[nodiscard]] PassExecuteCb& RenderPassBuilder::getBuildCallback() { return buildCb; }
  RenderPassBuilder& RenderPassBuilder::setGetClearDepthStencilCallback(std::function<bool(VkClearDepthStencilValue*)> cb) {
    this->getClearDepthStencilCb = std::move(cb);
    return *this;
  }
  [[nodiscard]] std::function<bool(VkClearDepthStencilValue*)>& RenderPassBuilder::getGetClearDepthStencilCallback() {
    return getClearDepthStencilCb;
  }
  RenderPassBuilder& RenderPassBuilder::setGetClearColorCallback(std::function<bool(unsigned, VkClearColorValue*)> cb) {
    this->getClearColorCb = std::move(cb);
    return *this;
  }
  [[nodiscard]] std::function<bool(unsigned, VkClearColorValue*)>& RenderPassBuilder::getGetClearColorCallback() { return getClearColorCb; }

  bool RenderPassBuilder::getClearColor(uint32_t attachmentIndex, VkClearColorValue* value) const {
    if (interface)
      return interface->getClearColor(attachmentIndex, value);
    else if (getClearColorCb)
      return getClearColorCb(attachmentIndex, value);

    return false;
  }
  bool RenderPassBuilder::getClearDepthStencil(VkClearDepthStencilValue* value) const {
    if (interface)
      return interface->getClearDepthStencil(value);
    else if (getClearDepthStencilCb)
      return getClearDepthStencilCb(value);

    return false;
  }
  [[nodiscard]] const std::vector<RenderTextureResource*>& RenderPassBuilder::getColorOutputs() const { return colorOutputs; }
  [[nodiscard]] const std::vector<RenderTextureResource*>& RenderPassBuilder::getResolveOutputs() const { return resolveOutputs; }
  [[nodiscard]] const std::vector<RenderTextureResource*>& RenderPassBuilder::getColorInputs() const { return colorInputs; }
  [[nodiscard]] const std::vector<RenderTextureResource*>& RenderPassBuilder::getHistoryInputs() const { return historyInputs; }
  [[nodiscard]] const std::vector<RenderTextureResource*>& RenderPassBuilder::getAttachmentInputs() const { return attachmentInputs; }
  [[nodiscard]] const std::vector<RenderTextureResource*>& RenderPassBuilder::getStorageImageOutputs() const { return storageImageOutputs; }
  [[nodiscard]] const std::vector<RenderTextureResource*>& RenderPassBuilder::getStorageImageInputs() const { return storageImageInputs; }
  [[nodiscard]] const std::vector<RenderBufferResource*>& RenderPassBuilder::getStorageOutputs() const { return storageOutputs; }
  [[nodiscard]] const std::vector<RenderBufferResource*>& RenderPassBuilder::getStorageInputs() const { return storageInputs; }
  [[nodiscard]] const std::vector<RenderBufferResource*>& RenderPassBuilder::getTransferOutputs() const { return transferOutputs; }
  [[nodiscard]] RenderTextureResource* RenderPassBuilder::getDepthStencilInput() const { return depthStencilInput; }
  [[nodiscard]] RenderTextureResource* RenderPassBuilder::getDepthStencilOutput() const { return depthStencilOutput; }
  [[nodiscard]] const std::vector<AccessedTextureResource>& RenderPassBuilder::getGenericTextureInputs() const { return genericTexutre; }
  [[nodiscard]] const std::vector<AccessedBufferResource>& RenderPassBuilder::getGenericBufferInputs() const { return genericBuffers; }

  RenderPassBuilder& RenderPassBuilder::setIndex(size_t idx) {
    this->index = idx;
    return *this;
  }
  [[nodiscard]] size_t RenderPassBuilder::getIndex() const { return index; }
  void RenderPassBuilder::setDepthStencilLayout(VkImageLayout layout) { depthStencilLayout = layout; }
  [[nodiscard]] VkImageLayout RenderPassBuilder::getDepthStencilLayout() const { return depthStencilLayout; }

  RenderBufferResource& RenderPassBuilder::addUniformInput(const std::string& n, VkPipelineStageFlags2 stages) {
    LOG("Adding uniform buffer input '{}' to pass '{}'", name, this->name);
    if (stages == 0) {
      if (COMPUTE_QUEUES.has(queue)) {
        stages = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
      } else {
        stages = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
      }
    }
    return addGenericBufferInput(n, stages, VK_ACCESS_UNIFORM_READ_BIT, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
  }

  RenderBufferResource& RenderPassBuilder::addStorageReadOnlyInput(const std::string& n, VkPipelineStageFlags2 stages) {
    LOG("Adding storage buffer input '{}' to pass '{}'", name, this->name);
    if (stages == 0) {
      if (COMPUTE_QUEUES.has(queue)) {
        stages = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
      } else {
        stages = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
      }
    }
    return addGenericBufferInput(n, stages, VK_ACCESS_2_SHADER_STORAGE_READ_BIT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
  }

  RenderBufferResource& RenderPassBuilder::addStorageOutput(const std::string& n, const BufferInfo& info, const std::string& input) {
    LOG("Adding storage buffer output '{}' to pass '{}' from '{}'", name, this->name, input.empty() ? "nothing" : input.c_str());
    auto& res = graph.getBufferResource(n);
    res.setBufferInfo(info).addBufferUsage(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT).addQueue(queue).writtenInPass(id);
    storageOutputs.push_back(&res);

    if (!input.empty()) {
      auto& inputRes = graph.getBufferResource(input);
      inputRes.addBufferUsage(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT).addQueue(queue).readInPass(id);
      storageInputs.push_back(&inputRes);
    } else {
      storageInputs.push_back(nullptr);
    }

    return res;
  }

  RenderBufferResource& RenderPassBuilder::addTransferOutput(const std::string& n, const BufferInfo& info) {
    LOG("Adding transfer buffer output '{}' to pass '{}'", name, this->name);
    auto& res = graph.getBufferResource(n);
    res.setBufferInfo(info).addBufferUsage(VK_BUFFER_USAGE_TRANSFER_DST_BIT).addQueue(queue).writtenInPass(id);
    transferOutputs.push_back(&res);
    return res;
  }

  RenderTextureResource& RenderPassBuilder::addTextureInput(const std::string& n, VkPipelineStageFlags2 stages) {
    LOG("Adding texture input '{}' to pass '{}'", name, this->name);
    auto& res = graph.getTextureResource(n);
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

  RenderTextureResource& RenderPassBuilder::addColorOutput(const std::string& n, const AttachmentInfo& info, const std::string& input) {
    LOG("Adding color output '{}' to pass '{}' from '{}'", name, this->name, input.empty() ? "nothing" : input.c_str());
    auto& res = graph.getTextureResource(n);
    res.setAttachmentInfo(info).addImageUsage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT).addQueue(queue).writtenInPass(id);

    if (info.mipLevels > 1)
      res.addImageUsage(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

    colorOutputs.push_back(&res);

    if (!input.empty()) {
      auto& inputRes = graph.getTextureResource(input);
      inputRes.addImageUsage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT).addQueue(queue).readInPass(id);
      colorInputs.push_back(&inputRes);
    } else {
      colorInputs.push_back(nullptr);
    }

    return res;
  }

  RenderTextureResource& RenderPassBuilder::addStorageImageOutput(const std::string& n, const AttachmentInfo& info,
                                                                  const std::string& input) {
    LOG("Adding storage image output '{}' to pass '{}' from '{}'", name, this->name, input.empty() ? "nothing" : input.c_str());
    auto& res = graph.getTextureResource(n);
    res.setAttachmentInfo(info).addImageUsage(VK_IMAGE_USAGE_STORAGE_BIT).addQueue(queue).writtenInPass(id);

    if (info.mipLevels > 1)
      res.addImageUsage(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

    storageImageOutputs.push_back(&res);

    if (!input.empty()) {
      auto& inputRes = graph.getTextureResource(input);
      inputRes.addImageUsage(VK_IMAGE_USAGE_STORAGE_BIT).addQueue(queue).readInPass(id);
      storageImageInputs.push_back(&inputRes);
    } else {
      storageImageInputs.push_back(nullptr);
    }

    return res;
  }

  RenderTextureResource& RenderPassBuilder::setDepthStencilInput(const std::string& n) {
    LOG("Setting depth-stencil input '{}' for pass '{}'", name, this->name);
    auto& res = graph.getTextureResource(n);
    res.addImageUsage(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT).addQueue(queue).readInPass(id);
    depthStencilInput = &res;
    return res;
  }

  RenderTextureResource& RenderPassBuilder::setDepthStencilOutput(const std::string& n, const AttachmentInfo& info) {
    LOG("Setting depth-stencil output '{}' for pass '{}'", name, this->name);
    auto& res = graph.getTextureResource(n);
    res.setAttachmentInfo(info).addImageUsage(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT).addQueue(queue).writtenInPass(id);
    depthStencilOutput = &res;
    return res;
  }
} // namespace kt::rdr