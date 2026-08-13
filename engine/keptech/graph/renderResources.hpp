#pragma once

#include "keptech/core/bitflag.hpp"
#include "keptech/rhi/bufferUsage.hpp"
#include "types.hpp"
#include <cstdint>
#include <string>
#include <unordered_set>

namespace kt {
  class RenderResource {
  public:
    enum class Type : uint8_t {
      Texture,
      Buffer,
    };

    RenderResource(Type type, ResourceId id) : _type(type), _id(id) {}

    RenderResource& addQueue(Bitflag<QueueType> queue) {
      usedQueues |= queue;
      return *this;
    }
    [[nodiscard]] Bitflag<QueueType> getUsedQueues() const { return usedQueues; }
    RenderResource& readInPass(PassId pass) {
      readInPasses.insert(pass);
      return *this;
    }
    [[nodiscard]] const std::unordered_set<PassId>& getReadPasses() const { return readInPasses; }
    RenderResource& writtenInPass(PassId pass) {
      writtenInPasses.insert(pass);
      return *this;
    }
    [[nodiscard]] const std::unordered_set<PassId>& getWritePasses() const { return writtenInPasses; }

    [[nodiscard]] Type getType() const { return _type; }
    [[nodiscard]] size_t getId() const { return _id; }
    [[nodiscard]] const std::string& getName() const { return name; }
    RenderResource& setName(const std::string& n) {
      this->name = n;
      return *this;
    }

    RenderResource& setPhysicalId(PhysResourceId id) {
      physicalResourceId = id;
      return *this;
    }
    [[nodiscard]] PhysResourceId getPhysicalId() const { return physicalResourceId; }

  private:
    Type _type;
    ResourceId _id;
    PhysResourceId physicalResourceId{};
    std::unordered_set<PassId> readInPasses;
    std::unordered_set<PassId> writtenInPasses;
    std::string name;
    Bitflag<QueueType> usedQueues{};
  };

  class RenderBufferResource : public RenderResource {
  public:
    explicit RenderBufferResource(ResourceId id) : RenderResource(RenderResource::Type::Buffer, id) {}

    RenderBufferResource& setBufferInfo(const BufferInfo& i) {
      this->info = i;
      return *this;
    }
    [[nodiscard]] const BufferInfo& getBufferInfo() const { return info; }

    RenderBufferResource& addBufferUsage(rhi::BufferUsage u) {
      this->usage |= u;
      return *this;
    }
    [[nodiscard]] Bitflag<rhi::BufferUsage> getBufferUsage() const { return usage; }

  private:
    BufferInfo info{};

    Bitflag<rhi::BufferUsage> usage = rhi::BufferUsage::None;
  };

  class RenderTextureResource : public RenderResource {
  public:
    explicit RenderTextureResource(ResourceId id) : RenderResource(RenderResource::Type::Texture, id) {}

    RenderTextureResource& setAttachmentInfo(const AttachmentInfo& i) {
      this->info = i;
      return *this;
    }
    [[nodiscard]] const AttachmentInfo& getAttachmentInfo() const { return info; }

    RenderTextureResource& addImageUsage(rhi::ImageUsage u) {
      this->usage |= u;
      return *this;
    }
    [[nodiscard]] Bitflag<rhi::ImageUsage> getImageUsage() const { return usage; }

    RenderTextureResource& setTransient(bool t) {
      this->transient = t;
      return *this;
    }
    [[nodiscard]] bool isTransient() const { return transient; }

  private:
    AttachmentInfo info{};
    Bitflag<rhi::ImageUsage> usage = rhi::ImageUsage::None;
    bool transient = false;
  };
} // namespace kt