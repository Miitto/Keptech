#pragma once

#include "keptech/vulkan/structs.hpp"
#include "macros.hpp"
#include "vulkan/vulkan.hpp"
#include <array>
#include <vulkan/vulkan_raii.hpp>

namespace keptech::vkh::setup {

  std::expected<DescriptorPoolSet<MAX_FRAMES_IN_FLIGHT>, std::string>
  createGlobalDescriptors(vk::raii::Device& device) {
    constexpr size_t descriptorBindingCount = 2;

    std::array<vk::DescriptorPoolSize, descriptorBindingCount> sizes{
        vk::DescriptorPoolSize{
            .type = vk::DescriptorType::eUniformBuffer,
            .descriptorCount = MAX_FRAMES_IN_FLIGHT,
        },
        vk::DescriptorPoolSize{
            .type = vk::DescriptorType::eCombinedImageSampler,
            .descriptorCount = 1000 * MAX_FRAMES_IN_FLIGHT,
        },
    };

    vk::DescriptorPoolCreateInfo poolCreateInfo{
        .flags = vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind |
                 vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        .maxSets = 2,
        .poolSizeCount = static_cast<uint32_t>(sizes.size()),
        .pPoolSizes = sizes.data(),
    };

    VK_MAKE(descriptorPool, device.createDescriptorPool(poolCreateInfo),
            "Failed to create bindless descriptor pool.");

    std::array<vk::DescriptorSetLayoutBinding, descriptorBindingCount> bindings{
        vk::DescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = vk::DescriptorType::eUniformBuffer,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eAll,
        },
        vk::DescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
            .descriptorCount = 1000,
            .stageFlags = vk::ShaderStageFlagBits::eAll,
        },
    };

    std::array<vk::DescriptorBindingFlags, descriptorBindingCount> bindingFlags{
        vk::DescriptorBindingFlagBits::eUpdateAfterBind,
        vk::DescriptorBindingFlagBits::ePartiallyBound |
            vk::DescriptorBindingFlagBits::eUpdateAfterBind,
    };

    vk::DescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{
        .bindingCount = descriptorBindingCount,
        .pBindingFlags = bindingFlags.data(),
    };

    vk::DescriptorSetLayoutCreateInfo layoutCreateInfo{
        .pNext = &bindingFlagsInfo,
        .flags = vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool,
        .bindingCount = descriptorBindingCount,
        .pBindings = bindings.data(),
    };

    VK_MAKE(layout, device.createDescriptorSetLayout(layoutCreateInfo),
            "Failed to create bindless descriptor layout.");

    std::array<vk::DescriptorSetLayout, MAX_FRAMES_IN_FLIGHT> layouts{
        {*layout, *layout}};

    vk::DescriptorSetAllocateInfo allocInfo{
        .descriptorPool = descriptorPool,
        .descriptorSetCount = 2,
        .pSetLayouts = layouts.data(),
    };

    VK_MAKE(descriptorSets, device.allocateDescriptorSets(allocInfo),
            "Failed to allocate bindless descriptor set.");

#ifndef NDEBUG
    for (int i = 0; i < descriptorSets.size(); i++) {
      std::string name = fmt::format("Global Descriptor Set {}", i);

      VkDescriptorSet vkDescriptorSet = *descriptorSets[i];
      device.setDebugUtilsObjectNameEXT(vk::DebugUtilsObjectNameInfoEXT{
          .objectType = vk::ObjectType::eDescriptorSet,
          .objectHandle = reinterpret_cast<uint64_t>(vkDescriptorSet),
          .pObjectName = name.c_str(),
      });
    }
#endif

    return DescriptorPoolSet<MAX_FRAMES_IN_FLIGHT>{
        .pool = std::move(descriptorPool),
        .layout = std::move(layout),
        .sets = {std::move(descriptorSets[0]), std::move(descriptorSets[1])},
    };
  }

} // namespace keptech::vkh::setup
