public:
static RHI& get();
inline uint8_t getLastFrameIndex() const { return (m.frameIndex + MAX_FRAMES_IN_FLIGHT - 1) % MAX_FRAMES_IN_FLIGHT; }
inline uint8_t getFrameIndex() const { return m.frameIndex; }
inline uint8_t getNextFrameIndex() const { return (m.frameIndex + 1) % MAX_FRAMES_IN_FLIGHT; }

#ifndef KT_DISABLE_STATS
struct Stats {
  size_t drawCalls = 0;
  size_t dispatchCalls = 0;
  size_t pipelineSwitches = 0;
  size_t renderPasses = 0;
};

Stats& getStats() { return stats; }

void resetStats() {
  stats.drawCalls = 0;
  stats.dispatchCalls = 0;
  stats.pipelineSwitches = 0;
  stats.renderPasses = 0;
}
#else
void resetStats() {}
#endif

bool canRenderToFormat(ImageFormat format) const;
bool canSampleFromFormat(ImageFormat format) const;

ImageFormat getSwapchainFormat() const;
ImageRef getSwapchainImage() const;
glm::uvec2 getSwapchainSize() const;

uint64_t getTimelineValue() const;

void submitGraphicsCmd(CommandBuffer& cmd, uint64_t waitFor = 0, uint64_t signalTo = 0, uint64_t waitForCopy = 0);
void submitComputeCmd(CommandBuffer& cmd, uint64_t waitFor = 0, uint64_t signalTo = 0, uint64_t waitForCopy = 0);

std::vector<CommandBuffer> allocateGraphicsCommandBuffers(uint32_t count);
std::vector<CommandBuffer> allocateComputeCommandBuffers(uint32_t count);

void submitBufferToDrop(Buffer& buffer);
void submitImageToDrop(Image& image);

kt::Result<ImageRef, RawRhiResult, RawRhiResultOk> createTexture(const ImageCreateInfo& createInfo);
DescriptorLayout createDescriptorLayout(std::span<const DescriptorInfo> infos);
DescriptorPool createDescriptorPool(const DescriptorPoolInfo& poolInfo);

void waitGraphicsIdle();
void waitComputeIdle();
void waitCopyIdle();
void waitIdle();

template <typename F>
  requires(std::is_invocable_v<F, CommandBuffer&> && std::is_same_v<std::invoke_result_t<F, CommandBuffer&>, std::vector<Buffer>>)
uint64_t oneshotCopy(const F& copyFunc);

// Called internally, don't use
void newFrame();
void startFrame();
void endFrame(CommandBuffer& cmdBuf);

static bool isInit();
std::expected<void, std::string> static init(const RendererCreateInfo& createInfo, const Window& window);

void onResize();

RHI(const RHI&) = delete;
RHI& operator=(const RHI&) = delete;
RHI(RHI&&) = delete;
RHI& operator=(RHI&&) = delete;

~RHI();

private:
void present();

void debugUi() const;

RHI() = default;

static RHI singleton;
static bool isInitialized;

#ifndef KT_DISABLE_STATS
Stats stats;
#endif