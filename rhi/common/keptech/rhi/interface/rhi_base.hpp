#pragma once

#include "keptech/core/result.hpp"
#include "keptech/rhi/imageFormat.hpp"
#include "keptech/rhi/result.hpp"
#include "keptech/rhi/sync.hpp"
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
    class ImageCreateInfo;
    class DescriptorLayout;
    class DescriptorSet;
    struct DescriptorInfo;

    class RhiBase {
    public:
      friend class RHI;

      static RHI& get();

      // Frame info
      uint8_t getLastFrameIndex(this const RHI& self);
      uint8_t getFrameIndex(this const RHI& self);
      uint8_t getNextFrameIndex(this const RHI& self);

      // Format info
      bool canRenderToFormat(this const RHI& self, ImageFormat format);
      bool canSampleFromFormat(this const RHI& self, ImageFormat format);
      ImageFormat getSwapchainFormat(this const RHI& self);
      ImageRef getSwapchainImage(this const RHI& self);
      glm::uvec2 getSwapchainSize(this const RHI& self);
      size_t getByteSize(this const RHI& self, ImageFormat format, glm::uvec3 size, uint32_t mipLevels = 1, uint32_t layers = 1);

      // Command Buffers
      GraphicsWaitValue submitGraphicsCmd(this RHI& self, CommandBuffer& cmd, GraphicsWaitValue graphicsWaitValue = {},
                                          ComputeWaitValue computeWaitValue = {}, CopyWaitValue copyWaitValue = {});
      ComputeWaitValue submitComputeCmd(this RHI& self, CommandBuffer& cmd, GraphicsWaitValue graphicsWaitValue = {},
                                        ComputeWaitValue computeWaitValue = {}, CopyWaitValue copyWaitValue = {});

      Vec<CommandBuffer> allocateGraphicsCommandBuffers(this RHI& self, uint32_t count);
      Vec<CommandBuffer> allocateComputeCommandBuffers(this RHI& self, uint32_t count);

      // Resource management
      void submitBufferToDrop(this RHI& self, Buffer& buffer);
      void submitImageToDrop(this RHI& self, Image& image);

      kt::Result<ImageRef, RawRhiResult, RawRhiResultOk> createTexture(this RHI& self, const ImageCreateInfo& createInfo);
      DescriptorLayout createDescriptorLayout(this RHI& self, std::span<const DescriptorInfo> infos);
      DescriptorSet allocateDescriptorSet(this RHI& self, const DescriptorLayout& layout);

      // Sync
      GraphicsWaitValue getGraphicsWaitValue(this const RHI& self);
      ComputeWaitValue getComputeWaitValue(this const RHI& self);
      CopyWaitValue getCopyWaitValue(this const RHI& self);
      void waitGraphicsIdle(this RHI& self);
      void waitComputeIdle(this RHI& self);
      void waitCopyIdle(this RHI& self);
      void waitIdle(this RHI& self);

      template <typename F>
        requires(std::is_invocable_v<F, CommandBuffer&> && std::is_same_v<std::invoke_result_t<F, CommandBuffer&>, Vec<Buffer>>)
      CopyWaitValue oneshotCopy(this RHI& self, const F& copyFunc);

      // Stats
#ifndef KT_DISABLE_STATS
      struct Stats {
        size_t drawCalls = 0;
        size_t dispatchCalls = 0;
        size_t pipelineSwitches = 0;
        size_t renderPasses = 0;

        size_t userBufferCount = 0;
        size_t userBufferMemUsage = 0;
        size_t userImageCount = 0;
        size_t userImageMemUsage = 0;

        size_t engineBufferCount = 0;
        size_t engineBufferMemUsage = 0;
        size_t engineImageCount = 0;
        size_t engineImageMemUsage = 0;
      };

      Stats& getStats();
#endif

      // GPU Info
      struct GPUInfo {
        std::string name;
        size_t vramSize = 0;
        size_t hostRamSize = 0;
        bool reBar = false;
      };

      /// Get GPU information.
      /// Info is cached once on startup.
      const GPUInfo& getGPUInfo(this const RHI& self);

      struct LiveMemInfo {
        uint64_t used = 0;
        uint64_t budget = 0;
      };
      /// Get live VRAM information.
      /// Info is queried per call.
      LiveMemInfo getLiveVramInfo(this const RHI& self);
      /// Get live Host RAM information.
      /// Info is queried per call.
      LiveMemInfo getLiveHostRamInfo(this const RHI& self);

#pragma region "Internal don't use"
      void resetStats();

      void newFrame(this RHI& self);
      void startFrame(this RHI& self);
      void endFrame(this RHI& self, CommandBuffer& cmdBuf);

      static bool isInit();
      std::expected<void, std::string> static init(const RendererCreateInfo& createInfo, const Window& window);

      void shutdown(this RHI& self);

      void onResize(this RHI& self);
#pragma endregion

      RhiBase(const RhiBase&) = delete;
      RhiBase& operator=(const RhiBase&) = delete;
      RhiBase(RhiBase&&) = delete;
      RhiBase& operator=(RhiBase&&) = delete;

      ~RhiBase() = default;

    private:
      void present(this RHI& self);

      void debugUi(this const RHI& self);

      RhiBase() = default;

      static RHI singleton;
      static bool isInitialized;

#ifndef KT_DISABLE_STATS
      Stats stats;
#endif

      GPUInfo gpuInfo;
    };
  } // namespace rhi
} // namespace kt