#include "keptech/vulkan/helpers/queueFinder.hpp"

#include "vk-logger.hpp"

namespace keptech::vkh {

  [[nodiscard]] auto
  QueueFinder::find(const std::function<bool(QueueFamily)>& finder) const
      -> QueueFinder {
    auto filtered = std::vector<QueueFinder::QueueFamily>{};

    for (const auto& queueFamily : queueFamilyProperties) {
      if (finder(queueFamily)) {
        filtered.push_back(queueFamily);
      }
    }

    return {std::move(filtered)};
  }

  [[nodiscard]] auto QueueFinder::findType(const QueueType type) const
      -> QueueFinder {
    auto filtered = std::vector<QueueFinder::QueueFamily>{};

    for (auto& queueFamily : queueFamilyProperties) {
      switch (type.type) {
      case QueueTypeFlags::Graphics: {
        if (queueFamily.properties.queueFlags & vk::QueueFlagBits::eGraphics) {
          filtered.push_back(queueFamily);
        }
        break;
      }
      case QueueTypeFlags::Transfer: {
        if (queueFamily.properties.queueFlags & vk::QueueFlagBits::eTransfer) {
          filtered.push_back(queueFamily);
        }
        break;
      }
      case QueueTypeFlags::Compute: {
        if (queueFamily.properties.queueFlags & vk::QueueFlagBits::eCompute) {
          filtered.push_back(queueFamily);
        }
        break;
      }
      case QueueTypeFlags::Present: {
        auto res = type.params.presentQueue.device.getSurfaceSupportKHR(
            queueFamily.index, type.params.presentQueue.surface);

        if (res.result != vk::Result::eSuccess) {
          VK_ERROR("Failed to query present support for queue family {}: {}",
                   queueFamily.index, vk::to_string(res.result));
          continue;
        }

        if (res.value) {
          filtered.push_back(queueFamily);
        }
        break;
      }
      }
    }

    return {std::move(filtered)};
  }

  [[nodiscard]] auto
  QueueFinder::findCombined(const std::vector<QueueType>& types) const
      -> QueueFinder {
    QueueFinder finder = *this;

    for (const auto& type : types) {
      finder = finder.findType(type);
    }

    return finder;
  }

  QueueFinder
  QueueFinder::filterTypes(core::Bitflag<QueueTypeFlags> type) const {
    std::vector<QueueFinder::QueueFamily> filtered{};

    for (auto& queueFamily : queueFamilyProperties) {
      const uint8_t vkFlags =
          static_cast<uint32_t>(queueFamily.properties.queueFlags);
      if ((type & vkFlags) == 0) {
        filtered.push_back(queueFamily);
      }
    }

    return {std::move(filtered)};
  }
} // namespace keptech::vkh
