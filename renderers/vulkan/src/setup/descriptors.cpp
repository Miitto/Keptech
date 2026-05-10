#pragma once

#include "keptech/vulkan/renderer.hpp"
#include "keptech/vulkan/structs.hpp"
#include "macros.hpp"
#include <Volk/volk.h>
#include <array>

namespace kt::vkh::setup {

  std::expected<DescriptorPoolSet<MAX_FRAMES_IN_FLIGHT>, std::string> createGlobalDescriptors(VkDevice device) {
    constexpr size_t descriptorBindingCount = 4;

    std::array sizes{VkDescriptorPoolSize{
                         .type = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                         .descriptorCount = MAX_FRAMES_IN_FLIGHT,
                     },
                     VkDescriptorPoolSize{
                         .type = VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                         .descriptorCount = 1000 * MAX_FRAMES_IN_FLIGHT,
                     },
                     VkDescriptorPoolSize{
                         .type = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                         .descriptorCount = 1 * MAX_FRAMES_IN_FLIGHT,
                     },
                     VkDescriptorPoolSize{
                         .type = VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                         .descriptorCount = 1 * MAX_FRAMES_IN_FLIGHT,
                     }};

    VkDescriptorPoolCreateInfo poolCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VkDescriptorPoolCreateFlagBits::VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT |
                 VkDescriptorPoolCreateFlagBits::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 2,
        .poolSizeCount = static_cast<uint32_t>(sizes.size()),
        .pPoolSizes = sizes.data(),
    };

    VkDescriptorPool descriptorPool;
    VK_MAKE(vkCreateDescriptorPool(device, &poolCreateInfo, nullptr, &descriptorPool), "Failed to create bindless descriptor pool.");

    std::array<VkDescriptorSetLayoutBinding, descriptorBindingCount> bindings{
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
        {
            .binding = 2,
            .descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_ALL,
        },
        {
            .binding = 3,
            .descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 1,
            .stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_ALL,
        }};

    std::array<VkDescriptorBindingFlags, descriptorBindingCount> bindingFlags{
        VkDescriptorBindingFlagBits::VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
        VkDescriptorBindingFlagBits::VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
            VkDescriptorBindingFlagBits::VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
        VkDescriptorBindingFlagBits::VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
        VkDescriptorBindingFlagBits::VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
    };

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

    VkDescriptorSetLayout layout;
    VK_MAKE(vkCreateDescriptorSetLayout(device, &layoutCreateInfo, nullptr, &layout), "Failed to create bindless descriptor layout.");

    std::array<VkDescriptorSetLayout, MAX_FRAMES_IN_FLIGHT> layouts{{layout, layout}};

    VkDescriptorSetAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptorPool,
        .descriptorSetCount = 2,
        .pSetLayouts = layouts.data(),
    };

    VkDescriptorSet descriptorSets[MAX_FRAMES_IN_FLIGHT];
    VK_MAKE(vkAllocateDescriptorSets(device, &allocInfo, descriptorSets), "Failed to allocate bindless descriptor set.");

#ifndef NDEBUG
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      std::string name = fmt::format("Global Descriptor Set {}", i);

      VkDescriptorSet vkDescriptorSet = descriptorSets[i];
    }
#endif

    return DescriptorPoolSet<MAX_FRAMES_IN_FLIGHT>{
        .pool = std::move(descriptorPool),
        .layout = std::move(layout),
        .sets = {std::move(descriptorSets[0]), std::move(descriptorSets[1])},
    };
  }

  std::expected<Renderer::StaticDescriptors, std::string> createStaticDescriptors(const Renderer::VulkanCore& vkcore) {
    constexpr size_t descriptorBindingCount = 2;
    std::array<VkDescriptorPoolSize, descriptorBindingCount> poolSizes{
        VkDescriptorPoolSize{
            .type = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
        },
        VkDescriptorPoolSize{
            .type = VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 1,
        },
    };

    VkDescriptorPoolCreateInfo poolCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data(),
    };

    VkDescriptorPool descriptorPool;
    VK_MAKE(vkCreateDescriptorPool(vkcore.device.logical, &poolCreateInfo, nullptr, &descriptorPool),
            "Failed to create static descriptor pool.");

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
    };

    VkDescriptorSetLayoutCreateInfo layoutCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = descriptorBindingCount,
        .pBindings = bindings.data(),
    };

    VkDescriptorSetLayout layout;
    VK_MAKE(vkCreateDescriptorSetLayout(vkcore.device.logical, &layoutCreateInfo, nullptr, &layout),
            "Failed to create static descriptor layout.");

    VkDescriptorSetAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &layout,
    };

    VkDescriptorSet descriptorSet;
    VK_MAKE(vkAllocateDescriptorSets(vkcore.device.logical, &allocInfo, &descriptorSet), "Failed to allocate static descriptor set.");

    return Renderer::StaticDescriptors{
        .pool = descriptorPool,
        .layout = layout,
        .set = descriptorSet,
    };
  }
} // namespace kt::vkh::setup
