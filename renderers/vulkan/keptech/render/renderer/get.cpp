#include "renderer.hpp"

namespace kt::rdr {
  Renderer& Renderer::get() { return singleton; }
  const Renderer::Members& Renderer::getMembers() const { return m; }
  Renderer::Members& Renderer::getMembers() { return m; }
  const Device& Renderer::getDevice() const { return m.vkcore.device; }
  const Device* Renderer::operator->() const { return &m.vkcore.device; }

  [[nodiscard]] VkDescriptorSetLayout Renderer::getGlobalDescriptorSetLayout() const { return m.globalDescriptorSets.layout; }
  [[nodiscard]] VkDescriptorSet Renderer::getGlobalDescriptorSet() const { return m.globalDescriptorSets.sets[m.frameInfo.index]; }
  [[nodiscard]] const Buffers& Renderer::getBuffers() const { return m.buffers; }
  std::array<VkBuffer, 2> Renderer::getVertexBuffers() const { return {m.buffers.vertexPositions, m.buffers.vertexAttribs}; }
  [[nodiscard]] VkBuffer Renderer::getIndexBuffer() const { return m.buffers.indices; }
  [[nodiscard]] VkBuffer Renderer::getMeshletBuffer() const { return m.buffers.meshlets; }
  [[nodiscard]] VkBuffer Renderer::getMeshletVertexBuffer() const { return m.buffers.meshletVertices; }
  [[nodiscard]] VkBuffer Renderer::getMeshletTriangleBuffer() const { return m.buffers.meshletTriangles; }
  [[nodiscard]] VkBuffer Renderer::getMaterialBuffer() const { return m.buffers.materials; }
  [[nodiscard]] VkSampler Renderer::getLinearRepeatSampler() const { return m.samplers.linearRepeat; }
  [[nodiscard]] VkSampler Renderer::getLinearClampSampler() const { return m.samplers.linearClamp; }
  [[nodiscard]] VkSampler Renderer::getNearestRepeatSampler() const { return m.samplers.nearestRepeat; }
  [[nodiscard]] VkSampler Renderer::getNearestClampSampler() const { return m.samplers.nearestClamp; }
  [[nodiscard]] Result<Buffer, VkResult, VK_SUCCESS> Renderer::createBuffer(const BufferCreateInfo& info) const {
    return Buffer::create(info);
  }
  [[nodiscard]] Result<Image, VkResult, VK_SUCCESS> Renderer::createImage(const ImageCreateInfo& info) const { return Image::create(info); }
  [[nodiscard]] Result<Shader, VkResult, VK_SUCCESS> Renderer::createShader(const shaders::Shader& info) const {
    return Shader::create(info);
  }
  [[nodiscard]] Result<VkPipelineLayout, VkResult, VK_SUCCESS>
  Renderer::createPipelineLayout(const VkPipelineLayoutCreateInfo& info) const {
    return m.vkcore.device.createPipelineLayout(info);
  }
  [[nodiscard]] Result<Pipeline, VkResult, VK_SUCCESS> Renderer::createPipeline(const VkGraphicsPipelineCreateInfo& info) const {
    return m.vkcore.device.createPipeline(info);
  }
} // namespace kt::rdr