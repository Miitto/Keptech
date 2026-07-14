#pragma once

#include "keptech/rendering/interface.hpp"
#include <glm/vec3.hpp>
#include <optional>
#include <string>

namespace kt::render_graph {
  struct AttachmentInfo {
    std::string name;
    rendering::ImageLayout desiredLayout;
    rendering::Image image;
  };

  enum class PassType : uint8_t {
    Graphics,
    Compute,
    AsyncCompute,
  };

  struct PassId {
    PassId(size_t idx = ~0u) : index(idx) {}
    operator size_t() const { return index; }
    operator bool() const { return index != ~0u; }
    bool operator==(const PassId& other) const { return index == other.index; }
    size_t index;
  };

  struct Pass {
    std::string name;
    PassType type;

    std::vector<std::string> inputs;
    std::vector<std::string> outputs;
  };

  struct SyncInfo {
    size_t waitOn;
    bool signal;
    std::vector<rendering::Image::TransitionType> transitions;
    std::vector<rendering::Image::TransitionType> sendOffs;
  };

  class RenderGraph {
  public:
    void addAttachment(const AttachmentInfo& attachment);
    PassId addPass(const Pass& pass);

    [[nodiscard]] const AttachmentInfo& getAttachment(const std::string& name) const;
    [[nodiscard]] const Pass& getPass(PassId id) const { return passes[id.index]; }

    [[nodiscard]] const std::unordered_map<std::string, AttachmentInfo>& getAttachments() const;
    [[nodiscard]] const std::vector<Pass>& getPasses() const;
    [[nodiscard]] const std::vector<PassId>& getExecutionOrder() const;

    void compile();

  protected:
    struct PassTypeCounts {
      size_t graphicsCount = 0;
      size_t computeCount = 0;
      size_t asyncComputeCount = 0;
    };

    std::unordered_map<std::string, AttachmentInfo> attachments;
    std::vector<Pass> passes;
    std::vector<PassId> executionOrder;
    std::vector<SyncInfo> syncInfos;
    PassTypeCounts passTypeCounts;
  };
} // namespace kt::render_graph