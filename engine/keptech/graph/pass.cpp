#include "pass.hpp"

#include "builder.hpp"
#include "keptech/core/kt-logger.hpp"
#include "keptech/rhi/wrappers/buffer.hpp"
#include "passInterface.hpp"
#include <algorithm>

#ifdef KT_VULKAN
#include <volk.h>
#endif

namespace kt {
  using namespace rhi;

  static constexpr Bitflag<QueueType> COMPUTE_QUEUES = QueueType::Compute | QueueType::AsyncCompute;

#define LOG(...) KT_TRACE(__VA_ARGS__) // NOLINT

  RenderTextureResource& RenderPassBuilder::addHistoryInput(const std::string& n) {
    LOG("Adding history input '{}' to pass '{}'", name, this->name);
    auto& res = graph.getTextureResource(n);
    res.addImageUsage(ImageUsage::Sampled);
    historyInputs.push_back(&res);
    return res;
  }

  RenderBufferResource& RenderPassBuilder::addGenericBufferInput(const std::string& n, BufferUsage usage
#ifdef KT_VULKAN
                                                                 ,
                                                                 VkPipelineStageFlags2 stages, VkAccessFlags2 access
#endif
  ) {
    LOG("Adding generic buffer input '{}' to pass '{}'", name, this->name);
    auto& res = graph.getBufferResource(n);

    res.addBufferUsage(usage).addQueue(queue).readInPass(id);

    // Check if we already use this buffer. The same buffer may be used as multiple types of inputs in the same pass.
    auto it = std::ranges::find_if(genericBuffers, [&](const AccessedBufferResource& b) { return b.buffer->getId() == res.getId(); });
    if (it != genericBuffers.end()) {
#ifdef KT_VULKAN
      it->stages |= stages;
      it->access |= access;
#endif
      return res;
    }
    AccessedBufferResource acc{.buffer = &res};
    acc.layout = rhi::ImageLayout::General;
#ifdef KT_VULKAN
    acc.access = access;
    acc.stages = stages;
#endif

    genericBuffers.push_back(acc);

    return res;
  }

  RenderBufferResource& RenderPassBuilder::addVertexBufferInput(const std::string& n) {
    LOG("Adding vertex buffer input '{}' to pass '{}'", name, this->name);
    return addGenericBufferInput(n, rhi::BufferUsage::Vertex
#ifdef KT_VULKAN
                                 ,
                                 VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT
#endif
    );
  }

  RenderBufferResource& RenderPassBuilder::addIndexBufferInput(const std::string& n) {
    LOG("Adding index buffer input '{}' to pass '{}'", name, this->name);
    return addGenericBufferInput(n, rhi::BufferUsage::Index
#ifdef KT_VULKAN
                                 ,
                                 VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_ACCESS_INDEX_READ_BIT
#endif
    );
  }

  RenderBufferResource& RenderPassBuilder::addIndirectBufferInput(const std::string& n) {
    LOG("Adding indirect buffer input '{}' to pass '{}'", name, this->name);
    return addGenericBufferInput(n, rhi::BufferUsage::Indirect
#ifdef KT_VULKAN
                                 ,
                                 VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT
#endif
    );
  }
  void RenderPassBuilder::setupDependencies() {
    if (passInterface)
      passInterface->setupDependencies(*this, graph);
  }
  RenderPassBuilder& RenderPassBuilder::setName(const std::string& n) {
    name = n;
    return *this;
  }

  RenderBufferResource& RenderPassBuilder::addMappedBuffer(const std::string& n, size_t size) {
    LOG("Adding mapped buffer input '{}' to pass '{}'", name, this->name);
    auto& res = graph.getBufferResource(n);

    res.setBufferInfo({.size = size, .type = BufferType::GpuMapped}).writtenInPass(id);

    mappedBuffers.push_back(&res);

    return res;
  }

  RenderBufferResource& RenderPassBuilder::addStagingBuffer(const std::string& n, size_t size) {
    LOG("Adding staging buffer input '{}' to pass '{}'", name, this->name);
    auto& res = graph.getBufferResource(n);

    res.setBufferInfo({.size = size, .type = BufferType::Staging}).addBufferUsage(rhi::BufferUsage::TransferSrc).writtenInPass(id);

    mappedBuffers.push_back(&res);

    return res;
  }

  [[nodiscard]] std::string& RenderPassBuilder::getName() { return name; }
  [[nodiscard]] const std::string& RenderPassBuilder::getName() const { return name; }
  [[nodiscard]] PassId RenderPassBuilder::getId() const { return id; }
  [[nodiscard]] QueueType RenderPassBuilder::getQueue() const { return queue; }
  [[nodiscard]] RenderPassInterface* RenderPassBuilder::getInterface() const { return passInterface; }
  RenderPassBuilder& RenderPassBuilder::setInterface(RenderPassInterface* i) {
    passInterface = i;
    return *this;
  }
  RenderPassBuilder& RenderPassBuilder::setBuildCallback(PassExecuteCb cb) {
    this->buildCb = std::move(cb);
    return *this;
  }
  [[nodiscard]] PassExecuteCb& RenderPassBuilder::getBuildCallback() { return buildCb; }
  RenderPassBuilder& RenderPassBuilder::setGetClearDepthStencilCallback(std::function<bool(DepthClearValue*)> cb) {
    this->getClearDepthStencilCb = std::move(cb);
    return *this;
  }
  [[nodiscard]] std::function<bool(DepthClearValue*)>& RenderPassBuilder::getGetClearDepthStencilCallback() {
    return getClearDepthStencilCb;
  }
  RenderPassBuilder& RenderPassBuilder::setGetClearColorCallback(std::function<bool(unsigned, ColorClearValue*)> cb) {
    this->getClearColorCb = std::move(cb);
    return *this;
  }
  [[nodiscard]] std::function<bool(unsigned, ColorClearValue*)>& RenderPassBuilder::getGetClearColorCallback() { return getClearColorCb; }

  bool RenderPassBuilder::getClearColor(uint32_t attachmentIndex, ColorClearValue* value) const {
    if (passInterface)
      return passInterface->getClearColor(attachmentIndex, value);
    else if (getClearColorCb)
      return getClearColorCb(attachmentIndex, value);

    return false;
  }
  bool RenderPassBuilder::getClearDepthStencil(DepthClearValue* value) const {
    if (passInterface)
      return passInterface->getClearDepthStencil(value);
    else if (getClearDepthStencilCb)
      return getClearDepthStencilCb(value);

    return false;
  }
  [[nodiscard]] const std::vector<RenderTextureResource*>& RenderPassBuilder::getColorOutputs() const { return colorOutputs; }
  [[nodiscard]] const std::vector<RenderTextureResource*>& RenderPassBuilder::getColorInputs() const { return colorInputs; }
  [[nodiscard]] const std::vector<RenderTextureResource*>& RenderPassBuilder::getHistoryInputs() const { return historyInputs; }
  [[nodiscard]] const std::vector<RenderTextureResource*>& RenderPassBuilder::getStorageImageOutputs() const { return storageImageOutputs; }
  [[nodiscard]] const std::vector<RenderTextureResource*>& RenderPassBuilder::getStorageImageInputs() const { return storageImageInputs; }
  [[nodiscard]] const std::vector<RenderBufferResource*>& RenderPassBuilder::getStorageOutputs() const { return storageOutputs; }
  [[nodiscard]] const std::vector<RenderBufferResource*>& RenderPassBuilder::getStorageInputs() const { return storageInputs; }
  [[nodiscard]] const std::vector<RenderBufferResource*>& RenderPassBuilder::getTransferOutputs() const { return transferOutputs; }
  const std::vector<RenderBufferResource*>& RenderPassBuilder::getMappedBuffers() const { return mappedBuffers; }
  [[nodiscard]] RenderTextureResource* RenderPassBuilder::getDepthStencilInput() const { return depthStencilInput; }
  [[nodiscard]] RenderTextureResource* RenderPassBuilder::getDepthStencilOutput() const { return depthStencilOutput; }
  [[nodiscard]] const std::vector<AccessedTextureResource>& RenderPassBuilder::getGenericTextureInputs() const { return genericTexutre; }
  [[nodiscard]] const std::vector<AccessedBufferResource>& RenderPassBuilder::getGenericBufferInputs() const { return genericBuffers; }
  const std::unordered_set<PassId>& RenderPassBuilder::getProxyPasses() const { return proxyPasses; }

  RenderPassBuilder& RenderPassBuilder::setIndex(size_t idx) {
    this->index = idx;
    return *this;
  }
  [[nodiscard]] size_t RenderPassBuilder::getIndex() const { return index; }
  void RenderPassBuilder::setDepthStencilLayout(ImageLayout layout) { depthStencilLayout = layout; }
  [[nodiscard]] ImageLayout RenderPassBuilder::getDepthStencilLayout() const { return depthStencilLayout; }

  RenderBufferResource& RenderPassBuilder::addUniformInput(const std::string& n
#ifdef KT_VULKAN
                                                           ,
                                                           VkPipelineStageFlags2 stages
#endif
  ) {
    KT_ASSERT(queue != QueueType::Cpu, "Uniform buffer inputs can only be used in graphics or compute passes");
    LOG("Adding uniform buffer input '{}' to pass '{}'", name, this->name);
#ifdef KT_VULKAN
    if (stages == 0) {
      if (COMPUTE_QUEUES.has(queue)) {
        stages = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
      } else {
        stages = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
      }
    }
    return addGenericBufferInput(n, rhi::BufferUsage::Uniform, stages, VK_ACCESS_UNIFORM_READ_BIT);
#else
    return addGenericBufferInput(n, rhi::BufferUsage::Uniform);
#endif
  } // namespace kt::rhi

  RenderBufferResource& RenderPassBuilder::addStorageReadOnlyInput(const std::string& n
#ifdef KT_VULKAN
                                                                   ,
                                                                   VkPipelineStageFlags2 stages
#endif
  ) {
    KT_ASSERT(queue != QueueType::Cpu, "Storage buffer inputs can only be used in graphics or compute passes");
    LOG("Adding storage buffer input '{}' to pass '{}'", name, this->name);
#ifdef KT_VULKAN
    if (stages == 0) {
      if (COMPUTE_QUEUES.has(queue)) {
        stages = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
      } else {
        stages = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
      }
    }
    return addGenericBufferInput(n, rhi::BufferUsage::Storage, stages, VK_ACCESS_2_SHADER_STORAGE_READ_BIT);
#else
    return addGenericBufferInput(n, rhi::BufferUsage::Storage);
#endif
  }

  RenderBufferResource& RenderPassBuilder::addStorageOutput(const std::string& n, const BufferInfo& info, const std::string& input) {
    LOG("Adding storage buffer output '{}' to pass '{}' from '{}'", name, this->name, input.empty() ? "nothing" : input.c_str());
    auto& res = graph.getBufferResource(n);
    res.setBufferInfo(info).addBufferUsage(rhi::BufferUsage::Storage).addQueue(queue).writtenInPass(id);
    storageOutputs.push_back(&res);

    if (!input.empty()) {
      auto& inputRes = graph.getBufferResource(input);
      inputRes.addBufferUsage(rhi::BufferUsage::Storage).addQueue(queue).readInPass(id);
      storageInputs.push_back(&inputRes);
    } else {
      storageInputs.push_back(nullptr);
    }

    return res;
  }

  RenderBufferResource& RenderPassBuilder::addTransferOutput(const std::string& n, const BufferInfo& info) {
    LOG("Adding transfer buffer output '{}' to pass '{}'", name, this->name);
    auto& res = graph.getBufferResource(n);
    res.setBufferInfo(info).addBufferUsage(rhi::BufferUsage::TransferDst).addQueue(queue).writtenInPass(id);
    transferOutputs.push_back(&res);
    return res;
  }

  RenderTextureResource& RenderPassBuilder::addTextureInput(const std::string& n
#ifdef KT_VULKAN
                                                            ,
                                                            VkPipelineStageFlags2 stages
#endif
  ) {
    KT_ASSERT(queue != QueueType::Cpu, "Texture inputs can only be used in graphics or compute passes");
    LOG("Adding texture input '{}' to pass '{}'", name, this->name);
    auto& res = graph.getTextureResource(n);
    res.addImageUsage(rhi::ImageUsage::Sampled).addQueue(queue).readInPass(id);

    auto it = std::ranges::find_if(genericTexutre, [&](const AccessedTextureResource& acc) { return acc.texture == &res; });

    if (it != genericTexutre.end()) {
      return *it->texture;
    }

    AccessedTextureResource acc{.texture = &res};
    acc.layout = rhi::ImageLayout::ShaderReadOnly;
#ifdef KT_VULKAN
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
#endif

    genericTexutre.push_back(acc);

    return res;
  }

  RenderTextureResource& RenderPassBuilder::addColorOutput(const std::string& n, const AttachmentInfo& info, const std::string& input) {
    KT_ASSERT(queue == QueueType::Graphics, "Color outputs can only be used in graphics passes");
    LOG("Adding color output '{}' to pass '{}' from '{}'", name, this->name, input.empty() ? "nothing" : input.c_str());
    auto& res = graph.getTextureResource(n);
    res.setAttachmentInfo(info).addImageUsage(rhi::ImageUsage::RenderTarget).addQueue(queue).writtenInPass(id);

    if (info.mipLevels > 1)
      res.addImageUsage(rhi::ImageUsage::TransferDst).addImageUsage(rhi::ImageUsage::TransferSrc);

    colorOutputs.push_back(&res);

    if (!input.empty()) {
      auto& inputRes = graph.getTextureResource(input);
      inputRes.addImageUsage(rhi::ImageUsage::RenderTarget).addQueue(queue).readInPass(id);
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
    res.setAttachmentInfo(info).addImageUsage(rhi::ImageUsage::Storage).addQueue(queue).writtenInPass(id);

    if (info.mipLevels > 1)
      res.addImageUsage(rhi::ImageUsage::TransferDst).addImageUsage(rhi::ImageUsage::TransferSrc);

    storageImageOutputs.push_back(&res);

    if (!input.empty()) {
      auto& inputRes = graph.getTextureResource(input);
      inputRes.addImageUsage(rhi::ImageUsage::Storage).addQueue(queue).readInPass(id);
      storageImageInputs.push_back(&inputRes);
    } else {
      storageImageInputs.push_back(nullptr);
    }

    return res;
  }

  RenderTextureResource& RenderPassBuilder::setDepthStencilInput(const std::string& n) {
    KT_ASSERT(queue == QueueType::Graphics, "Depth-stencil inputs can only be used in graphics passes");
    LOG("Setting depth-stencil input '{}' for pass '{}'", name, this->name);
    auto& res = graph.getTextureResource(n);
    res.addImageUsage(rhi::ImageUsage::DepthStencil).addImageUsage(rhi::ImageUsage::Sampled).addQueue(queue).readInPass(id);
    depthStencilInput = &res;
    return res;
  }

  RenderTextureResource& RenderPassBuilder::setDepthStencilOutput(const std::string& n, const AttachmentInfo& info) {
    KT_ASSERT(queue == QueueType::Graphics, "Depth-stencil outputs can only be used in graphics passes");
    LOG("Setting depth-stencil output '{}' for pass '{}'", name, this->name);
    auto& res = graph.getTextureResource(n);
    res.setAttachmentInfo(info).addImageUsage(rhi::ImageUsage::DepthStencil).addQueue(queue).writtenInPass(id);
    depthStencilOutput = &res;
    return res;
  }

  RenderPassBuilder& RenderPassBuilder::addProxyPass(const std::string& n, QueueType q, bool abr) {
    LOG("Adding proxy pass '{}' to pass '{}'", n, this->name);
    auto* proxyPass = graph.findPass(n);

    if (!proxyPass) {
      KT_TRACE("Proxy pass '{}' not found, creating a new one", n);
      proxyPass = &graph.addPass(n, q, abr);
    }

    proxyPasses.insert(proxyPass->getId());
    return *proxyPass;
  }

  bool RenderPassBuilder::getAutoBeingRendering() const { return autoBeginRendering; }
} // namespace kt