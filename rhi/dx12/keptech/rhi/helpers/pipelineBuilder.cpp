#include <algorithm>
#include <d3d12.h>
#include <d3dcompiler.h> // Requires macros that we undefine in headers below

#include "keptech/rhi/pipelineBuilder.hpp"

#include "d3dx12.h"
#include "dx/constants.hpp"
#include "dx/dx-logger.hpp"
#include "dx/macros.hpp"
#include "keptech/shaders/resources.hpp"
#include "pipeline.hpp"
#include "rhi.hpp"
#include <ranges>
#include <vector>
#include <wrl.h>

template <typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

namespace kt::rhi {
  namespace {
    DXGI_FORMAT dxgiFormatFromDataType(shaders::DataType type) {
      switch (type) {
      case shaders::DataType::F32:
        return DXGI_FORMAT_R32_FLOAT;
      case shaders::DataType::F32_2:
        return DXGI_FORMAT_R32G32_FLOAT;
      case shaders::DataType::F32_3:
        return DXGI_FORMAT_R32G32B32_FLOAT;
      case shaders::DataType::F32_4:
        return DXGI_FORMAT_R32G32B32A32_FLOAT;
      case shaders::DataType::I32:
        return DXGI_FORMAT_R32_SINT;
      case shaders::DataType::I32_2:
        return DXGI_FORMAT_R32G32_SINT;
      case shaders::DataType::I32_3:
        return DXGI_FORMAT_R32G32B32_SINT;
      case shaders::DataType::I32_4:
        return DXGI_FORMAT_R32G32B32A32_SINT;
      case shaders::DataType::U32:
        return DXGI_FORMAT_R32_UINT;
      case shaders::DataType::U32_2:
        return DXGI_FORMAT_R32G32_UINT;
      case shaders::DataType::U32_3:
        return DXGI_FORMAT_R32G32B32_UINT;
      case shaders::DataType::U32_4:
        return DXGI_FORMAT_R32G32B32A32_UINT;
      case shaders::DataType::F16_2:
        return DXGI_FORMAT_R16G16_FLOAT;
      default:
        DX_ABORT("Unsupported data type for DXGI format conversion: {}", static_cast<uint32_t>(type));
      }
    }
  } // namespace

  std::expected<Pipeline, std::string> PipelineBuilder::build() {
    DX_ASSERT(shader != nullptr, "Shader must be set before building pipeline");

    Pipeline pipeline;
    pipeline.info = shader->info;

    std::vector<ComPtr<ID3DBlob>> code;
    code.reserve(shader->code.size());
    for (auto& entryPointCode : shader->code) {
      D3DCreateBlob(entryPointCode.size(), code.emplace_back().GetAddressOf());
      memcpy(code.back()->GetBufferPointer(), entryPointCode.data(), entryPointCode.size());
    }

    std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout;
    for (const auto& [bufIdx, buffer] : shader->info.vertex.layout | std::views::enumerate) {
      for (const auto& entry : buffer.layout) {
        inputLayout.push_back({
            .SemanticName = entry.semantic.c_str(),
            .SemanticIndex = static_cast<UINT>(entry.semanticIndex),
            .Format = dxgiFormatFromDataType(entry.type),
            .InputSlot = static_cast<UINT>(bufIdx),
            .AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT,
            .InputSlotClass = buffer.inputRate == shaders::InputRate::Vertex ? D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA
                                                                             : D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,
            .InstanceDataStepRate = buffer.inputRate == shaders::InputRate::Vertex ? 0u : 1u,
        });
      }
    }

    D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
        (inputLayout.empty() ? D3D12_ROOT_SIGNATURE_FLAG_NONE : D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT) |
        D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED | D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED;

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc = {};

    int32_t maxCbvBinding = -1;

    auto rangeTypeFromResourceType = [](shaders::ShaderResourceType type) {
      switch (type) {
      case shaders::ShaderResourceType::UniformBuffer:
        return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
      case shaders::ShaderResourceType::StorageBuffer:
      case shaders::ShaderResourceType::Texture1D:
      case shaders::ShaderResourceType::Texture1DArray:
      case shaders::ShaderResourceType::Texture2D:
      case shaders::ShaderResourceType::Texture2DArray:
      case shaders::ShaderResourceType::Texture3D:
      case shaders::ShaderResourceType::Texture3DArray:
      case shaders::ShaderResourceType::TextureCube:
        return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
      case shaders::ShaderResourceType::RWStorageBuffer:
        return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
      case shaders::ShaderResourceType::Sampler:
        return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
      }
    };

    std::vector<D3D12_DESCRIPTOR_RANGE1> ranges;

    struct BindingSpace {
      uint32_t binding;
      uint32_t space;
    };

    std::vector<BindingSpace> directCbvs;
    std::vector<BindingSpace> directSrvs;
    std::vector<BindingSpace> directUavs;
    std::vector<BindingSpace> directSamplers;

    if (!shader->info.globalResources.sets.empty()) {
      std::vector<D3D12_DESCRIPTOR_RANGE1> cbvRanges;
      std::vector<D3D12_DESCRIPTOR_RANGE1> srvRanges;
      std::vector<D3D12_DESCRIPTOR_RANGE1> uavRanges;

      for (uint32_t space = 0; space < shader->info.globalResources.sets.size(); ++space) {
        const auto& resourceSet = shader->info.globalResources.sets[space];
        for (const auto& resource : resourceSet.resources) {
          auto rangeType = rangeTypeFromResourceType(resource.type);

          if (resource.isPush) {
            switch (rangeType) {
            case D3D12_DESCRIPTOR_RANGE_TYPE_CBV:
              maxCbvBinding = std::max(maxCbvBinding, static_cast<int32_t>(resource.binding));
              directCbvs.push_back({resource.binding, space});
              break;
            case D3D12_DESCRIPTOR_RANGE_TYPE_SRV:
              DX_ASSERT(resource.type == shaders::ShaderResourceType::StorageBuffer,
                        "Only storage buffers can be directly bound as SRVs in DX12");
              directSrvs.push_back({resource.binding, space});
              break;
            case D3D12_DESCRIPTOR_RANGE_TYPE_UAV:
              directUavs.push_back({resource.binding, space});
              break;
            case D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER:
              DX_ABORT("Directly bound samplers are not supported in DX12");
              break;
            }
            continue;
          }

          switch (rangeType) {
          case D3D12_DESCRIPTOR_RANGE_TYPE_CBV: {
            if (cbvRanges.empty() || (resource.binding != cbvRanges.back().BaseShaderRegister + cbvRanges.back().NumDescriptors ||
                                      space != cbvRanges.back().RegisterSpace)) {
              cbvRanges.push_back({.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
                                   .NumDescriptors = 0,
                                   .BaseShaderRegister = resource.binding,
                                   .RegisterSpace = space,
                                   .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND});
            }
            cbvRanges.back().NumDescriptors += resource.count;
            maxCbvBinding = std::max(maxCbvBinding, static_cast<int32_t>(resource.binding));
          } break;
          case D3D12_DESCRIPTOR_RANGE_TYPE_SRV: {
            if (srvRanges.empty() || (resource.binding != srvRanges.back().BaseShaderRegister + srvRanges.back().NumDescriptors ||
                                      space != srvRanges.back().RegisterSpace)) {
              srvRanges.push_back({.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
                                   .NumDescriptors = 0,
                                   .BaseShaderRegister = resource.binding,
                                   .RegisterSpace = space,
                                   .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND});
            }
            srvRanges.back().NumDescriptors += resource.count;
          } break;
          case D3D12_DESCRIPTOR_RANGE_TYPE_UAV: {
            if (uavRanges.empty() || (resource.binding != uavRanges.back().BaseShaderRegister + uavRanges.back().NumDescriptors ||
                                      space != uavRanges.back().RegisterSpace)) {
              uavRanges.push_back({.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
                                   .NumDescriptors = 0,
                                   .BaseShaderRegister = resource.binding,
                                   .RegisterSpace = space,
                                   .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND});
            }
            uavRanges.back().NumDescriptors += resource.count;
          } break;
          case D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER:
            DX_ABORT("Samplers in DX12 are only supported via bindless indexing.");
            break;
          }
        }
      }

      ranges.append_range(cbvRanges);
      ranges.append_range(srvRanges);
      ranges.append_range(uavRanges);
    }

    size_t tableOffset = ranges.empty() ? 0 : 1;

    DX_DEBUG("Building {} pipeline with:", shader->info.name);
    if (!ranges.empty()) {
      DX_DEBUG("  Table:");
      for (const auto& range : ranges) {
        switch (range.RangeType) {
        case D3D12_DESCRIPTOR_RANGE_TYPE_CBV:
          DX_DEBUG("    {}-{} CBV", range.BaseShaderRegister, range.BaseShaderRegister + range.NumDescriptors - 1);
          break;
        case D3D12_DESCRIPTOR_RANGE_TYPE_SRV:
          DX_DEBUG("    {}-{} SRV", range.BaseShaderRegister, range.BaseShaderRegister + range.NumDescriptors - 1);
          break;
        case D3D12_DESCRIPTOR_RANGE_TYPE_UAV:
          DX_DEBUG("    {}-{} UAV", range.BaseShaderRegister, range.BaseShaderRegister + range.NumDescriptors - 1);
          break;
        }
      }
    }

    std::vector<CD3DX12_ROOT_PARAMETER1> rootParameters{tableOffset + directCbvs.size() + directSrvs.size() + directUavs.size() +
                                                        directSamplers.size() +
                                                        (shader->info.globalResources.pushConstants.size > 0u ? 1u : 0u)};

    if (!ranges.empty()) {
      rootParameters[0].InitAsDescriptorTable(static_cast<UINT>(ranges.size()), ranges.data(), D3D12_SHADER_VISIBILITY_ALL);
    }

    for (size_t i = 0; i < directCbvs.size(); ++i) {
      rootParameters[tableOffset + i].InitAsConstantBufferView(directCbvs[i].binding, directCbvs[i].space);
    }
    uint32_t offset = static_cast<uint32_t>(tableOffset + directCbvs.size());
    for (size_t i = 0; i < directSrvs.size(); ++i) {
      rootParameters[offset + i].InitAsShaderResourceView(directSrvs[i].binding, directSrvs[i].space);
    }
    offset += static_cast<uint32_t>(directSrvs.size());
    for (size_t i = 0; i < directUavs.size(); ++i) {
      rootParameters[offset + i].InitAsUnorderedAccessView(directUavs[i].binding, directUavs[i].space);
    }

    if (shader->info.globalResources.pushConstants.size > 0) {
      rootParameters.back().InitAsConstants(static_cast<UINT>(shader->info.globalResources.pushConstants.size / sizeof(uint32_t)), 0, 0);
      pipeline.constantSlot = static_cast<uint32_t>(rootParameters.size() - 1);
    }

    rootSignatureDesc.Init_1_1(static_cast<UINT>(rootParameters.size()), rootParameters.data(), 0, nullptr, rootSignatureFlags);

    ComPtr<ID3DBlob> rootSignatureBlob;
    ComPtr<ID3DBlob> errorBlob;

    auto res = D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, ROOT_SIGNATURE_VERSION, rootSignatureBlob.GetAddressOf(),
                                                     errorBlob.GetAddressOf());
    if (FAILED(res)) {
      if (errorBlob) {
        DX_ERROR("Failed to serialize root signature: {}", static_cast<const char*>(errorBlob->GetBufferPointer()));
      } else {
        _com_error e(res);
        DX_ERROR("Failed to serialize root signature: {}", e.ErrorMessage());
      }
      return std::unexpected("Failed to serialize root signature");
    }

    DX_MAKE(RHI::get().dxGetDevice()->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(), rootSignatureBlob->GetBufferSize(),
                                                          IID_PPV_ARGS(&pipeline.rootSignature)),
            "Failed to create root signature");

    D3D12_RT_FORMAT_ARRAY rtvFormats{};
    rtvFormats.NumRenderTargets = static_cast<UINT>(colorAttachmentFormats.size());
    DX_ASSERT(colorAttachmentFormats.size() <= D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT, "Too many color attachment formats");
    for (size_t i = 0; i < colorAttachmentFormats.size(); ++i) {
      rtvFormats.RTFormats[i] = raw(colorAttachmentFormats[i]);
    }

    auto setPrimitiveTopologyInfo = [&](CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY& i) {
      i = shader->info.vertex.topology == shaders::PrimitiveTopology::TriangleList
              ? D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE
              : D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE; // TODO: Properly map all topologies.
    };
    auto setRasterizerInfo = [&](CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER& i) {
      CD3DX12_RASTERIZER_DESC raster{D3D12_DEFAULT};
      raster.FrontCounterClockwise =
          TRUE; // TODO: Properly handle front face winding from shader attributes. Engine loads models CCW though so fine for now.
      switch (shader->info.vertex.cullMode) {
      case shaders::CullMode::None:
        raster.CullMode = D3D12_CULL_MODE_NONE;
        break;
      case shaders::CullMode::Front:
        raster.CullMode = D3D12_CULL_MODE_FRONT;
        break;
      case shaders::CullMode::Back:
        raster.CullMode = D3D12_CULL_MODE_BACK;
        break;
      case shaders::CullMode::FrontAndBack:
        DX_ABORT("DX12 does not support culling front and back faces");
      }

      i = raster;
    };

    auto setBlendInfo = [&](CD3DX12_PIPELINE_STATE_STREAM_BLEND_DESC& i) {
      CD3DX12_BLEND_DESC blend{D3D12_DEFAULT}; // TODO: Properly handle blending state fro shader attributes.
      i = blend;
    };

    auto setDepthStencilInfo = [&](CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL& i) {
      CD3DX12_DEPTH_STENCIL_DESC depthStencil{D3D12_DEFAULT}; // TODO: Properly handle depth/stencil state from shader attributes.
      if (!shader->info.fragment.depthWrite) {
        depthStencil.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
      }
      if (depthAttachmentFormat == ImageFormat::Undefined) {
        depthStencil.DepthEnable = FALSE;
      }
      i = depthStencil;
    };

    Bitflag<shaders::ShaderStages> stages{};
    for (const auto& stage : shader->stages) {
      stages |= stage.stage;
    }

#define MAKE_PSO(_T, _PTR)                                                                                                                 \
  D3D12_PIPELINE_STATE_STREAM_DESC psoDesc{                                                                                                \
      .SizeInBytes = sizeof(_T),                                                                                                           \
      .pPipelineStateSubobjectStream = _PTR,                                                                                               \
  };                                                                                                                                       \
  DX_MAKE(RHI::get().dxGetDevice()->CreatePipelineState(&psoDesc, IID_PPV_ARGS(&pipeline.pipelineState)),                                  \
          "Failed to create pipeline state");

    constexpr uint8_t vertFragmentMask = (Bitflag(shaders::ShaderStages::Vertex) | shaders::ShaderStages::Fragment).as_underlying();
    constexpr uint8_t vertGeomFragmentMask =
        (Bitflag(shaders::ShaderStages::Vertex) | shaders::ShaderStages::Geometry | shaders::ShaderStages::Fragment).as_underlying();
    constexpr uint8_t meshFragmentMask = (Bitflag(shaders::ShaderStages::Mesh) | shaders::ShaderStages::Fragment).as_underlying();
    constexpr uint8_t taskMeshFragmentMask =
        (Bitflag(shaders::ShaderStages::Task) | shaders::ShaderStages::Mesh | shaders::ShaderStages::Fragment).as_underlying();
    constexpr uint8_t computeMask = (Bitflag(shaders::ShaderStages::Compute)).as_underlying();

    using CD3DX12_PIPELINE_STATE_STREAM_MS =
        CD3DX12_PIPELINE_STATE_STREAM_SUBOBJECT<D3D12_SHADER_BYTECODE, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS>;
    using CD3DX12_PIPELINE_STATE_STREAM_AS =
        CD3DX12_PIPELINE_STATE_STREAM_SUBOBJECT<D3D12_SHADER_BYTECODE, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS>;

    // TODO: There is probably a much better way of doing this....
    switch (stages.as_underlying()) {
    case vertFragmentMask: {
      struct PipelineStateStream {
        CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
        CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
        CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
        CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER Rasterizer;
        CD3DX12_PIPELINE_STATE_STREAM_BLEND_DESC Blend;
        CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL DepthStencil;
        CD3DX12_PIPELINE_STATE_STREAM_VS VS;
        CD3DX12_PIPELINE_STATE_STREAM_PS PS;
        CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
        CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
      } pss;
      pss.pRootSignature = pipeline.rootSignature.Get();
      pss.InputLayout = {inputLayout.data(), static_cast<UINT>(inputLayout.size())};
      setPrimitiveTopologyInfo(pss.PrimitiveTopologyType);
      setRasterizerInfo(pss.Rasterizer);
      setBlendInfo(pss.Blend);
      setDepthStencilInfo(pss.DepthStencil);
      for (size_t i = 0; i < shader->stages.size(); ++i) {
        switch (shader->stages[i].stage) {
        case shaders::ShaderStages::Vertex:
          pss.VS = CD3DX12_SHADER_BYTECODE(code[i].Get());
          break;
        case shaders::ShaderStages::Fragment:
          pss.PS = CD3DX12_SHADER_BYTECODE(code[i].Get());
          break;
        default:
          DX_ABORT("Unsupported shader stage for pipeline creation: {}", static_cast<uint32_t>(shader->stages[i].stage));
        }
      }
      pss.DSVFormat = raw(depthAttachmentFormat);
      pss.RTVFormats = rtvFormats;
      MAKE_PSO(PipelineStateStream, &pss);
    } break;
    case vertGeomFragmentMask: {
      struct PipelineStateStream {
        CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
        CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
        CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
        CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER Rasterizer;
        CD3DX12_PIPELINE_STATE_STREAM_BLEND_DESC Blend;
        CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL DepthStencil;
        CD3DX12_PIPELINE_STATE_STREAM_VS VS;
        CD3DX12_PIPELINE_STATE_STREAM_GS GS;
        CD3DX12_PIPELINE_STATE_STREAM_PS PS;
        CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
        CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
      } pss;
      pss.pRootSignature = pipeline.rootSignature.Get();
      pss.InputLayout = {inputLayout.data(), static_cast<UINT>(inputLayout.size())};
      setPrimitiveTopologyInfo(pss.PrimitiveTopologyType);
      setRasterizerInfo(pss.Rasterizer);
      setBlendInfo(pss.Blend);
      setDepthStencilInfo(pss.DepthStencil);
      for (size_t i = 0; i < shader->stages.size(); ++i) {
        switch (shader->stages[i].stage) {
        case shaders::ShaderStages::Vertex:
          pss.VS = CD3DX12_SHADER_BYTECODE(code[i].Get());
          break;
        case shaders::ShaderStages::Geometry:
          pss.GS = CD3DX12_SHADER_BYTECODE(code[i].Get());
          break;
        case shaders::ShaderStages::Fragment:
          pss.PS = CD3DX12_SHADER_BYTECODE(code[i].Get());
          break;
        default:
          DX_ABORT("Unsupported shader stage for pipeline creation: {}", static_cast<uint32_t>(shader->stages[i].stage));
        }
      }
      pss.DSVFormat = raw(depthAttachmentFormat);
      pss.RTVFormats = rtvFormats;
      MAKE_PSO(PipelineStateStream, &pss);
    } break;
    case meshFragmentMask: {

      struct PipelineStateStream {
        CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
        CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
        CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER Rasterizer;
        CD3DX12_PIPELINE_STATE_STREAM_BLEND_DESC Blend;
        CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL DepthStencil;
        CD3DX12_PIPELINE_STATE_STREAM_MS MS;
        CD3DX12_PIPELINE_STATE_STREAM_PS PS;
        CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
        CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
      } pss;
      pss.pRootSignature = pipeline.rootSignature.Get();
      setPrimitiveTopologyInfo(pss.PrimitiveTopologyType);
      setRasterizerInfo(pss.Rasterizer);
      setBlendInfo(pss.Blend);
      setDepthStencilInfo(pss.DepthStencil);
      for (size_t i = 0; i < shader->stages.size(); ++i) {
        switch (shader->stages[i].stage) {
        case shaders::ShaderStages::Mesh:
          pss.MS = CD3DX12_SHADER_BYTECODE(code[i].Get());
          break;
        case shaders::ShaderStages::Fragment:
          pss.PS = CD3DX12_SHADER_BYTECODE(code[i].Get());
          break;
        default:
          DX_ABORT("Unsupported shader stage for pipeline creation: {}", static_cast<uint32_t>(shader->stages[i].stage));
        }
      }
      pss.DSVFormat = raw(depthAttachmentFormat);
      pss.RTVFormats = rtvFormats;
      MAKE_PSO(PipelineStateStream, &pss);
    } break;
    case taskMeshFragmentMask: {
      struct PipelineStateStream {
        CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
        CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
        CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER Rasterizer;
        CD3DX12_PIPELINE_STATE_STREAM_BLEND_DESC Blend;
        CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL DepthStencil;
        CD3DX12_PIPELINE_STATE_STREAM_AS AS;
        CD3DX12_PIPELINE_STATE_STREAM_MS MS;
        CD3DX12_PIPELINE_STATE_STREAM_PS PS;
        CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
        CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
      } pss;
      pss.pRootSignature = pipeline.rootSignature.Get();
      setPrimitiveTopologyInfo(pss.PrimitiveTopologyType);
      setRasterizerInfo(pss.Rasterizer);
      setBlendInfo(pss.Blend);
      setDepthStencilInfo(pss.DepthStencil);
      for (size_t i = 0; i < shader->stages.size(); ++i) {
        switch (shader->stages[i].stage) {
        case shaders::ShaderStages::Task:
          pss.AS = CD3DX12_SHADER_BYTECODE(code[i].Get());
          break;
        case shaders::ShaderStages::Mesh:
          pss.MS = CD3DX12_SHADER_BYTECODE(code[i].Get());
          break;
        case shaders::ShaderStages::Fragment:
          pss.PS = CD3DX12_SHADER_BYTECODE(code[i].Get());
          break;
        default:
          DX_ABORT("Unsupported shader stage for pipeline creation: {}", static_cast<uint32_t>(shader->stages[i].stage));
        }
      }
      pss.DSVFormat = raw(depthAttachmentFormat);
      pss.RTVFormats = rtvFormats;
      MAKE_PSO(PipelineStateStream, &pss);
    } break;
    case computeMask: {
      struct PipelineStateStream {
        CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
        CD3DX12_PIPELINE_STATE_STREAM_CS CS;
      } pss;
      pss.pRootSignature = pipeline.rootSignature.Get();
      DX_ASSERT(shader->stages.size() == 1 && shader->stages[0].stage == shaders::ShaderStages::Compute,
                "Compute pipeline must have exactly one compute shader stage");
      pss.CS = CD3DX12_SHADER_BYTECODE(code[0].Get());
      MAKE_PSO(PipelineStateStream, &pss);
    } break;
    default:
      DX_CRITICAL("Unsupported shader stage combination for pipeline creation. Stages:");
      for (const auto& stage : shader->stages) {
        DX_CRITICAL("  {}", stage.stage);
      }
      DX_ABORT("Unsupported shader stage combination for pipeline creation");
    }

    return pipeline;
  }
} // namespace kt::rhi