#pragma once

namespace keptech::vkh {
  struct Device {
    vk::raii::PhysicalDevice physical;
    vk::raii::Device logical;

    operator vk::raii::PhysicalDevice&() { return physical; }
    operator vk::raii::Device&() { return logical; }

    vk::raii::Device& operator*() { return logical; }
    vk::raii::Device* operator->() { return &logical; }
  };
} // namespace keptech::vkh
