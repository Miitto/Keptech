#include "keptech/render/graph/graph.hpp"
#include "keptech/render/renderer.hpp"
#include "wrappers/cmdBuf.hpp"

namespace kt::rdr {
  CommandBuffer RenderGraph::runPasses() {
    auto& r = Renderer::get();
    auto& m = r.getMembers();

    m.commandLists.compute->Close();

    return m.commandLists.graphics;
  }

  void RenderGraph::updateDescriptors() {}
} // namespace kt::rdr