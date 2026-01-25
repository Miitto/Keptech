#pragma once

#include "keptech/renderer.hpp"

namespace keptech {

  void Renderer::render() { backend->endFrame(); }
} // namespace keptech
