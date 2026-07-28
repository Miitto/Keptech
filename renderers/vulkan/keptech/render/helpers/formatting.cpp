#include "keptech/render/helpers/formatting.hpp"
#include <spdlog/fmt/bundled/ranges.h>
#include <vector>

fmt::format_context::iterator fmt::formatter<VkResult>::format(VkResult format, fmt::format_context& ctx) const {
  std::string_view name = "Unknown";
#define CASE(x)                                                                                                                            \
  case VK_##x:                                                                                                                             \
    name = #x;                                                                                                                             \
    break;
  switch (format) {
    CASE(SUCCESS)
    CASE(NOT_READY)
    CASE(TIMEOUT)
    CASE(EVENT_SET)
    CASE(EVENT_RESET)
    CASE(INCOMPLETE)
    CASE(ERROR_OUT_OF_HOST_MEMORY)
    CASE(ERROR_OUT_OF_DEVICE_MEMORY)
    CASE(ERROR_INITIALIZATION_FAILED)
    CASE(ERROR_DEVICE_LOST)
    CASE(ERROR_MEMORY_MAP_FAILED)
    CASE(ERROR_LAYER_NOT_PRESENT)
    CASE(ERROR_EXTENSION_NOT_PRESENT)
    CASE(ERROR_FEATURE_NOT_PRESENT)
    CASE(ERROR_INCOMPATIBLE_DRIVER)
    CASE(ERROR_TOO_MANY_OBJECTS)
    CASE(ERROR_FORMAT_NOT_SUPPORTED)
    CASE(ERROR_FRAGMENTED_POOL)
    CASE(ERROR_UNKNOWN)
    CASE(ERROR_VALIDATION_FAILED)
    CASE(ERROR_OUT_OF_POOL_MEMORY)
    CASE(ERROR_INVALID_EXTERNAL_HANDLE)
    CASE(ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS)
    CASE(ERROR_FRAGMENTATION)
    CASE(PIPELINE_COMPILE_REQUIRED)
    CASE(ERROR_NOT_PERMITTED)
    CASE(ERROR_SURFACE_LOST_KHR)
    CASE(ERROR_NATIVE_WINDOW_IN_USE_KHR)
    CASE(SUBOPTIMAL_KHR)
    CASE(ERROR_OUT_OF_DATE_KHR)
    CASE(ERROR_INCOMPATIBLE_DISPLAY_KHR)
    CASE(ERROR_INVALID_SHADER_NV)
    CASE(ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR)
    CASE(ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR)
    CASE(ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR)
    CASE(ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR)
    CASE(ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR)
    CASE(ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR)
    CASE(ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT)
    CASE(ERROR_PRESENT_TIMING_QUEUE_FULL_EXT)
    CASE(ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT)
    CASE(THREAD_IDLE_KHR)
    CASE(THREAD_DONE_KHR)
    CASE(OPERATION_DEFERRED_KHR)
    CASE(OPERATION_NOT_DEFERRED_KHR)
    CASE(ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR)
    CASE(ERROR_COMPRESSION_EXHAUSTED_EXT)
    CASE(INCOMPATIBLE_SHADER_BINARY_EXT)
    CASE(PIPELINE_BINARY_MISSING_KHR)
    CASE(ERROR_NOT_ENOUGH_SPACE_KHR)
    CASE(RESULT_MAX_ENUM)
  }
#undef CASE
  return fmt::formatter<std::string_view>::format(name, ctx);
}

fmt::format_context::iterator fmt::formatter<VkFormat>::format(VkFormat format, fmt::format_context& ctx) const {
  std::string_view name = "Invalid";
#define CASE(x)                                                                                                                            \
  case VK_FORMAT_##x:                                                                                                                      \
    name = #x;                                                                                                                             \
    break;
  switch (format) {
    CASE(UNDEFINED)
    CASE(R4G4_UNORM_PACK8)
    CASE(R4G4B4A4_UNORM_PACK16)
    CASE(B4G4R4A4_UNORM_PACK16)
    CASE(R5G6B5_UNORM_PACK16)
    CASE(B5G6R5_UNORM_PACK16)
    CASE(R5G5B5A1_UNORM_PACK16)
    CASE(B5G5R5A1_UNORM_PACK16)
    CASE(A1R5G5B5_UNORM_PACK16)
    CASE(R8_UNORM)
    CASE(R8_SNORM)
    CASE(R8_USCALED)
    CASE(R8_SSCALED)
    CASE(R8_UINT)
    CASE(R8_SINT)
    CASE(R8_SRGB)
    CASE(R8G8_UNORM)
    CASE(R8G8_SNORM)
    CASE(R8G8_USCALED)
    CASE(R8G8_SSCALED)
    CASE(R8G8_UINT)
    CASE(R8G8_SINT)
    CASE(R8G8_SRGB)
    CASE(R8G8B8_UNORM)
    CASE(R8G8B8_SNORM)
    CASE(R8G8B8_USCALED)
    CASE(R8G8B8_SSCALED)
    CASE(R8G8B8_UINT)
    CASE(R8G8B8_SINT)
    CASE(R8G8B8_SRGB)
    CASE(B8G8R8_UNORM)
    CASE(B8G8R8_SNORM)
    CASE(B8G8R8_USCALED)
    CASE(B8G8R8_SSCALED)
    CASE(B8G8R8_UINT)
    CASE(B8G8R8_SINT)
    CASE(B8G8R8_SRGB)
    CASE(R8G8B8A8_UNORM)
    CASE(R8G8B8A8_SNORM)
    CASE(R8G8B8A8_USCALED)
    CASE(R8G8B8A8_SSCALED)
    CASE(R8G8B8A8_UINT)
    CASE(R8G8B8A8_SINT)
    CASE(R8G8B8A8_SRGB)
    CASE(B8G8R8A8_UNORM)
    CASE(B8G8R8A8_SNORM)
    CASE(B8G8R8A8_USCALED)
    CASE(B8G8R8A8_SSCALED)
    CASE(B8G8R8A8_UINT)
    CASE(B8G8R8A8_SINT)
    CASE(B8G8R8A8_SRGB)
    CASE(A8B8G8R8_UNORM_PACK32)
    CASE(A8B8G8R8_SNORM_PACK32)
    CASE(A8B8G8R8_USCALED_PACK32)
    CASE(A8B8G8R8_SSCALED_PACK32)
    CASE(A8B8G8R8_UINT_PACK32)
    CASE(A8B8G8R8_SINT_PACK32)
    CASE(A8B8G8R8_SRGB_PACK32)
    CASE(A2R10G10B10_UNORM_PACK32)
    CASE(A2R10G10B10_SNORM_PACK32)
    CASE(A2R10G10B10_USCALED_PACK32)
    CASE(A2R10G10B10_SSCALED_PACK32)
    CASE(A2R10G10B10_UINT_PACK32)
    CASE(A2R10G10B10_SINT_PACK32)
    CASE(A2B10G10R10_UNORM_PACK32)
    CASE(A2B10G10R10_SNORM_PACK32)
    CASE(A2B10G10R10_USCALED_PACK32)
    CASE(A2B10G10R10_SSCALED_PACK32)
    CASE(A2B10G10R10_UINT_PACK32)
    CASE(A2B10G10R10_SINT_PACK32)
    CASE(R16_UNORM)
    CASE(R16_SNORM)
    CASE(R16_USCALED)
    CASE(R16_SSCALED)
    CASE(R16_UINT)
    CASE(R16_SINT)
    CASE(R16_SFLOAT)
    CASE(R16G16_UNORM)
    CASE(R16G16_SNORM)
    CASE(R16G16_USCALED)
    CASE(R16G16_SSCALED)
    CASE(R16G16_UINT)
    CASE(R16G16_SINT)
    CASE(R16G16_SFLOAT)
    CASE(R16G16B16_UNORM)
    CASE(R16G16B16_SNORM)
    CASE(R16G16B16_USCALED)
    CASE(R16G16B16_SSCALED)
    CASE(R16G16B16_UINT)
    CASE(R16G16B16_SINT)
    CASE(R16G16B16_SFLOAT)
    CASE(R16G16B16A16_UNORM)
    CASE(R16G16B16A16_SNORM)
    CASE(R16G16B16A16_USCALED)
    CASE(R16G16B16A16_SSCALED)
    CASE(R16G16B16A16_UINT)
    CASE(R16G16B16A16_SINT)
    CASE(R16G16B16A16_SFLOAT)
    CASE(R32_UINT)
    CASE(R32_SINT)
    CASE(R32_SFLOAT)
    CASE(R32G32_UINT)
    CASE(R32G32_SINT)
    CASE(R32G32_SFLOAT)
    CASE(R32G32B32_UINT)
    CASE(R32G32B32_SINT)
    CASE(R32G32B32_SFLOAT)
    CASE(R32G32B32A32_UINT)
    CASE(R32G32B32A32_SINT)
    CASE(R32G32B32A32_SFLOAT)
    CASE(R64_UINT)
    CASE(R64_SINT)
    CASE(R64_SFLOAT)
    CASE(R64G64_UINT)
    CASE(R64G64_SINT)
    CASE(R64G64_SFLOAT)
    CASE(R64G64B64_UINT)
    CASE(R64G64B64_SINT)
    CASE(R64G64B64_SFLOAT)
    CASE(R64G64B64A64_UINT)
    CASE(R64G64B64A64_SINT)
    CASE(R64G64B64A64_SFLOAT)
    CASE(B10G11R11_UFLOAT_PACK32)
    CASE(E5B9G9R9_UFLOAT_PACK32)
    CASE(D16_UNORM)
    CASE(X8_D24_UNORM_PACK32)
    CASE(D32_SFLOAT)
    CASE(S8_UINT)
    CASE(D16_UNORM_S8_UINT)
    CASE(D24_UNORM_S8_UINT)
    CASE(D32_SFLOAT_S8_UINT)
    CASE(BC1_RGB_UNORM_BLOCK)
    CASE(BC1_RGB_SRGB_BLOCK)
    CASE(BC1_RGBA_UNORM_BLOCK)
    CASE(BC1_RGBA_SRGB_BLOCK)
    CASE(BC2_UNORM_BLOCK)
    CASE(BC2_SRGB_BLOCK)
    CASE(BC3_UNORM_BLOCK)
    CASE(BC3_SRGB_BLOCK)
    CASE(BC4_UNORM_BLOCK)
    CASE(BC4_SNORM_BLOCK)
    CASE(BC5_UNORM_BLOCK)
    CASE(BC5_SNORM_BLOCK)
    CASE(BC6H_UFLOAT_BLOCK)
    CASE(BC6H_SFLOAT_BLOCK)
    CASE(BC7_UNORM_BLOCK)
    CASE(BC7_SRGB_BLOCK)
    CASE(ETC2_R8G8B8_UNORM_BLOCK)
    CASE(ETC2_R8G8B8_SRGB_BLOCK)
    CASE(ETC2_R8G8B8A1_UNORM_BLOCK)
    CASE(ETC2_R8G8B8A1_SRGB_BLOCK)
    CASE(ETC2_R8G8B8A8_UNORM_BLOCK)
    CASE(ETC2_R8G8B8A8_SRGB_BLOCK)
    CASE(EAC_R11_UNORM_BLOCK)
    CASE(EAC_R11_SNORM_BLOCK)
    CASE(EAC_R11G11_UNORM_BLOCK)
    CASE(EAC_R11G11_SNORM_BLOCK)
    CASE(ASTC_4x4_UNORM_BLOCK)
    CASE(ASTC_4x4_SRGB_BLOCK)
    CASE(ASTC_5x4_UNORM_BLOCK)
    CASE(ASTC_5x4_SRGB_BLOCK)
    CASE(ASTC_5x5_UNORM_BLOCK)
    CASE(ASTC_5x5_SRGB_BLOCK)
    CASE(ASTC_6x5_UNORM_BLOCK)
    CASE(ASTC_6x5_SRGB_BLOCK)
    CASE(ASTC_6x6_UNORM_BLOCK)
    CASE(ASTC_6x6_SRGB_BLOCK)
    CASE(ASTC_8x5_UNORM_BLOCK)
    CASE(ASTC_8x5_SRGB_BLOCK)
    CASE(ASTC_8x6_UNORM_BLOCK)
    CASE(ASTC_8x6_SRGB_BLOCK)
    CASE(ASTC_8x8_UNORM_BLOCK)
    CASE(ASTC_8x8_SRGB_BLOCK)
    CASE(ASTC_10x5_UNORM_BLOCK)
    CASE(ASTC_10x5_SRGB_BLOCK)
    CASE(ASTC_10x6_UNORM_BLOCK)
    CASE(ASTC_10x6_SRGB_BLOCK)
    CASE(ASTC_10x8_UNORM_BLOCK)
    CASE(ASTC_10x8_SRGB_BLOCK)
    CASE(ASTC_10x10_UNORM_BLOCK)
    CASE(ASTC_10x10_SRGB_BLOCK)
    CASE(ASTC_12x10_UNORM_BLOCK)
    CASE(ASTC_12x10_SRGB_BLOCK)
    CASE(ASTC_12x12_UNORM_BLOCK)
    CASE(ASTC_12x12_SRGB_BLOCK)
    CASE(G8B8G8R8_422_UNORM)
    CASE(B8G8R8G8_422_UNORM)
    CASE(G8_B8_R8_3PLANE_420_UNORM)
    CASE(G8_B8R8_2PLANE_420_UNORM)
    CASE(G8_B8_R8_3PLANE_422_UNORM)
    CASE(G8_B8R8_2PLANE_422_UNORM)
    CASE(G8_B8_R8_3PLANE_444_UNORM)
    CASE(R10X6_UNORM_PACK16)
    CASE(R10X6G10X6_UNORM_2PACK16)
    CASE(R10X6G10X6B10X6A10X6_UNORM_4PACK16)
    CASE(G10X6B10X6G10X6R10X6_422_UNORM_4PACK16)
    CASE(B10X6G10X6R10X6G10X6_422_UNORM_4PACK16)
    CASE(G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16)
    CASE(G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16)
    CASE(G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16)
    CASE(G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16)
    CASE(G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16)
    CASE(R12X4_UNORM_PACK16)
    CASE(R12X4G12X4_UNORM_2PACK16)
    CASE(R12X4G12X4B12X4A12X4_UNORM_4PACK16)
    CASE(G12X4B12X4G12X4R12X4_422_UNORM_4PACK16)
    CASE(B12X4G12X4R12X4G12X4_422_UNORM_4PACK16)
    CASE(G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16)
    CASE(G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16)
    CASE(G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16)
    CASE(G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16)
    CASE(G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16)
    CASE(G16B16G16R16_422_UNORM)
    CASE(B16G16R16G16_422_UNORM)
    CASE(G16_B16_R16_3PLANE_420_UNORM)
    CASE(G16_B16R16_2PLANE_420_UNORM)
    CASE(G16_B16_R16_3PLANE_422_UNORM)
    CASE(G16_B16R16_2PLANE_422_UNORM)
    CASE(G16_B16_R16_3PLANE_444_UNORM)
    CASE(G8_B8R8_2PLANE_444_UNORM)
    CASE(G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16)
    CASE(G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16)
    CASE(G16_B16R16_2PLANE_444_UNORM)
    CASE(A4R4G4B4_UNORM_PACK16)
    CASE(A4B4G4R4_UNORM_PACK16)
    CASE(ASTC_4x4_SFLOAT_BLOCK)
    CASE(ASTC_5x4_SFLOAT_BLOCK)
    CASE(ASTC_5x5_SFLOAT_BLOCK)
    CASE(ASTC_6x5_SFLOAT_BLOCK)
    CASE(ASTC_6x6_SFLOAT_BLOCK)
    CASE(ASTC_8x5_SFLOAT_BLOCK)
    CASE(ASTC_8x6_SFLOAT_BLOCK)
    CASE(ASTC_8x8_SFLOAT_BLOCK)
    CASE(ASTC_10x5_SFLOAT_BLOCK)
    CASE(ASTC_10x6_SFLOAT_BLOCK)
    CASE(ASTC_10x8_SFLOAT_BLOCK)
    CASE(ASTC_10x10_SFLOAT_BLOCK)
    CASE(ASTC_12x10_SFLOAT_BLOCK)
    CASE(ASTC_12x12_SFLOAT_BLOCK)
    CASE(A1B5G5R5_UNORM_PACK16)
    CASE(A8_UNORM)
    CASE(PVRTC1_2BPP_UNORM_BLOCK_IMG)
    CASE(PVRTC1_4BPP_UNORM_BLOCK_IMG)
    CASE(PVRTC2_2BPP_UNORM_BLOCK_IMG)
    CASE(PVRTC2_4BPP_UNORM_BLOCK_IMG)
    CASE(PVRTC1_2BPP_SRGB_BLOCK_IMG)
    CASE(PVRTC1_4BPP_SRGB_BLOCK_IMG)
    CASE(PVRTC2_2BPP_SRGB_BLOCK_IMG)
    CASE(PVRTC2_4BPP_SRGB_BLOCK_IMG)
    CASE(R8_BOOL_ARM)
    CASE(R16G16_SFIXED5_NV)
    CASE(R10X6_UINT_PACK16_ARM)
    CASE(R10X6G10X6_UINT_2PACK16_ARM)
    CASE(R10X6G10X6B10X6A10X6_UINT_4PACK16_ARM)
    CASE(R12X4_UINT_PACK16_ARM)
    CASE(R12X4G12X4_UINT_2PACK16_ARM)
    CASE(R12X4G12X4B12X4A12X4_UINT_4PACK16_ARM)
    CASE(R14X2_UINT_PACK16_ARM)
    CASE(R14X2G14X2_UINT_2PACK16_ARM)
    CASE(R14X2G14X2B14X2A14X2_UINT_4PACK16_ARM)
    CASE(R14X2_UNORM_PACK16_ARM)
    CASE(R14X2G14X2_UNORM_2PACK16_ARM)
    CASE(R14X2G14X2B14X2A14X2_UNORM_4PACK16_ARM)
    CASE(G14X2_B14X2R14X2_2PLANE_420_UNORM_3PACK16_ARM)
    CASE(G14X2_B14X2R14X2_2PLANE_422_UNORM_3PACK16_ARM)
  case VK_FORMAT_MAX_ENUM:
    break;
  }
#undef CASE

  return fmt::format_to(ctx.out(), "{}", name);
}

fmt::format_context::iterator fmt::formatter<VkPresentModeKHR>::format(VkPresentModeKHR format, fmt::format_context& ctx) const {
  std::string_view name = "Unknown";
#define CASE(x)                                                                                                                            \
  case VK_##x:                                                                                                                             \
    name = #x;                                                                                                                             \
    break;

  switch (format) {
    CASE(PRESENT_MODE_IMMEDIATE_KHR)
    CASE(PRESENT_MODE_MAILBOX_KHR)
    CASE(PRESENT_MODE_FIFO_KHR)
    CASE(PRESENT_MODE_FIFO_RELAXED_KHR)
    CASE(PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR)
    CASE(PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR)
    CASE(PRESENT_MODE_FIFO_LATEST_READY_KHR)
    CASE(PRESENT_MODE_MAX_ENUM_KHR)
  }
#undef CASE
  return fmt::format_to(ctx.out(), "{}", name);
}

fmt::format_context::iterator fmt::formatter<VkImageLayout>::format(VkImageLayout layout, fmt::format_context& ctx) const {
  std::string_view name = "Unknown";
#define CASE(x)                                                                                                                            \
  case VK_IMAGE_LAYOUT_##x:                                                                                                                \
    name = #x;                                                                                                                             \
    break;

  switch (layout) {
    CASE(UNDEFINED)
    CASE(GENERAL)
    CASE(COLOR_ATTACHMENT_OPTIMAL)
    CASE(DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
    CASE(DEPTH_STENCIL_READ_ONLY_OPTIMAL)
    CASE(SHADER_READ_ONLY_OPTIMAL)
    CASE(TRANSFER_SRC_OPTIMAL)
    CASE(TRANSFER_DST_OPTIMAL)
    CASE(PREINITIALIZED)
    CASE(DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL)
    CASE(DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL)
    CASE(DEPTH_ATTACHMENT_OPTIMAL)
    CASE(DEPTH_READ_ONLY_OPTIMAL)
    CASE(STENCIL_ATTACHMENT_OPTIMAL)
    CASE(STENCIL_READ_ONLY_OPTIMAL)
    CASE(PRESENT_SRC_KHR)
    CASE(READ_ONLY_OPTIMAL)
    CASE(ATTACHMENT_OPTIMAL)
    CASE(RENDERING_LOCAL_READ)
    CASE(VIDEO_DECODE_DST_KHR)
    CASE(VIDEO_DECODE_SRC_KHR)
    CASE(VIDEO_DECODE_DPB_KHR)
    CASE(SHARED_PRESENT_KHR)
    CASE(FRAGMENT_DENSITY_MAP_OPTIMAL_EXT)
    CASE(FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR)
    CASE(VIDEO_ENCODE_DST_KHR)
    CASE(VIDEO_ENCODE_SRC_KHR)
    CASE(VIDEO_ENCODE_DPB_KHR)
    CASE(ATTACHMENT_FEEDBACK_LOOP_OPTIMAL_EXT)
    CASE(TENSOR_ALIASING_ARM)
    CASE(VIDEO_ENCODE_QUANTIZATION_MAP_KHR)
    CASE(ZERO_INITIALIZED_EXT)
    CASE(MAX_ENUM)
    break;
  }

#undef CASE
  return fmt::format_to(ctx.out(), "{}", name);
}

fmt::format_context::iterator fmt::formatter<kt::rdr::VkAccessFlags2Formatter>::format(kt::rdr::VkAccessFlags2Formatter formatter,
                                                                                       fmt::format_context& ctx) const {
  std::vector<std::string_view> accessFlags;

#define B(x)                                                                                                                               \
  if (formatter.flags & VK_ACCESS_2_##x##_BIT)                                                                                             \
    accessFlags.emplace_back(#x);
#define B_KHR(x)                                                                                                                           \
  if (formatter.flags & VK_ACCESS_2_##x##_BIT_KHR)                                                                                         \
    accessFlags.emplace_back(#x "_KHR");
#define B_EXT(x)                                                                                                                           \
  if (formatter.flags & VK_ACCESS_2_##x##_BIT_EXT)                                                                                         \
    accessFlags.emplace_back(#x "_EXT");
#define B_NV(x)                                                                                                                            \
  if (formatter.flags & VK_ACCESS_2_##x##_BIT_NV)                                                                                          \
    accessFlags.emplace_back(#x "_NV");
#define B_QCOM(x)                                                                                                                          \
  if (formatter.flags & VK_ACCESS_2_##x##_BIT_QCOM)                                                                                        \
    accessFlags.emplace_back(#x "_QCOM");
#define B_ARM(x)                                                                                                                           \
  if (formatter.flags & VK_ACCESS_2_##x##_BIT_ARM)                                                                                         \
    accessFlags.emplace_back(#x "_ARM");
#define B_HUAWEI(x)                                                                                                                        \
  if (formatter.flags & VK_ACCESS_2_##x##_BIT_HUAWEI)                                                                                      \
    accessFlags.emplace_back(#x "_HUAWEI");

  auto f = formatter.flags;
  if (f == VK_ACCESS_NONE) {
    return fmt::format_to(ctx.out(), "{}", "NONE");
  }
  B(INDIRECT_COMMAND_READ);
  B(INDEX_READ);
  B(VERTEX_ATTRIBUTE_READ);
  B(UNIFORM_READ);
  B(INPUT_ATTACHMENT_READ);
  B(SHADER_READ);
  B(SHADER_WRITE);
  B(COLOR_ATTACHMENT_READ);
  B(COLOR_ATTACHMENT_WRITE);
  B(DEPTH_STENCIL_ATTACHMENT_READ);
  B(DEPTH_STENCIL_ATTACHMENT_WRITE);
  B(TRANSFER_READ);
  B(TRANSFER_WRITE);
  B(HOST_READ);
  B(HOST_WRITE);
  B(MEMORY_READ);
  B(MEMORY_WRITE);
  B(SHADER_SAMPLED_READ);
  B(SHADER_STORAGE_READ);
  B(SHADER_STORAGE_WRITE);
  B_KHR(VIDEO_DECODE_READ);
  B_KHR(VIDEO_DECODE_WRITE);
  B_EXT(SAMPLER_HEAP_READ);
  B_EXT(RESOURCE_HEAP_READ);
  B_KHR(VIDEO_ENCODE_READ);
  B_KHR(VIDEO_ENCODE_WRITE);
  B_QCOM(SHADER_TILE_ATTACHMENT_READ);
  B_QCOM(SHADER_TILE_ATTACHMENT_WRITE);
  B_EXT(TRANSFORM_FEEDBACK_WRITE);
  B_EXT(TRANSFORM_FEEDBACK_COUNTER_READ);
  B_EXT(TRANSFORM_FEEDBACK_COUNTER_WRITE);
  B_EXT(CONDITIONAL_RENDERING_READ);
  B_NV(COMMAND_PREPROCESS_READ);
  B_NV(COMMAND_PREPROCESS_WRITE);
  B_EXT(COMMAND_PREPROCESS_READ);
  B_EXT(COMMAND_PREPROCESS_WRITE);
  B_KHR(FRAGMENT_SHADING_RATE_ATTACHMENT_READ);
  B_NV(SHADING_RATE_IMAGE_READ);
  B_KHR(ACCELERATION_STRUCTURE_READ);
  B_KHR(ACCELERATION_STRUCTURE_WRITE);
  B_NV(ACCELERATION_STRUCTURE_READ);
  B_NV(ACCELERATION_STRUCTURE_WRITE);
  B_EXT(FRAGMENT_DENSITY_MAP_READ);
  B_EXT(COLOR_ATTACHMENT_READ_NONCOHERENT);
  B_EXT(DESCRIPTOR_BUFFER_READ);
  B_HUAWEI(INVOCATION_MASK_READ);
  B_KHR(SHADER_BINDING_TABLE_READ);
  B_EXT(MICROMAP_READ);
  B_EXT(MICROMAP_WRITE);
  B_NV(OPTICAL_FLOW_READ);
  B_NV(OPTICAL_FLOW_WRITE);
  B_ARM(DATA_GRAPH_READ);
  B_ARM(DATA_GRAPH_WRITE);
  B_EXT(MEMORY_DECOMPRESSION_READ);
  B_EXT(MEMORY_DECOMPRESSION_WRITE);

#undef B
#undef B_KHR
#undef B_EXT
#undef B_NV
#undef B_QCOM
#undef B_ARM
#undef B_HUAWEI

  return fmt::format_to(ctx.out(), "{}", fmt::join(accessFlags, " | "));
}

fmt::format_context::iterator
fmt::formatter<kt::rdr::VkPipelineStageFlags2Formatter>::format(kt::rdr::VkPipelineStageFlags2Formatter formatter,
                                                                fmt::format_context& ctx) const {
  std::vector<std::string_view> stageFlags;

  auto f = formatter.flags;
  if (f == VK_PIPELINE_STAGE_2_NONE) {
    return fmt::format_to(ctx.out(), "{}", "NONE");
  }

#define B(x)                                                                                                                               \
  if (formatter.flags & VK_PIPELINE_STAGE_2_##x##_BIT)                                                                                     \
    stageFlags.emplace_back(#x);
#define B_KHR(x)                                                                                                                           \
  if (formatter.flags & VK_PIPELINE_STAGE_2_##x##_BIT_KHR)                                                                                 \
    stageFlags.emplace_back(#x "_KHR");
#define B_EXT(x)                                                                                                                           \
  if (formatter.flags & VK_PIPELINE_STAGE_2_##x##_BIT_EXT)                                                                                 \
    stageFlags.emplace_back(#x "_EXT");
#define B_NV(x)                                                                                                                            \
  if (formatter.flags & VK_PIPELINE_STAGE_2_##x##_BIT_NV)                                                                                  \
    stageFlags.emplace_back(#x "_NV");
#define B_ARM(x)                                                                                                                           \
  if (formatter.flags & VK_PIPELINE_STAGE_2_##x##_BIT_ARM)                                                                                 \
    stageFlags.emplace_back(#x "_ARM");
#define B_HUAWEI(x)                                                                                                                        \
  if (formatter.flags & VK_PIPELINE_STAGE_2_##x##_BIT_HUAWEI)                                                                              \
    stageFlags.emplace_back(#x "_HUAWEI");

  B(TOP_OF_PIPE)
  B(DRAW_INDIRECT)
  B(VERTEX_INPUT)
  B(VERTEX_SHADER)
  B(TESSELLATION_CONTROL_SHADER)
  B(TESSELLATION_EVALUATION_SHADER)
  B(GEOMETRY_SHADER)
  B(FRAGMENT_SHADER)
  B(EARLY_FRAGMENT_TESTS)
  B(LATE_FRAGMENT_TESTS)
  B(COLOR_ATTACHMENT_OUTPUT)
  B(COMPUTE_SHADER)
  B(ALL_TRANSFER)
  B(TRANSFER)
  B(BOTTOM_OF_PIPE)
  B(HOST)
  B(ALL_GRAPHICS)
  B(ALL_COMMANDS)
  B(COPY)
  B(RESOLVE)
  B(BLIT)
  B(CLEAR)
  B(INDEX_INPUT)
  B(VERTEX_ATTRIBUTE_INPUT)
  B(PRE_RASTERIZATION_SHADERS)
  B_KHR(VIDEO_DECODE)
  B_KHR(VIDEO_ENCODE)
  B_KHR(DRAW_INDIRECT)
  B_KHR(VERTEX_INPUT)
  B_KHR(VERTEX_SHADER)
  B_KHR(TESSELLATION_CONTROL_SHADER)
  B_KHR(TESSELLATION_EVALUATION_SHADER)
  B_KHR(GEOMETRY_SHADER)
  B_KHR(COMPUTE_SHADER)
  B_KHR(ALL_TRANSFER)
  B_KHR(TRANSFER)
  B_KHR(BOTTOM_OF_PIPE)
  B_KHR(HOST)
  B_KHR(ALL_GRAPHICS)
  B_KHR(ALL_COMMANDS)
  B_KHR(COPY)
  B_KHR(RESOLVE)
  B_KHR(BLIT)
  B_KHR(CLEAR)
  B_KHR(INDEX_INPUT)
  B_KHR(VERTEX_ATTRIBUTE_INPUT)
  B_KHR(PRE_RASTERIZATION_SHADERS)
  B_EXT(TRANSFORM_FEEDBACK)
  B_EXT(CONDITIONAL_RENDERING)
  B_NV(COMMAND_PREPROCESS)
  B_EXT(COMMAND_PREPROCESS)
  B_KHR(FRAGMENT_SHADING_RATE_ATTACHMENT)
  B_NV(SHADING_RATE_IMAGE)
  B_KHR(ACCELERATION_STRUCTURE_BUILD)
  B_KHR(RAY_TRACING_SHADER)
  B_NV(RAY_TRACING_SHADER)
  B_NV(ACCELERATION_STRUCTURE_BUILD)
  B_EXT(FRAGMENT_DENSITY_PROCESS)
  B_NV(TASK_SHADER)
  B_NV(MESH_SHADER)
  B_EXT(TASK_SHADER)
  B_EXT(MESH_SHADER)
  B_HUAWEI(SUBPASS_SHADER)
  B_HUAWEI(INVOCATION_MASK)
  B_KHR(ACCELERATION_STRUCTURE_COPY)
  B_EXT(MICROMAP_BUILD)
  B_HUAWEI(CLUSTER_CULLING_SHADER)
  B_NV(OPTICAL_FLOW)
  B_NV(CONVERT_COOPERATIVE_VECTOR_MATRIX)
  B_ARM(DATA_GRAPH)
  B_KHR(COPY_INDIRECT)
  B_EXT(MEMORY_DECOMPRESSION)

  return fmt::format_to(ctx.out(), "{}", fmt::join(stageFlags, " | "));
}
