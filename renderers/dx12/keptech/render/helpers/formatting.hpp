#pragma once

#include <spdlog/fmt/bundled/format.h>

template <> struct fmt::formatter<D3D12_RESOURCE_STATES> : fmt::formatter<std::string_view> {
  template <typename FormatContext> auto format(const D3D12_RESOURCE_STATES& layout, FormatContext& ctx) const {
    switch (layout) {
    case D3D12_RESOURCE_STATE_COMMON:
      return fmt::formatter<std::string_view>::format("COMMON", ctx);
    case D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE:
      return fmt::formatter<std::string_view>::format("PIXEL_SHADER_RESOURCE", ctx);
    case D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE:
      return fmt::formatter<std::string_view>::format("NON_PIXEL_SHADER_RESOURCE", ctx);
    case D3D12_RESOURCE_STATE_UNORDERED_ACCESS:
      return fmt::formatter<std::string_view>::format("UNORDERED_ACCESS", ctx);
    case D3D12_RESOURCE_STATE_RENDER_TARGET:
      return fmt::formatter<std::string_view>::format("RENDER_TARGET", ctx);
    case D3D12_RESOURCE_STATE_DEPTH_WRITE:
      return fmt::formatter<std::string_view>::format("DEPTH_WRITE", ctx);
    case D3D12_RESOURCE_STATE_DEPTH_READ:
      return fmt::formatter<std::string_view>::format("DEPTH_READ", ctx);
    case D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER:
      return fmt::formatter<std::string_view>::format("VERTEX_AND_CONSTANT_BUFFER", ctx);
    case D3D12_RESOURCE_STATE_INDEX_BUFFER:
      return fmt::formatter<std::string_view>::format("INDEX_BUFFER", ctx);
    case D3D12_RESOURCE_STATE_STREAM_OUT:
      return fmt::formatter<std::string_view>::format("STREAM_OUT", ctx);
    case D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT:
      return fmt::formatter<std::string_view>::format("INDIRECT_ARGUMENT", ctx);
    case D3D12_RESOURCE_STATE_COPY_DEST:
      return fmt::formatter<std::string_view>::format("COPY_DEST", ctx);
    case D3D12_RESOURCE_STATE_COPY_SOURCE:
      return fmt::formatter<std::string_view>::format("COPY_SOURCE", ctx);
    case D3D12_RESOURCE_STATE_RESOLVE_DEST:
      return fmt::formatter<std::string_view>::format("RESOLVE_DEST", ctx);
    case D3D12_RESOURCE_STATE_RESOLVE_SOURCE:
      return fmt::formatter<std::string_view>::format("RESOLVE_SOURCE", ctx);
    case D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE:
      return fmt::formatter<std::string_view>::format("RAYTRACING_ACCELERATION_STRUCTURE", ctx);
    case D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE:
      return fmt::formatter<std::string_view>::format("SHADING_RATE_SOURCE", ctx);
    case D3D12_RESOURCE_STATE_RESERVED_INTERNAL_8000:
      return fmt::formatter<std::string_view>::format("RESERVED_INTERNAL_8000", ctx);
    case D3D12_RESOURCE_STATE_RESERVED_INTERNAL_4000:
      return fmt::formatter<std::string_view>::format("RESERVED_INTERNAL_4000", ctx);
    case D3D12_RESOURCE_STATE_RESERVED_INTERNAL_100000:
      return fmt::formatter<std::string_view>::format("RESERVED_INTERNAL_100000", ctx);
    case D3D12_RESOURCE_STATE_RESERVED_INTERNAL_40000000:
      return fmt::formatter<std::string_view>::format("RESERVED_INTERNAL_40000000", ctx);
    case D3D12_RESOURCE_STATE_RESERVED_INTERNAL_80000000:
      return fmt::formatter<std::string_view>::format("RESERVED_INTERNAL_8000000₀", ctx);
    case D3D12_RESOURCE_STATE_GENERIC_READ:
      return fmt::formatter<std::string_view>::format("GENERIC_READ", ctx);
    case D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE:
      return fmt::formatter<std::string_view>::format("ALL_SHADER_RESOURCE", ctx);
    case D3D12_RESOURCE_STATE_VIDEO_DECODE_READ:
      return fmt::formatter<std::string_view>::format("VIDEO_DECODE_READ", ctx);
    case D3D12_RESOURCE_STATE_VIDEO_DECODE_WRITE:
      return fmt::formatter<std::string_view>::format("VIDEO_DECODE_WRITE", ctx);
    case D3D12_RESOURCE_STATE_VIDEO_PROCESS_READ:
      return fmt::formatter<std::string_view>::format("VIDEO_PROCESS_READ", ctx);
    case D3D12_RESOURCE_STATE_VIDEO_PROCESS_WRITE:
      return fmt::formatter<std::string_view>::format("VIDEO_PROCESS_WRITE", ctx);
    case D3D12_RESOURCE_STATE_VIDEO_ENCODE_READ:
      return fmt::formatter<std::string_view>::format("VIDEO_ENCODE_READ", ctx);
    case D3D12_RESOURCE_STATE_VIDEO_ENCODE_WRITE:
      return fmt::formatter<std::string_view>::format("VIDEO_ENCODE_WRITE", ctx);
      break;
    }
  }
};

template <> struct fmt::formatter<DXGI_FORMAT> : fmt::formatter<std::string_view> {
  template <typename FormatContext> auto format(const DXGI_FORMAT& format, FormatContext& ctx) const {

#define CASE_FMT(x)                                                                                                                        \
  case DXGI_FORMAT_##x:                                                                                                                    \
    return fmt::formatter<std::string_view>::format(#x, ctx);
    switch (format) {
      CASE_FMT(UNKNOWN)
      CASE_FMT(R8G8B8A8_UNORM)
      CASE_FMT(D32_FLOAT)
      CASE_FMT(R32G32B32A32_TYPELESS)
      CASE_FMT(R32G32B32A32_FLOAT)
      CASE_FMT(R32G32B32A32_UINT)
      CASE_FMT(R32G32B32A32_SINT)
      CASE_FMT(R32G32B32_TYPELESS)
      CASE_FMT(R32G32B32_FLOAT)
      CASE_FMT(R32G32B32_UINT)
      CASE_FMT(R32G32B32_SINT)
      CASE_FMT(R16G16B16A16_TYPELESS)
      CASE_FMT(R16G16B16A16_FLOAT)
      CASE_FMT(R16G16B16A16_UNORM)
      CASE_FMT(R16G16B16A16_UINT)
      CASE_FMT(R16G16B16A16_SNORM)
      CASE_FMT(R16G16B16A16_SINT)
      CASE_FMT(R32G32_TYPELESS)
      CASE_FMT(R32G32_FLOAT)
      CASE_FMT(R32G32_UINT)
      CASE_FMT(R32G32_SINT)
      CASE_FMT(R32G8X24_TYPELESS)
      CASE_FMT(D32_FLOAT_S8X24_UINT)
      CASE_FMT(R32_FLOAT_X8X24_TYPELESS)
      CASE_FMT(X32_TYPELESS_G8X24_UINT)
      CASE_FMT(R10G10B10A2_TYPELESS)
      CASE_FMT(R10G10B10A2_UNORM)
      CASE_FMT(R10G10B10A2_UINT)
      CASE_FMT(R11G11B10_FLOAT)
      CASE_FMT(R8G8B8A8_TYPELESS)
      CASE_FMT(R8G8B8A8_UNORM_SRGB)
      CASE_FMT(R8G8B8A8_UINT)
      CASE_FMT(R8G8B8A8_SNORM)
      CASE_FMT(R8G8B8A8_SINT)
      CASE_FMT(R16G16_TYPELESS)
      CASE_FMT(R16G16_FLOAT)
      CASE_FMT(R16G16_UNORM)
      CASE_FMT(R16G16_UINT)
      CASE_FMT(R16G16_SNORM)
      CASE_FMT(R16G16_SINT)
      CASE_FMT(R32_TYPELESS)
      CASE_FMT(R32_FLOAT)
      CASE_FMT(R32_UINT)
      CASE_FMT(R32_SINT)
      CASE_FMT(R24G8_TYPELESS)
      CASE_FMT(D24_UNORM_S8_UINT)
      CASE_FMT(R24_UNORM_X8_TYPELESS)
      CASE_FMT(X24_TYPELESS_G8_UINT)
      CASE_FMT(R8G8_TYPELESS)
      CASE_FMT(R8G8_UNORM)
      CASE_FMT(R8G8_UINT)
      CASE_FMT(R8G8_SNORM)
      CASE_FMT(R8G8_SINT)
      CASE_FMT(R16_TYPELESS)
      CASE_FMT(R16_FLOAT)
      CASE_FMT(D16_UNORM)
      CASE_FMT(R16_UNORM)
      CASE_FMT(R16_UINT)
      CASE_FMT(R16_SNORM)
      CASE_FMT(R16_SINT)
      CASE_FMT(R8_TYPELESS)
      CASE_FMT(R8_UNORM)
      CASE_FMT(R8_UINT)
      CASE_FMT(R8_SNORM)
      CASE_FMT(R8_SINT)
      CASE_FMT(A8_UNORM)
      CASE_FMT(R1_UNORM)
      CASE_FMT(R9G9B9E5_SHAREDEXP)
      CASE_FMT(R8G8_B8G8_UNORM)
      CASE_FMT(G8R8_G8B8_UNORM)
      CASE_FMT(BC1_TYPELESS)
      CASE_FMT(BC1_UNORM)
      CASE_FMT(BC1_UNORM_SRGB)
      CASE_FMT(BC2_TYPELESS)
      CASE_FMT(BC2_UNORM)
      CASE_FMT(BC2_UNORM_SRGB)
      CASE_FMT(BC3_TYPELESS)
      CASE_FMT(BC3_UNORM)
      CASE_FMT(BC3_UNORM_SRGB)
      CASE_FMT(BC4_TYPELESS)
      CASE_FMT(BC4_UNORM)
      CASE_FMT(BC4_SNORM)
      CASE_FMT(BC5_TYPELESS)
      CASE_FMT(BC5_UNORM)
      CASE_FMT(BC5_SNORM)
      CASE_FMT(B5G6R5_UNORM)
      CASE_FMT(B5G5R5A1_UNORM)
      CASE_FMT(B8G8R8A8_UNORM)
      CASE_FMT(B8G8R8X8_UNORM)
      CASE_FMT(R10G10B10_XR_BIAS_A2_UNORM)
      CASE_FMT(B8G8R8A8_TYPELESS)
      CASE_FMT(B8G8R8A8_UNORM_SRGB)
      CASE_FMT(B8G8R8X8_TYPELESS)
      CASE_FMT(B8G8R8X8_UNORM_SRGB)
      CASE_FMT(BC6H_TYPELESS)
      CASE_FMT(BC6H_UF16)
      CASE_FMT(BC6H_SF16)
      CASE_FMT(BC7_TYPELESS)
      CASE_FMT(BC7_UNORM)
      CASE_FMT(BC7_UNORM_SRGB)
      CASE_FMT(AYUV)
      CASE_FMT(Y410)
      CASE_FMT(Y416)
      CASE_FMT(NV12)
      CASE_FMT(P010)
      CASE_FMT(P016)
      CASE_FMT(420_OPAQUE)
      CASE_FMT(YUY2)
      CASE_FMT(Y210)
      CASE_FMT(Y216)
      CASE_FMT(NV11)
      CASE_FMT(AI44)
      CASE_FMT(IA44)
      CASE_FMT(P8)
      CASE_FMT(A8P8)
      CASE_FMT(B4G4R4A4_UNORM)
      CASE_FMT(P208)
      CASE_FMT(V208)
      CASE_FMT(V408)
      CASE_FMT(SAMPLER_FEEDBACK_MIN_MIP_OPAQUE)
      CASE_FMT(SAMPLER_FEEDBACK_MIP_REGION_USED_OPAQUE)
      CASE_FMT(A4B4G4R4_UNORM)
      CASE_FMT(FORCE_UINT)
    }

#undef CASE_FMT
  }
};