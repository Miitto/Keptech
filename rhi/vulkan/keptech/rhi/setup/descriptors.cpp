#include "constants.hpp"
#include "keptech/maths/maths.hpp"
#include "keptech/rhi/rhi.hpp"
#include "keptech/rhi/structs.hpp"
#include "macros.hpp"
#include <Volk/volk.h>
#include <array>
#include <keptech/components/camera.hpp>

namespace kt::rhi {

  enum ResourceBindingIndices : uint8_t {
    SAMPLER = 0,
    COMBINED_IMAGE_SAMPLER = 1,
    SAMPLED_IMAGE = 2,
    STORAGE_IMAGE = 3,
    UNIFORM_TEXEL_BUFFER = 4,
    STORAGE_TEXEL_BUFFER = 5,
    UNIFORM_BUFFER = 6,
    STORAGE_BUFFER = 7,
    MAX_BINDING_COUNT,
  };

  enum PerFrameUniformBufferIndices : uint8_t {
    CAMERA,
    ADDRESSES,
    MAX_PER_FRAME_UNIFORM_BUFFER_COUNT,
  };

  std::expected<void, std::string> RHI::initDescriptors() {

    std::array sizes{
        VkDescriptorPoolSize{
            .type = VkDescriptorType::VK_DESCRIPTOR_TYPE_SAMPLER,
            .descriptorCount = 1000 * MAX_FRAMES_IN_FLIGHT,
        },
        VkDescriptorPoolSize{
            .type = VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1000 * MAX_FRAMES_IN_FLIGHT,
        },
        VkDescriptorPoolSize{
            .type = VkDescriptorType::VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount = 1000 * MAX_FRAMES_IN_FLIGHT,
        },
        VkDescriptorPoolSize{
            .type = VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 1000 * MAX_FRAMES_IN_FLIGHT,
        },
        VkDescriptorPoolSize{
            .type = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
            .descriptorCount = 1000 * MAX_FRAMES_IN_FLIGHT,
        },
        VkDescriptorPoolSize{
            .type = VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
            .descriptorCount = 1000 * MAX_FRAMES_IN_FLIGHT,
        },
        VkDescriptorPoolSize{
            .type = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = (MAX_PER_FRAME_UNIFORM_BUFFER_COUNT + 1000) * MAX_FRAMES_IN_FLIGHT,
        },
        VkDescriptorPoolSize{
            .type = VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1000 * MAX_FRAMES_IN_FLIGHT,
        },
    };

    VkDescriptorPoolCreateInfo poolCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VkDescriptorPoolCreateFlagBits::VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT |
                 VkDescriptorPoolCreateFlagBits::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 2,
        .poolSizeCount = static_cast<uint32_t>(sizes.size()),
        .pPoolSizes = sizes.data(),
    };

    VK_MAKE(vkCreateDescriptorPool(m.vkcore.device, &poolCreateInfo, nullptr, &m.globalDescriptorSets.pool),
            "Failed to create bindless descriptor pool.");

    constexpr size_t descriptorBindingCount = 8;

    std::array<VkDescriptorSetLayoutBinding, descriptorBindingCount> bindings{
        VkDescriptorSetLayoutBinding{.binding = SAMPLER,
                                     .descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_SAMPLER,
                                     .descriptorCount = 1000,
                                     .stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_ALL},
        VkDescriptorSetLayoutBinding{.binding = COMBINED_IMAGE_SAMPLER,
                                     .descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                     .descriptorCount = 1000,
                                     .stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_ALL},
        VkDescriptorSetLayoutBinding{.binding = SAMPLED_IMAGE,
                                     .descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                     .descriptorCount = 1000,
                                     .stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_ALL},
        VkDescriptorSetLayoutBinding{.binding = STORAGE_IMAGE,
                                     .descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                     .descriptorCount = 1000,
                                     .stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_ALL},
        VkDescriptorSetLayoutBinding{.binding = UNIFORM_TEXEL_BUFFER,
                                     .descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
                                     .descriptorCount = 1000,
                                     .stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_ALL},
        VkDescriptorSetLayoutBinding{.binding = STORAGE_TEXEL_BUFFER,
                                     .descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
                                     .descriptorCount = 1000,
                                     .stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_ALL},
        VkDescriptorSetLayoutBinding{.binding = UNIFORM_BUFFER,
                                     .descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                     .descriptorCount = 1000,
                                     .stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_ALL},
        VkDescriptorSetLayoutBinding{.binding = STORAGE_BUFFER,
                                     .descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                     .descriptorCount = 1000,
                                     .stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_ALL}};

    std::array<VkDescriptorBindingFlags, descriptorBindingCount> bindingFlags{};
    for (auto& b : bindingFlags) {
      b = VkDescriptorBindingFlagBits::VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
          VkDescriptorBindingFlagBits::VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
    }

    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .bindingCount = descriptorBindingCount,
        .pBindingFlags = bindingFlags.data(),
    };

    VkDescriptorSetLayoutCreateInfo layoutCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &bindingFlagsInfo,
        .flags = VkDescriptorSetLayoutCreateFlagBits::VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
        .bindingCount = descriptorBindingCount,
        .pBindings = bindings.data(),
    };

    VK_MAKE(vkCreateDescriptorSetLayout(m.vkcore.device, &layoutCreateInfo, nullptr, &m.globalDescriptorSets.layout),
            "Failed to create bindless descriptor layout.");

    std::array<VkDescriptorSetLayout, MAX_FRAMES_IN_FLIGHT> layouts{{m.globalDescriptorSets.layout, m.globalDescriptorSets.layout}};

    VkDescriptorSetAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = m.globalDescriptorSets.pool,
        .descriptorSetCount = 2,
        .pSetLayouts = layouts.data(),
    };

    VK_MAKE(vkAllocateDescriptorSets(m.vkcore.device, &allocInfo, m.globalDescriptorSets.sets.data()),
            "Failed to allocate bindless descriptor set.");

    return {};
  }

  void RHI::writeDescriptors() {
    const auto& vkcore = m.vkcore;
    const auto& buffers = m.buffers;

    size_t camSize = maths::roundToAlignment(sizeof(components::Camera::Uniforms), limits::minUniformBufferOffsetAlignment);
    size_t addressesSize = maths::roundToAlignment(sizeof(BufferPointers), limits::minUniformBufferOffsetAlignment);

    constexpr size_t BUFFER_COUNT = 9;
    constexpr size_t TOTAL_BUFFER_WRITES = MAX_FRAMES_IN_FLIGHT * BUFFER_COUNT;

    std::array<VkDescriptorBufferInfo, TOTAL_BUFFER_WRITES> bufferInfos{};
    std::array<VkWriteDescriptorSet, MAX_FRAMES_IN_FLIGHT * 2> writes{};
    for (size_t writeIdx = 0; writeIdx < MAX_FRAMES_IN_FLIGHT; ++writeIdx) {
      auto bufferIdx = writeIdx * BUFFER_COUNT;
      bufferInfos[bufferIdx] = VkDescriptorBufferInfo{
          .buffer = buffers.camera,
          .offset = writeIdx * camSize,
          .range = sizeof(components::Camera::Uniforms),
      };
      bufferInfos[bufferIdx + 1] = VkDescriptorBufferInfo{
          .buffer = buffers.positions,
          .offset = writeIdx * addressesSize,
          .range = VK_WHOLE_SIZE,
      };
      bufferInfos[bufferIdx + 2] = VkDescriptorBufferInfo{
          .buffer = buffers.vertexAttribs,
          .offset = 0,
          .range = VK_WHOLE_SIZE,
      };
      bufferInfos[bufferIdx + 3] = VkDescriptorBufferInfo{
          .buffer = buffers.indices,
          .offset = 0,
          .range = VK_WHOLE_SIZE,
      };
      bufferInfos[bufferIdx + 4] = VkDescriptorBufferInfo{
          .buffer = buffers.meshlets,
          .offset = 0,
          .range = VK_WHOLE_SIZE,
      };
      bufferInfos[bufferIdx + 5] = VkDescriptorBufferInfo{
          .buffer = buffers.meshletVertices,
          .offset = 0,
          .range = VK_WHOLE_SIZE,
      };
      bufferInfos[bufferIdx + 6] = VkDescriptorBufferInfo{
          .buffer = buffers.meshletTriangles,
          .offset = 0,
          .range = VK_WHOLE_SIZE,
      };
      bufferInfos[bufferIdx + 7] = VkDescriptorBufferInfo{
          .buffer = buffers.materials,
          .offset = 0,
          .range = VK_WHOLE_SIZE,
      };
      bufferInfos[bufferIdx + 8] = VkDescriptorBufferInfo{
          .buffer = buffers.meshes,
          .offset = 0,
          .range = VK_WHOLE_SIZE,
      };
      writes[writeIdx * MAX_FRAMES_IN_FLIGHT] = VkWriteDescriptorSet{
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = m.globalDescriptorSets.sets[writeIdx * MAX_FRAMES_IN_FLIGHT],
          .dstBinding = UNIFORM_BUFFER,
          .dstArrayElement = 0,
          .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
          .pBufferInfo = &bufferInfos[bufferIdx],
      };
      writes[writeIdx * MAX_FRAMES_IN_FLIGHT + 1] = VkWriteDescriptorSet{
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = m.globalDescriptorSets.sets[writeIdx * MAX_FRAMES_IN_FLIGHT],
          .dstBinding = STORAGE_BUFFER,
          .dstArrayElement = 0,
          .descriptorCount = BUFFER_COUNT - 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .pBufferInfo = &bufferInfos[bufferIdx + 1],
      };
    }

    vkUpdateDescriptorSets(vkcore.device, writes.size(), writes.data(), 0, nullptr);

    m.indices.nextUniformBufferIndex = 1 m.indices.nextStorageBufferIndex = BUFFER_COUNT - 1;
  }
} // namespace kt::rhi
