#include "constants.hpp"
#include "keptech/maths/maths.hpp"
#include "keptech/rhi/rhi.hpp"

#include "profile.hpp"
#include "vk-logger.hpp"

namespace kt::rhi {

  void RHI::updateTextureDescriptors() {
    KT_PROFILE_FUNCTION
    auto& textureUpdates = m.frameInfo.perFrame->texToUpdate;
    if (!textureUpdates.empty()) {
      auto& globalDescSet = m.globalDescriptorSets.sets[m.frameInfo.index];
      std::vector<VkDescriptorImageInfo> imageInfos;
      imageInfos.reserve(textureUpdates.size());
      std::vector<VkWriteDescriptorSet> descriptorWrites;
      descriptorWrites.reserve(textureUpdates.size());
      for (auto& update : textureUpdates) {
        auto imageIndex = update.indexInDescriptorSet;

        imageInfos.push_back(VkDescriptorImageInfo{
            .sampler = m.samplers.linearRepeat,
            .imageView = *update.texture,
            .imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        });

        descriptorWrites.push_back(VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = globalDescSet,
            .dstBinding = 1,
            .dstArrayElement = static_cast<uint32_t>(imageIndex),
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &imageInfos.back(),
        });
      }

      VK_DEBUG("Updating {} texture descriptors for frame {}", descriptorWrites.size(), m.frameInfo.index);
      vkUpdateDescriptorSets(m.vkcore.device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
      textureUpdates.clear();
    }
  }

  void RHI::updateBufferPointers() const {
    BufferPointers bufferPointers{
        .vertexPositions = m.buffers.vertexPositions->address(),
        .vertexAttribs = m.buffers.vertexAttribs->address(),
        .indices = m.buffers.indices->address(),
        .meshlets = m.buffers.meshlets->address(),
        .meshletVertices = m.buffers.meshletVertices->address(),
        .meshletTriangles = m.buffers.meshletTriangles->address(),
        .materials = m.buffers.materials->address(),
        .meshes = m.buffers.meshes->address(),
    };

    size_t offset = maths::roundToAlignment(m.frameInfo.index * sizeof(BufferPointers), limits::minUniformBufferOffsetAlignment);
    memcpy(m.buffers.addresses.mapping() + offset, &bufferPointers, sizeof(BufferPointers));
  }

  void RHI::loadImage(Image& image) {
    ImageHandle index = m.indices.nextCombinedImageIndex++;
    image.setHandle(index);
    for (auto& frame : m.vkcore.perFrame) {
      frame.texToUpdate.emplace_back(&image, index);
    }
  }
} // namespace kt::rhi
