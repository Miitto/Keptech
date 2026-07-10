#include "constants.hpp"
#include "keptech/maths/maths.hpp"
#include "keptech/vulkan/renderer.hpp"
#include "keptech/vulkan/structs.hpp"
#include "macros.hpp"
#include <Volk/volk.h>
#include <array>
#include <keptech/components/camera.hpp>

namespace kt::vkh::setup {

  enum PerFrameUniformBufferIndices : uint8_t {
    CAMERA,
    ADDRESSES,
    MAX_PER_FRAME_UNIFORM_BUFFER_COUNT,
  };

  std::expected<DescriptorPoolSet<MAX_FRAMES_IN_FLIGHT>, std::string> createGlobalDescriptors(VkDevice device) {

    std::array sizes{VkDescriptorPoolSize{
                         .type = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                         .descriptorCount = MAX_PER_FRAME_UNIFORM_BUFFER_COUNT * MAX_FRAMES_IN_FLIGHT,
                     },
                     VkDescriptorPoolSize{
                         .type = VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                         .descriptorCount = 1000 * MAX_FRAMES_IN_FLIGHT,
                     }};

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

    constexpr size_t descriptorBindingCount = 3;

    std::array<VkDescriptorSetLayoutBinding, descriptorBindingCount> bindings{
        // Camera
        VkDescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_ALL,
        },
        VkDescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1000,
            .stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_ALL,
        },
        // Addresses
        VkDescriptorSetLayoutBinding{
            .binding = 2,
            .descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_ALL,
        }};

    std::array<VkDescriptorBindingFlags, descriptorBindingCount> bindingFlags{};
    for (auto& b : bindingFlags) {
      b = VkDescriptorBindingFlagBits::VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
    }

    bindingFlags[1] |= VkDescriptorBindingFlagBits::VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

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

  void writeGlobalDescriptors(const VulkanCore& vkcore, DescriptorPoolSet<MAX_FRAMES_IN_FLIGHT>& sets, Buffers& buffers) {
    {
      size_t size = maths::roundToAlignment(sizeof(components::Camera::Uniforms), limits::minUniformBufferOffsetAlignment);

      std::array<VkDescriptorBufferInfo, MAX_FRAMES_IN_FLIGHT> bufferInfos{};
      std::array<VkWriteDescriptorSet, MAX_FRAMES_IN_FLIGHT> writes{};
      for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        bufferInfos[i] = VkDescriptorBufferInfo{
            .buffer = *buffers.camera,
            .offset = i * size,
            .range = sizeof(components::Camera::Uniforms),
        };
        writes[i] = VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = sets.sets[i],
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &bufferInfos[i],
        };
      }

      vkUpdateDescriptorSets(vkcore.device, writes.size(), writes.data(), 0, nullptr);
    }

    {
      size_t size = maths::roundToAlignment(sizeof(BufferPointers), limits::minUniformBufferOffsetAlignment);
      std::array<VkDescriptorBufferInfo, MAX_FRAMES_IN_FLIGHT> bufferInfos{};
      std::array<VkWriteDescriptorSet, MAX_FRAMES_IN_FLIGHT> writes{};
      for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        bufferInfos[i] = VkDescriptorBufferInfo{
            .buffer = *buffers.addresses,
            .offset = i * size,
            .range = sizeof(BufferPointers),
        };
        writes[i] = VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = sets.sets[i],
            .dstBinding = 2,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &bufferInfos[i],
        };
      }
      vkUpdateDescriptorSets(vkcore.device, writes.size(), writes.data(), 0, nullptr);
    }
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
