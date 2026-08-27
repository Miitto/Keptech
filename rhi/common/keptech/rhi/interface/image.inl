public:
ImageFormat format() const;
ImageDim dim() const;
uint32_t mips() const;
uint32_t layers() const;
glm::uvec3 getExtent() const;
const std::string& getName() const;
Bitflag<ImageUsage> getUsage() const;

bool operator==(const Image& other) const;

bool isDepth() const;

void setTextureIndex(uint64_t index) { textureIndex = index; }
uint64_t getTextureIndex() const { return textureIndex; }

void destroy();

operator ImageRef() const;

static Result<Image, RawRhiResult, RawRhiResultOk> create(const ImageCreateInfo& info);

Image() = default;
Image(const Image&) = delete;
Image& operator=(const Image&) = delete;
Image(Image&& other) noexcept;
Image& operator=(Image&& other) noexcept;
~Image() { destroy(); }

kt::Result<Image, RawRhiResult, RawRhiResultOk> resize(const glm::uvec3& newExtent) const;