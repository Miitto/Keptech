#pragma once

#include "keptech/vulkan/renderer.hpp"
#include "keptech/vulkan/structs.hpp"
#include "macros.hpp"
#include <expected>
#include <imgui/backends/imgui_impl_sdl3.h>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <imgui/imgui.h>
#include <keptech/core/window.hpp>

namespace keptech::vkh::setup {
  using namespace keptech::vkh;

  std::expected<Renderer::ImGuiVkObjects, std::string>
  setupImGui(const keptech::core::window::Window& window,
             const Renderer::VulkanCore& vkcore) {
    std::array<vk::DescriptorPoolSize, 11> pool_sizes = {
        {{
             .type = vk::DescriptorType::eSampler,
             .descriptorCount = 1000,
         },
         {
             .type = vk::DescriptorType::eCombinedImageSampler,
             .descriptorCount = 1000,
         },
         {
             .type = vk::DescriptorType::eSampledImage,
             .descriptorCount = 1000,
         },
         {
             .type = vk::DescriptorType::eStorageImage,
             .descriptorCount = 1000,
         },
         {
             .type = vk::DescriptorType::eUniformTexelBuffer,
             .descriptorCount = 1000,
         },
         {
             .type = vk::DescriptorType::eStorageTexelBuffer,
             .descriptorCount = 1000,
         },
         {
             .type = vk::DescriptorType::eUniformBuffer,
             .descriptorCount = 1000,
         },
         {
             .type = vk::DescriptorType::eStorageBuffer,
             .descriptorCount = 1000,
         },
         {
             .type = vk::DescriptorType::eUniformBufferDynamic,
             .descriptorCount = 1000,
         },
         {
             .type = vk::DescriptorType::eStorageBufferDynamic,
             .descriptorCount = 1000,
         },
         {
             .type = vk::DescriptorType::eInputAttachment,
             .descriptorCount = 1000,
         }}};

    vk::DescriptorPoolCreateInfo pool_info = {
        .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        .maxSets = 1000,
        .poolSizeCount = (uint32_t)std::size(pool_sizes),
        .pPoolSizes = pool_sizes.data(),
    };

    VK_MAKE(imguiPool, vkcore.device.logical.createDescriptorPool(pool_info),
            "Failed to create ImGui descriptor pool");

    // 2: initialize imgui library

    // this initializes the core structures of imgui
    ImGui::CreateContext();

    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Enable Docking

    ImGui_ImplSDL3_InitForVulkan(window.getHandle());

    // this initializes imgui for Vulkan
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.ApiVersion = VK_API_VERSION_1_4;
    init_info.Instance = *vkcore.instance;
    init_info.PhysicalDevice = *vkcore.device.physical,
    init_info.Device = *vkcore.device.logical;
    init_info.QueueFamily = vkcore.queues.graphics.index;
    init_info.Queue = **vkcore.queues.graphics.queue;
    init_info.DescriptorPool = *imguiPool;
    init_info.MinImageCount = 3;
    init_info.ImageCount = 3;
    init_info.UseDynamicRendering = true;

    VkPipelineRenderingCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    info.colorAttachmentCount = 1;

    auto swapchainFormatC =
        static_cast<VkFormat>(vkcore.swapchain.config().format.format);

    info.pColorAttachmentFormats = &swapchainFormatC;

    // dynamic rendering parameters for imgui to use
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo = info;

    ImGui_ImplVulkan_Init(&init_info);

    VK_MAKE(linearSampler,
            vkcore.device.logical.createSampler(vk::SamplerCreateInfo{
                .magFilter = vk::Filter::eLinear,
                .minFilter = vk::Filter::eLinear,
                .mipmapMode = vk::SamplerMipmapMode::eLinear,
                .addressModeU = vk::SamplerAddressMode::eRepeat,
                .addressModeV = vk::SamplerAddressMode::eRepeat,
                .addressModeW = vk::SamplerAddressMode::eRepeat,
                .maxAnisotropy = 1.0f,
            }),
            "Failed to create ImGui linear sampler");

    VkSampler samplerC = static_cast<VkSampler>(*linearSampler);

    auto albedoHandle = ImGui_ImplVulkan_AddTexture(
        samplerC, static_cast<VkImageView>(vkcore.gBuffer.color.view),
        static_cast<VkImageLayout>(vk::ImageLayout::eShaderReadOnlyOptimal));
    auto normalHandle = ImGui_ImplVulkan_AddTexture(
        samplerC, static_cast<VkImageView>(vkcore.gBuffer.normal.view),
        static_cast<VkImageLayout>(vk::ImageLayout::eShaderReadOnlyOptimal));
    auto depthHandle = ImGui_ImplVulkan_AddTexture(
        samplerC, static_cast<VkImageView>(vkcore.gBuffer.depth.view),
        static_cast<VkImageLayout>(vk::ImageLayout::eShaderReadOnlyOptimal));

    Renderer::ImGuiGBufferHandles gBufferHandles{
        .albedo = albedoHandle,
        .normal = normalHandle,
        .depth = depthHandle,
    };

    return Renderer::ImGuiVkObjects{.descriptorPool = std::move(imguiPool),
                                    .gBufferSampler = std::move(linearSampler),
                                    .gBufferHandles = gBufferHandles};
  }
} // namespace keptech::vkh::setup
