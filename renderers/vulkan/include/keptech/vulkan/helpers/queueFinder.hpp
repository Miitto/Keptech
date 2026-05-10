#pragma once

#include <Volk/volk.h>
#include <cstdint>
#include <functional>
#include <keptech/core/bitflag.hpp>

namespace kt::vkh {

  class QueueFinder {
  public:
    struct QueueFamily {
      VkQueueFamilyProperties properties;
      uint32_t index;
    };

  private:
    std::vector<QueueFamily> queueFamilyProperties;

  public:
    enum class QueueTypeFlags : uint8_t {
      Present = 0, // Is not really a queue flag, but a special case
      Graphics = VkQueueFlagBits::VK_QUEUE_GRAPHICS_BIT,
      Transfer = VkQueueFlagBits::VK_QUEUE_TRANSFER_BIT,
      Compute = VkQueueFlagBits::VK_QUEUE_COMPUTE_BIT,
    };

    struct PresentQueue {
      const VkPhysicalDevice& device;
      const VkSurfaceKHR& surface;
    };

    union QueueTypeParams {
      void* none = nullptr;
      PresentQueue presentQueue;
    };

    struct QueueType {
      QueueTypeFlags type;
      QueueTypeParams params = QueueTypeParams{.none = nullptr};
    };

    QueueFinder(const VkPhysicalDevice& physicalDevice) noexcept : queueFamilyProperties(std::vector<QueueFamily>{}) {
      uint32_t queueFamilyCount = 0;
      vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
      std::vector<VkQueueFamilyProperties> props(queueFamilyCount);
      vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, props.data());

      queueFamilyProperties.reserve(props.size());

      for (uint32_t i = 0; i < props.size(); ++i) {
        queueFamilyProperties.push_back({.properties = props[i], .index = i});
      }
    }

    QueueFinder(std::vector<QueueFamily>&& queueFamilyProperties) noexcept : queueFamilyProperties(std::move(queueFamilyProperties)) {}

    [[nodiscard]] auto find(const std::function<bool(QueueFamily)>& finder) const -> QueueFinder;
    [[nodiscard]] auto findType(const QueueType type) const -> QueueFinder;
    [[nodiscard]] auto findCombined(const std::vector<QueueType>& types) const -> QueueFinder;
    [[nodiscard]] auto filterTypes(Bitflag<QueueTypeFlags> type) const -> QueueFinder;

    [[nodiscard]] auto queues() const noexcept -> const std::vector<QueueFamily>& { return queueFamilyProperties; }

    [[nodiscard]] auto hasQueue() const noexcept -> bool { return !queueFamilyProperties.empty(); }

    [[nodiscard]] auto size() const noexcept -> size_t { return queueFamilyProperties.size(); }

    [[nodiscard]] auto operator[](size_t index) const noexcept -> const QueueFamily& { return queueFamilyProperties[index]; }

    [[nodiscard]] auto begin() const noexcept -> std::vector<QueueFamily>::const_iterator { return queueFamilyProperties.begin(); }

    [[nodiscard]] auto end() const noexcept -> std::vector<QueueFamily>::const_iterator { return queueFamilyProperties.end(); }

    [[nodiscard]] auto cbegin() const noexcept -> std::vector<QueueFamily>::const_iterator { return queueFamilyProperties.cbegin(); }

    [[nodiscard]] auto cend() const noexcept -> std::vector<QueueFamily>::const_iterator { return queueFamilyProperties.cend(); }

    [[nodiscard]] auto first() const noexcept -> const QueueFamily& { return queueFamilyProperties.front(); }
  };
} // namespace kt::vkh

DEFINE_BITFLAG_ENUM_OPERATORS(kt::vkh::QueueFinder::QueueTypeFlags)
