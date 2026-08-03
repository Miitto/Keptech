#include "keptech/render/graph/graph.hpp"
#include "keptech/render/renderer.hpp"
#include "wrappers/cmdBuf.hpp"

namespace kt::rdr {
  CommandBuffer RenderGraph::runPasses() { return Renderer::get().getMembers().graphicsCmdList; }

  void RenderGraph::updateDescriptors() {}
} // namespace kt::rdr