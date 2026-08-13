#include "rhi.hpp"

namespace kt::rhi {
  Renderer& RHI::get() { return singleton; }
  const RHI::Members& RHI::getMembers() const { return m; }
  RHI::Members& RHI::getMembers() { return m; }
  const Device& RHI::getDevice() const { return m.vkcore.device; }
  const Device* RHI::operator->() const { return &m.vkcore.device; }

  [[nodiscard]] VkDescriptorSetLayout RHI::getGlobalDescriptorSetLayout() const { return m.globalDescriptorSets.layout; }
  [[nodiscard]] VkDescriptorSet RHI::getGlobalDescriptorSet() const { return m.globalDescriptorSets.sets[m.frameInfo.index]; }
  [[nodiscard]] const Buffers& RHI::getBuffers() const { return m.buffers; }
  std::array<VkBuffer, 2> RHI::getVertexBuffers() const { return {m.buffers.vertexPositions, m.buffers.vertexAttribs}; }
  [[nodiscard]] VkBuffer RHI::getIndexBuffer() const { return m.buffers.indices; }
  [[nodiscard]] VkBuffer RHI::getMeshletBuffer() const { return m.buffers.meshlets; }
  [[nodiscard]] VkBuffer RHI::getMeshletVertexBuffer() const { return m.buffers.meshletVertices; }
  [[nodiscard]] VkBuffer RHI::getMeshletTriangleBuffer() const { return m.buffers.meshletTriangles; }
  [[nodiscard]] VkBuffer RHI::getMaterialBuffer() const { return m.buffers.materials; }
  [[nodiscard]] VkSampler RHI::getLinearRepeatSampler() const { return m.samplers.linearRepeat; }
  [[nodiscard]] VkSampler RHI::getLinearClampSampler() const { return m.samplers.linearClamp; }
  [[nodiscard]] VkSampler RHI::getNearestRepeatSampler() const { return m.samplers.nearestRepeat; }
  [[nodiscard]] VkSampler RHI::getNearestClampSampler() const { return m.samplers.nearestClamp; }
  [[nodiscard]] Result<Buffer, VkResult, VK_SUCCESS> RHI::createBuffer(const BufferCreateInfo& info) const { return Buffer::create(info); }
  [[nodiscard]] Result<Image, VkResult, VK_SUCCESS> RHI::createImage(const ImageCreateInfo& info) const { return Image::create(info); }
  [[nodiscard]] Result<Shader, VkResult, VK_SUCCESS> RHI::createShader(const shaders::Shader& info) const { return Shader::create(info); }
  [[nodiscard]] Result<VkPipelineLayout, VkResult, VK_SUCCESS> RHI::createPipelineLayout(const VkPipelineLayoutCreateInfo& info) const {
    return m.vkcore.device.createPipelineLayout(info);
  }
  [[nodiscard]] Result<Pipeline, VkResult, VK_SUCCESS> RHI::createPipeline(const VkGraphicsPipelineCreateInfo& info) const {
    return m.vkcore.device.createPipeline(info);
  }
} // namespace kt::rhi