#pragma once

#include "keptech/core/bitflag.hpp"

#ifdef KT_VULKAN
#include <Volk/volk.h>
#define F(vk, dx12) vk // NOLINT
#define VKF(vk) vk     // NOLINT
#else
#include <dxgiformat.h>
#define F(vk, dx12) dx12 // NOLINT
#define VKF(vk) 0        // NOLINT
#endif

namespace kt::rhi {
  enum class PipelineStageFlags : uint32_t {
    TopOfPipe = F(VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0),
    DrawIndirect = F(VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT, 0),
    VertexInput = F(VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT, 0),
    VertexShader = F(VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT, 0),
    GeometryShader = F(VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT, 0),
    FragmentShader = F(VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, 0),
    EarlyFragmentTests = F(VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT, 0),
    LateFragmentTests = F(VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT, 0),
    ColorAttachmentOutput = F(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 0),
    ComputeShader = F(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, 0),
    AllTransfer = F(VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, 0),
    Transfer = F(VK_PIPELINE_STAGE_2_TRANSFER_BIT, 0),
    Copy = F(VK_PIPELINE_STAGE_2_COPY_BIT, 0),
    TaskShader = F(VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_EXT, 0),
    MeshShader = F(VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT, 0),
    IndexInput = F(VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT, 0),
    Blit = F(VK_PIPELINE_STAGE_2_BLIT_BIT, 0),
    BottomOfPipe = F(VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, 0),
  };
}

DEFINE_BITFLAG_ENUM_OPERATORS(kt::rhi::PipelineStageFlags)