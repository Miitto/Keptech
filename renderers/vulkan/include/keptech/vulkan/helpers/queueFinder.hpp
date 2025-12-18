#pragma once

#include <functional>
#include <keptech/core/bitflag.hpp>

namespace keptech::vkh {

  class QueueFinder {
  public:
    struct QueueFamily {
      vk::QueueFamilyProperties properties;
      uint32_t index;
    };

  private:
    std::vector<QueueFamily> queueFamilyProperties;

  public:
    enum class QueueTypeFlags : uint8_t {
      Present = 0, // Is not really a queue flag, but a special case
      Graphics = static_cast<VkQueueFlags>(vk::QueueFlagBits::eGraphics),
      Transfer = static_cast<VkQueueFlags>(vk::QueueFlagBits::eTransfer),
      Compute = static_cast<VkQueueFlags>(vk::QueueFlagBits::eCompute),
    };

    struct PresentQueue {
      const vk::raii::PhysicalDevice& device;
      const vk::raii::SurfaceKHR& surface;
    };

    union QueueTypeParams {
      void* none = nullptr;
      PresentQueue presentQueue;
    };

    struct QueueType {
      QueueTypeFlags type;
      QueueTypeParams params = QueueTypeParams{.none = nullptr};
    };

    QueueFinder(const vk::raii::PhysicalDevice& physicalDevice) noexcept
        : queueFamilyProperties(std::vector<QueueFamily>{}) {
      auto props = physicalDevice.getQueueFamilyProperties();

      queueFamilyProperties.reserve(props.size());

      for (uint32_t i = 0; i < props.size(); ++i) {
        queueFamilyProperties.push_back({.properties = props[i], .index = i});
      }
    }

    QueueFinder(std::vector<QueueFamily>&& queueFamilyProperties) noexcept
        : queueFamilyProperties(std::move(queueFamilyProperties)) {}

    [[nodiscard]] auto
    find(const std::function<bool(QueueFamily)>& finder) const -> QueueFinder;
    [[nodiscard]] auto findType(const QueueType type) const -> QueueFinder;
    [[nodiscard]] auto findCombined(const std::vector<QueueType>& types) const
        -> QueueFinder;
    [[nodiscard]] auto filterTypes(core::Bitflag<QueueTypeFlags> type) const
        -> QueueFinder;

    [[nodiscard]] auto queues() const noexcept
        -> const std::vector<QueueFamily>& {
      return queueFamilyProperties;
    }

    [[nodiscard]] auto hasQueue() const noexcept -> bool {
      return !queueFamilyProperties.empty();
    }

    [[nodiscard]] auto size() const noexcept -> size_t {
      return queueFamilyProperties.size();
    }

    [[nodiscard]] auto operator[](size_t index) const noexcept
        -> const QueueFamily& {
      return queueFamilyProperties[index];
    }

    [[nodiscard]] auto begin() const noexcept
        -> std::vector<QueueFamily>::const_iterator {
      return queueFamilyProperties.begin();
    }

    [[nodiscard]] auto end() const noexcept
        -> std::vector<QueueFamily>::const_iterator {
      return queueFamilyProperties.end();
    }

    [[nodiscard]] auto cbegin() const noexcept
        -> std::vector<QueueFamily>::const_iterator {
      return queueFamilyProperties.cbegin();
    }

    [[nodiscard]] auto cend() const noexcept
        -> std::vector<QueueFamily>::const_iterator {
      return queueFamilyProperties.cend();
    }

    [[nodiscard]] auto first() const noexcept -> const QueueFamily& {
      return queueFamilyProperties.front();
    }
  };
} // namespace keptech::vkh

DEFINE_BITFLAG_ENUM_OPERATORS(keptech::vkh::QueueFinder::QueueTypeFlags)
