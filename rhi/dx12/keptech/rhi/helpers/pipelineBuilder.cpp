#include <d3d12.h>
#include <d3dcompiler.h> // Requires macros that we undefine in headers below

#include "keptech/rhi/pipelineBuilder.hpp"

#include "d3dx12.h"
#include "dx/constants.hpp"
#include "dx/dx-logger.hpp"
#include "dx/macros.hpp"
#include "pipeline.hpp"
#include "rhi.hpp"
#include <ranges>
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

    size_t rootParameterCount = shader->info.resources.size();
    if (shader->info.pushConstantSize > 0) {
      DX_ASSERT(shader->info.pushConstantSize % sizeof(uint32_t) == 0,
                "Push constant size must be a multiple of 4 bytes (32 bits) for DX12");
      rootParameterCount += 1;
    }

    std::vector<CD3DX12_ROOT_PARAMETER1> rootParameters(rootParameterCount);
    uint32_t maxConstantBinding = 1;
    for (size_t i = 0; i < shader->info.resources.size(); ++i) {
      auto& resource = shader->info.resources[i];
      switch (resource.type) {
      case shaders::ShaderResourceType::UniformBuffer:
        rootParameters[i].InitAsConstantBufferView(resource.binding, resource.set);
        maxConstantBinding = std::max(maxConstantBinding, resource.binding + 2);
        break;
      case shaders::ShaderResourceType::StorageBuffer:
        rootParameters[i].InitAsShaderResourceView(resource.binding, resource.set);
        break;
      default:
        DX_ABORT("Unsupported resource type for root signature: {}", static_cast<uint32_t>(resource.type));
      }
    }

    if (shader->info.pushConstantSize > 0) {
      rootParameters.back().InitAsConstants(static_cast<UINT>(shader->info.pushConstantSize / sizeof(uint32_t)), maxConstantBinding - 1, 0);
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

    struct PipelineStateStream {
      CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
      CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
      CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
      CD3DX12_PIPELINE_STATE_STREAM_VS VS;
      CD3DX12_PIPELINE_STATE_STREAM_PS PS;
      CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
      CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
    } pipelineStateStream;

    pipelineStateStream.pRootSignature = pipeline.rootSignature.Get();
    pipelineStateStream.InputLayout = {.pInputElementDescs = inputLayout.data(), .NumElements = static_cast<UINT>(inputLayout.size())};
    pipelineStateStream.PrimitiveTopologyType = shader->info.vertex.topology == shaders::PrimitiveTopology::TriangleList
                                                    ? D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE
                                                    : D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
    // FIXME: Shader order is not guaranteed to be vertex first, fragment second. We should sort by stage instead of relying on order.
    pipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(code[0].Get());
    pipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(code[1].Get());
    pipelineStateStream.DSVFormat = raw(depthAttachmentFormat);
    pipelineStateStream.RTVFormats = rtvFormats;

    D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
        .SizeInBytes = sizeof(PipelineStateStream),
        .pPipelineStateSubobjectStream = &pipelineStateStream,
    };

    DX_MAKE(RHI::get().dxGetDevice()->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&pipeline.pipelineState)),
            "Failed to create pipeline state");

    return pipeline;
  }
} // namespace kt::rhi