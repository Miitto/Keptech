#include "keptech/rhi/imgui.hpp"
#include "keptech/rhi/rhi.hpp"
#include "macros.hpp"
#include "rhi.hpp"
#include <expected>
#include <imgui/backends/imgui_impl_sdl3.h>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <imgui/imgui.h>
#include <keptech/core/window.hpp>

namespace kt::rhi {

  std::expected<void, std::string> RHI::initImGui() {
    rendering::initImGui();

    std::array<VkDescriptorPoolSize, 11> pool_sizes = {{
        {
            .type = VkDescriptorType::VK_DESCRIPTOR_TYPE_SAMPLER,
            .descriptorCount = 1000,
        },
        {
            .type = VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1000,
        },
        {
            .type = VkDescriptorType::VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount = 1000,
        },
        {
            .type = VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 1000,
        },
        {
            .type = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
            .descriptorCount = 1000,
        },
        {
            .type = VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
            .descriptorCount = 1000,
        },
        {
            .type = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1000,
        },
        {
            .type = VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1000,
        },
        {
            .type = VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
            .descriptorCount = 1000,
        },
        {
            .type = VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
            .descriptorCount = 1000,
        },
        {
            .type = VkDescriptorType::VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
            .descriptorCount = 1000,
        },
    }};

    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VkDescriptorPoolCreateFlagBits::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 1000,
        .poolSizeCount = (uint32_t)std::size(pool_sizes),
        .pPoolSizes = pool_sizes.data(),
    };

    VK_MAKE(vkCreateDescriptorPool(m.vkcore.device, &pool_info, nullptr, &m.imGuiDescriptorPool), "Failed to create ImGui descriptor pool");

    // 2: initialize imgui library

    // this initializes the core structures of imgui

    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Enable Docking

    ImGui_ImplSDL3_InitForVulkan(m.window->getHandle());

    // this initializes imgui for Vulkan
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.ApiVersion = VK_API_VERSION_1_4;
    init_info.Instance = m.vkcore.instance;
    init_info.PhysicalDevice = m.vkcore.device, init_info.Device = m.vkcore.device;
    init_info.QueueFamily = m.vkcore.queues.graphics.index;
    init_info.Queue = m.vkcore.queues.graphics.queue;
    init_info.DescriptorPool = m.imGuiDescriptorPool;
    init_info.MinImageCount = 3;
    init_info.ImageCount = 3;
    init_info.UseDynamicRendering = true;

    VkPipelineRenderingCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    info.colorAttachmentCount = 1;

    auto swapchainFormatC = static_cast<VkFormat>(m.vkcore.swapchain.config().format.format);

    info.pColorAttachmentFormats = &swapchainFormatC;

    // dynamic rendering parameters for imgui to use
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo = info;

    ImGui_ImplVulkan_Init(&init_info);

    return {};
  }
} // namespace kt::rhi
