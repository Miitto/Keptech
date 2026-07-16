#pragma once

#include "keptech/vulkan/wrappers/buffer.hpp"
#include "keptech/vulkan/wrappers/image.hpp"
#include "pass.hpp"
#include <glm/ext/vector_uint2.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace kt::vkh {
  class Renderer;
  class RenderGraphBuilder;
  class RenderGraph;

  enum class LoadOp : uint8_t {
    Load,
    Clear,
    DontCare,
  };

  enum class StoreOp : uint8_t {
    Store,
    DontCare,
  };

  struct RenderAttachment {
    PhysResourceId resourceId{};
    VkAttachmentLoadOp loadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    VkAttachmentStoreOp storeOp = VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_DONT_CARE;

    operator bool() const { return resourceId.used(); }
  };

  class RenderPass {
  public:
    friend class RenderGraphBuilder;

    void setGraph(RenderGraph& graph) { this->graph = &graph; }
    void setInterface(RenderPassInterface* interface) { this->interface = interface; }
    void setBuildCallback(std::function<void(CommandBuffer&)>&& cb) { buildCb = std::move(cb); }
    void setGetClearDepthStencilCallback(std::function<bool(VkClearDepthStencilValue*)>&& cb) { getClearDepthStencilCb = std::move(cb); }
    void setGetClearColorCallback(std::function<bool(unsigned, VkClearColorValue*)>&& cb) { getClearColorCb = std::move(cb); }

    void setup(RenderGraph& graph, const Renderer& renderer) {
      if (interface)
        interface->setup(renderer);
    }

    void prepare(RenderGraph& graph) {
      if (interface)
        interface->prepare(graph);
    }

    void execute(CommandBuffer& cmd) {
      if (interface) {
        interface->execute(cmd);
      } else if (buildCb) {
        buildCb(cmd);
      }
    }

    bool getClearColor(size_t attachmentIndex, VkClearColorValue* value = nullptr) const {
      if (interface)
        return interface->getClearColor(attachmentIndex, value);
      else if (getClearColorCb)
        return getClearColorCb(attachmentIndex, value);

      return false;
    }

    bool getClearDepthStencil(VkClearDepthStencilValue* value = nullptr) const {
      if (interface)
        return interface->getClearDepthStencil(value);
      else if (getClearDepthStencilCb)
        return getClearDepthStencilCb(value);

      return false;
    }

    const Barriers& getBarriers() const { return barriers; }
    const std::string& getName() const { return name; }

    const std::vector<RenderAttachment>& getColorAttachments() const { return colorAttachments; }
    const RenderAttachment& getDepthStencilAttachment() const { return depthStencilAttachment; }

    void setExtentSourceId(PhysResourceId id) { extentSourceId = id; }
    PhysResourceId getExtentSourceId() const { return extentSourceId; }

  private:
    RenderPass(std::string&& name, QueueType queue, Barriers&& barriers, std::vector<RenderAttachment>&& colorAttachments,
               RenderAttachment depthStencilAttachment)
        : name(std::move(name)), queue(queue), barriers(std::move(barriers)), colorAttachments(std::move(colorAttachments)),
          depthStencilAttachment(depthStencilAttachment) {}

    RenderGraph* graph = nullptr;
    std::string name;
    QueueType queue;

    PhysResourceId extentSourceId{};

    RenderPassInterface* interface = nullptr;
    std::function<void(CommandBuffer&)> buildCb = nullptr;
    std::function<bool(VkClearDepthStencilValue*)> getClearDepthStencilCb = nullptr;
    std::function<bool(unsigned, VkClearColorValue*)> getClearColorCb = nullptr;

    Barriers barriers;
    std::vector<RenderAttachment> colorAttachments;
    RenderAttachment depthStencilAttachment;
  };

  struct RelativeImage {
    size_t index;
    glm::vec3 ratio;
  };

  class RenderGraph {
  public:
    // Friend so it is the only thing that can construct a RenderGraph.
    friend class RenderGraphBuilder;

    void execute();

    void setRenderer(Renderer& renderer) { this->renderer = &renderer; }

    [[nodiscard]] const std::vector<RenderPass>& getPasses() const { return passes; }
    [[nodiscard]] const std::vector<bool>& getPhysicalImageHasHistory() const { return physicalImageHasHistory; }

    /// Get the index of the image resource with the given name. Throws if the resource does not exist.
    /// The returned index is safe to store as it will remain constant even if the image is resized. Use getImage() to get the image at the
    /// index.
    [[nodiscard]] size_t getImageIndex(const std::string& name) const {
      auto it = nameToImage.find(name);
      VK_REQUIRE(it != nameToImage.end(), "Image resource with name '{}' not found in render graph", name);
      return it->second;
    }

    /// Get the index of the buffer resource with the given name. Throws if the resource does not exist.
    /// The returned index is safe to store as it will remain constant even if the buffer is resized. Use getBuffer() to get the buffer at
    /// the index.
    [[nodiscard]] size_t getBufferIndex(const std::string& name) const {
      auto it = nameToBuffer.find(name);
      VK_REQUIRE(it != nameToBuffer.end(), "Buffer resource with name '{}' not found in render graph", name);
      return it->second;
    }

    /// Get the image at the given index. Do not store a reference to the image as it may become invalid if the image is resized. Use the
    /// index to get the image again if needed.
    [[nodiscard]] const Image& getImage(size_t index) const {
      VK_REQUIRE(index < images.size(), "Image index {} is out of bounds (size: {})", index, images.size());
      return images[index];
    }

    /// Get the buffer at the given index. Do not store a reference to the buffer as it may become invalid if the buffer is resized. Use the
    /// index to get the buffer again if needed.
    [[nodiscard]] const Buffer& getBuffer(size_t index) const {
      VK_REQUIRE(index < buffers.size(), "Buffer index {} is out of bounds (size: {})", index, buffers.size());
      return buffers[index];
    }

    void destroy();

    void setBackbufferSource(const std::string& name) {
      auto it = nameToImage.find(name);
      VK_REQUIRE(it != nameToImage.end(), "Backbuffer source '{}' not found in render graph", name);
      backbufferSourceIndex = it->second;
      VK_DEBUG("Backbuffer source set to '{}' (index {})", name, backbufferSourceIndex);
    }

    [[nodiscard]] const Image& getBackbufferImage() const {
      VK_REQUIRE(backbufferSourceIndex < images.size(), "Backbuffer source index {} is out of bounds (size: {})", backbufferSourceIndex,
                 images.size());
      return images[backbufferSourceIndex];
    }

  private:
    RenderGraph(Renderer& renderer, std::vector<RenderPass>&& passes, std::vector<Image>&& images, std::vector<Buffer>&& buffers,
                std::unordered_map<std::string, size_t>&& nameToImage, std::unordered_map<std::string, size_t>&& nameToBuffer,
                std::vector<bool>&& physicalImageHasHistory, std::vector<RelativeImage>&& swapchainRelativeImages,
                std::vector<RelativeImage>&& resolutionRelativeImages)
        : renderer(&renderer), passes(std::move(passes)), images(std::move(images)), buffers(std::move(buffers)),
          nameToImage(std::move(nameToImage)), nameToBuffer(std::move(nameToBuffer)),
          physicalImageHasHistory(std::move(physicalImageHasHistory)), swapchainRelativeImages(std::move(swapchainRelativeImages)),
          resolutionRelativeImages(std::move(resolutionRelativeImages)) {}

    Renderer* renderer;

    std::vector<RenderPass> passes;

    size_t backbufferSourceIndex = 0;

    std::vector<Image> images;
    std::vector<Buffer> buffers;
    std::unordered_map<std::string, size_t> nameToImage;
    std::unordered_map<std::string, size_t> nameToBuffer;
    std::vector<bool> physicalImageHasHistory;
    std::vector<RelativeImage> swapchainRelativeImages;
    std::vector<RelativeImage> resolutionRelativeImages;
  };
} // namespace kt::vkh