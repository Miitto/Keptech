#pragma once

#include "keptech/core/result.hpp"
#include "keptech/rhi/imageFormat.hpp"
#include "keptech/rhi/result.hpp"
#include "keptech/std/vec.hpp"
#include <expected>
#include <span>

namespace kt {
  class Window;
  struct RendererCreateInfo;

  namespace rhi {
    class RHI;
    class ImageRef;
    class CommandBuffer;
    class Buffer;
    class Image;
    struct ImageCreateInfo;
    class DescriptorLayout;
    class DescriptorSet;
    struct DescriptorInfo;

    class RhiBase {
    public:
      static RHI& get();
      uint8_t getLastFrameIndex(this const RHI& self);
      uint8_t getFrameIndex(this const RHI& self);
      uint8_t getNextFrameIndex(this const RHI& self);

#ifndef KT_DISABLE_STATS
      struct Stats {
        size_t drawCalls = 0;
        size_t dispatchCalls = 0;
        size_t pipelineSwitches = 0;
        size_t renderPasses = 0;
      };

      Stats& getStats();
#endif
      void resetStats();

      bool canRenderToFormat(this const RHI& self, ImageFormat format);
      bool canSampleFromFormat(this const RHI& self, ImageFormat format);

      ImageFormat getSwapchainFormat(this const RHI& self);
      ImageRef getSwapchainImage(this const RHI& self);
      glm::uvec2 getSwapchainSize(this const RHI& self);

      uint64_t getTimelineValue(this const RHI& self);

      void submitGraphicsCmd(this RHI& self, CommandBuffer& cmd, uint64_t waitFor = 0, uint64_t signalTo = 0, uint64_t waitForCopy = 0);
      void submitComputeCmd(this RHI& self, CommandBuffer& cmd, uint64_t waitFor = 0, uint64_t signalTo = 0, uint64_t waitForCopy = 0);

      Vec<CommandBuffer> allocateGraphicsCommandBuffers(this RHI& self, uint32_t count);
      Vec<CommandBuffer> allocateComputeCommandBuffers(this RHI& self, uint32_t count);

      void submitBufferToDrop(this RHI& self, Buffer& buffer);
      void submitImageToDrop(this RHI& self, Image& image);

      kt::Result<ImageRef, RawRhiResult, RawRhiResultOk> createTexture(this RHI& self, const ImageCreateInfo& createInfo);
      DescriptorLayout createDescriptorLayout(this RHI& self, std::span<const DescriptorInfo> infos);
      DescriptorSet allocateDescriptorSet(this RHI& self, const DescriptorLayout& layout);

      void waitGraphicsIdle(this RHI& self);
      void waitComputeIdle(this RHI& self);
      void waitCopyIdle(this RHI& self);
      void waitIdle(this RHI& self);

      template <typename F>
        requires(std::is_invocable_v<F, CommandBuffer&> && std::is_same_v<std::invoke_result_t<F, CommandBuffer&>, Vec<Buffer>>)
      uint64_t oneshotCopy(this RHI& self, const F& copyFunc);

      // Called internally, don't use
      void newFrame(this RHI& self);
      void startFrame(this RHI& self);
      void endFrame(this RHI& self, CommandBuffer& cmdBuf);

      static bool isInit();
      std::expected<void, std::string> static init(const RendererCreateInfo& createInfo, const Window& window);

      void shutdown(this RHI& self);

      void onResize(this RHI& self);

      RhiBase(const RhiBase&) = delete;
      RhiBase& operator=(const RhiBase&) = delete;
      RhiBase(RhiBase&&) = delete;
      RhiBase& operator=(RhiBase&&) = delete;

      ~RhiBase() = default;

    protected:
      void present(this RHI& self);

      void debugUi(this const RHI& self);

      RhiBase() = default;

      static RHI singleton;
      static bool isInitialized;

#ifndef KT_DISABLE_STATS
      Stats stats;
#endif
    };
  } // namespace rhi
} // namespace kt