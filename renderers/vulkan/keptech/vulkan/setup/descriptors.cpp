#include "constants.hpp"
#include "keptech/maths/maths.hpp"
#include "keptech/vulkan/renderer.hpp"
#include "keptech/vulkan/structs.hpp"
#include "macros.hpp"
#include <Volk/volk.h>
#include <array>
#include <keptech/components/camera.hpp>

namespace kt::vkh::setup {

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

  std::expected<DescriptorPoolSet<MAX_FRAMES_IN_FLIGHT>, std::string> createGlobalDescriptors(VkDevice device) {

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

    VkDescriptorPool descriptorPool{};
    VK_MAKE(vkCreateDescriptorPool(device, &poolCreateInfo, nullptr, &descriptorPool), "Failed to create bindless descriptor pool.");

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

    VkDescriptorSetLayout layout{};
    VK_MAKE(vkCreateDescriptorSetLayout(device, &layoutCreateInfo, nullptr, &layout), "Failed to create bindless descriptor layout.");

    std::array<VkDescriptorSetLayout, MAX_FRAMES_IN_FLIGHT> layouts{{layout, layout}};

    VkDescriptorSetAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptorPool,
        .descriptorSetCount = 2,
        .pSetLayouts = layouts.data(),
    };

    std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> descriptorSets{};
    VK_MAKE(vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()), "Failed to allocate bindless descriptor set.");

    return DescriptorPoolSet<MAX_FRAMES_IN_FLIGHT>{
        .pool = descriptorPool,
        .layout = layout,
        .sets = descriptorSets,
    };
  }

  void writeGlobalDescriptors(Members& m, DescriptorPoolSet<MAX_FRAMES_IN_FLIGHT>& sets) {
    const auto& vkcore = m.vkcore;
    const auto& buffers = m.buffers;

    size_t camSize = maths::roundToAlignment(sizeof(components::Camera::Uniforms), limits::minUniformBufferOffsetAlignment);
    size_t addressesSize = maths::roundToAlignment(sizeof(BufferPointers), limits::minUniformBufferOffsetAlignment);

    constexpr size_t BUFFER_COUNT = 2;
    constexpr size_t TOTAL_BUFFER_WRITES = MAX_FRAMES_IN_FLIGHT * BUFFER_COUNT;

    std::array<VkDescriptorBufferInfo, TOTAL_BUFFER_WRITES> bufferInfos{};
    std::array<VkWriteDescriptorSet, MAX_FRAMES_IN_FLIGHT> writes{};
    for (size_t writeIdx = 0; writeIdx < MAX_FRAMES_IN_FLIGHT; ++writeIdx) {
      auto bufferIdx = writeIdx * BUFFER_COUNT;
      bufferInfos[bufferIdx] = VkDescriptorBufferInfo{
          .buffer = *buffers.camera,
          .offset = writeIdx * camSize,
          .range = sizeof(components::Camera::Uniforms),
      };
      bufferInfos[bufferIdx + 1] = VkDescriptorBufferInfo{
          .buffer = *buffers.addresses,
          .offset = writeIdx * addressesSize,
          .range = sizeof(BufferPointers),
      };
      writes[writeIdx] = VkWriteDescriptorSet{
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = sets.sets[writeIdx],
          .dstBinding = UNIFORM_BUFFER,
          .dstArrayElement = 0,
          .descriptorCount = 2,
          .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
          .pBufferInfo = &bufferInfos[bufferIdx],
      };
    }

    vkUpdateDescriptorSets(vkcore.device, writes.size(), writes.data(), 0, nullptr);

    m.indices.nextUniformBufferIndex = BUFFER_COUNT;
  }

  std::expected<StaticDescriptors, std::string> createStaticDescriptors(const VulkanCore& vkcore) {
    std::array poolSizes{
        VkDescriptorPoolSize{
            .type = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
        },
        VkDescriptorPoolSize{
            .type = VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 1,
        },
        VkDescriptorPoolSize{
            .type = VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = constants::STATIC_TEXTURE_COUNT,
        },
    };

    VkDescriptorPoolCreateInfo poolCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data(),
    };

    VkDescriptorPool descriptorPool{};
    VK_MAKE(vkCreateDescriptorPool(vkcore.device.logical, &poolCreateInfo, nullptr, &descriptorPool),
            "Failed to create static descriptor pool.");

    constexpr size_t descriptorBindingCount = 3;

    std::array<VkDescriptorSetLayoutBinding, descriptorBindingCount> bindings{
        VkDescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_ALL,
        },
        VkDescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_ALL,
        },
        VkDescriptorSetLayoutBinding{
            .binding = 2,
            .descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = constants::STATIC_TEXTURE_COUNT,
            .stageFlags = VK_SHADER_STAGE_ALL,
        }};

    VkDescriptorSetLayoutCreateInfo layoutCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = descriptorBindingCount,
        .pBindings = bindings.data(),
    };

    VkDescriptorSetLayout layout{};
    VK_MAKE(vkCreateDescriptorSetLayout(vkcore.device.logical, &layoutCreateInfo, nullptr, &layout),
            "Failed to create static descriptor layout.");

    VkDescriptorSetAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &layout,
    };

    VkDescriptorSet descriptorSet{};
    VK_MAKE(vkAllocateDescriptorSets(vkcore.device.logical, &allocInfo, &descriptorSet), "Failed to allocate static descriptor set.");

    return StaticDescriptors{
        .pool = descriptorPool,
        .layout = layout,
        .set = descriptorSet,
    };
  }

  void writeStaticDescriptors(const VulkanCore& vkcore, const StaticDescriptors& staticDescriptorSets, const Buffers& buffers,
                              const RenderTargets& renderTargets, const Samplers& samplers) {
    VkDescriptorBufferInfo bufferInfo{
        .buffer = *buffers.ssaoKernel,
        .offset = 0,
        .range = sizeof(glm::vec4) * constants::SSAO_KERNEL_SIZE,
    };

    VkDescriptorImageInfo ssaoResultImageInfo{
        .sampler = samplers.linearRepeat,
        .imageView = *renderTargets.lights.ssaoResult,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };

    std::array<VkDescriptorImageInfo, constants::STATIC_TEXTURE_COUNT> imageInfos = {
        VkDescriptorImageInfo{
            .sampler = samplers.linearRepeat,
            .imageView = *renderTargets.gBuffer.albedo,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
        VkDescriptorImageInfo{
            .sampler = samplers.linearRepeat,
            .imageView = *renderTargets.gBuffer.normal,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
        VkDescriptorImageInfo{
            .sampler = samplers.linearRepeat,
            .imageView = *renderTargets.gBuffer.emissive,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
        VkDescriptorImageInfo{
            .sampler = samplers.linearRepeat,
            .imageView = *renderTargets.gBuffer.metRough,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
        VkDescriptorImageInfo{
            .sampler = samplers.linearRepeat,
            .imageView = *renderTargets.gBuffer.depth,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
        VkDescriptorImageInfo{
            .sampler = samplers.linearRepeat,
            .imageView = *renderTargets.lights.diffuse,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
        VkDescriptorImageInfo{
            .sampler = samplers.linearRepeat,
            .imageView = *renderTargets.lights.specular,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
        VkDescriptorImageInfo{
            .sampler = samplers.nearestRepeat,
            .imageView = *renderTargets.lights.ssaoNoise,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
        VkDescriptorImageInfo{
            .sampler = samplers.nearestRepeat,
            .imageView = *renderTargets.lights.ssaoResult,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
        VkDescriptorImageInfo{
            .sampler = samplers.nearestRepeat,
            .imageView = *renderTargets.lights.ssaoBlur,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
        VkDescriptorImageInfo{
            .sampler = samplers.linearRepeat,
            .imageView = *renderTargets.lights.combined,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        },
    };

    std::array writes = {
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = staticDescriptorSets.set,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &bufferInfo,
        },
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = staticDescriptorSets.set,
            .dstBinding = 1,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &ssaoResultImageInfo,
        },
        VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = staticDescriptorSets.set,
            .dstBinding = 2,
            .dstArrayElement = 0,
            .descriptorCount = imageInfos.size(),
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = imageInfos.data(),
        },
    };

    vkUpdateDescriptorSets(vkcore.device.logical, writes.size(), writes.data(), 0, nullptr);
  }
} // namespace kt::vkh::setup
