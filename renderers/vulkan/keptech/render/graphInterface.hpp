#pragma once

#define KT_EXTRA_GRAPH_FNS                                                                                                                 \
  void executeGraphicsPass(size_t passIdx, RenderPass& pass, CommandBuffer& cmd);                                                          \
  void executeComputePass(size_t passIdx, RenderPass& pass, CommandBuffer& cmd);                                                           \
                                                                                                                                           \
  void pipelineBarrier(const Barriers& barriers, const CommandBuffer& cmd) const;                                                          \
  void beginRendering(const RenderPass& pass, const CommandBuffer& cmd) const;                                                             \
  void updateDescriptors();