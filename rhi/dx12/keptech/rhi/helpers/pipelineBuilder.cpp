#include <d3dcompiler.h> // Requires macros that we undefine in headers below

#include "keptech/rhi/pipelineBuilder.hpp"

#include "constants.hpp"
#include "d3dx12.h"
#include "dx-logger.hpp"
#include "macros.hpp"
#include "rhi.hpp"
#include "wrappers/pipeline.hpp"
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
      default:
        throw std::runtime_error("Unsupported data type for vertex input");
      }
    }
  } // namespace

  std::expected<Pipeline, std::string> PipelineBuilder::build() {
    DX_ASSERT(shader != nullptr, "Shader must be set before building pipeline");

    std::vector<ComPtr<ID3DBlob>> code;
    code.reserve(shader->code.size());
    for (auto& entryPointCode : shader->code) {
      D3DCreateBlob(entryPointCode.size(), code.emplace_back().GetAddressOf());
      memcpy(code.back()->GetBufferPointer(), entryPointCode.data(), entryPointCode.size());
    }

    std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout;
    for (const auto& [bufIdx, buffer] : shader->vertex.layout | std::views::enumerate) {
      for (const auto& entry : buffer.layout) {
        inputLayout.push_back({
            .SemanticName = entry.semantic.c_str(),
            .SemanticIndex = static_cast<UINT>(entry.semanticIndex),
            .Format = dxgiFormatFromDataType(entry.type),
            .InputSlot = static_cast<UINT>(bufIdx),
            .AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT,
            .InputSlotClass = buffer.inputRate == shaders::InputRate::Vertex ? D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA
                                                                             : D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,
            .InstanceDataStepRate = 1,
        });
      }
    }

    D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
        inputLayout.empty() ? D3D12_ROOT_SIGNATURE_FLAG_NONE : D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc = {};

    rootSignatureDesc.Init_1_1(0, nullptr, 0, nullptr, rootSignatureFlags);

    ComPtr<ID3DBlob> rootSignatureBlob;
    ComPtr<ID3DBlob> errorBlob;

    DX_MAKE(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, ROOT_SIGNATURE_VERSION, rootSignatureBlob.GetAddressOf(),
                                                  errorBlob.GetAddressOf()),
            "Failed to serialize root signature");

    Pipeline pipeline;

    DX_MAKE(RHI::get().getDevice()->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(), rootSignatureBlob->GetBufferSize(),
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
    pipelineStateStream.PrimitiveTopologyType = shader->vertex.topology == shaders::PrimitiveTopology::TriangleList
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

    DX_MAKE(RHI::get().getDevice()->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&pipeline.pipelineState)),
            "Failed to create pipeline state");

    return pipeline;
  }
} // namespace kt::rhi