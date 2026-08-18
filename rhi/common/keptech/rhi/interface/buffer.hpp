public:
static kt::Result<Buffer, HRESULT, 0> create(const BufferCreateInfo& info);

const std::string& getName() const;
BufferType getType() const;
size_t size() const;
bool isMapped() const;
void* mapping(size_t offset = 0) const;

bool isValid() const;

template <typename T> T* mapping(size_t offset = 0) const { return static_cast<T*>(mapping(offset)); }

Bitflag<BufferUsage> getUsage() const;

void destroy();

/// Creates a new buffer with the specified size.
/// @param newSize The new size of the buffer in bytes.
/// @returns A Result containing the new buffer, or an error code if the reallocation failed.
/// @note Data is not copied from the old buffer to the new buffer. The caller is responsible for copying any necessary data after the
/// reallocation.
/// @note The old buffer may need to be submitted to the RHI for destruction next frame, as it may still be in use by the GPU.
kt::Result<Buffer, HRESULT, S_OK> reallocate(size_t newSize);

operator BufferRef() const;

Buffer() = default;
Buffer(const Buffer&) = delete;
Buffer& operator=(const Buffer&) = delete;
Buffer(Buffer&& other) noexcept;
Buffer& operator=(Buffer&& other) noexcept;
~Buffer();