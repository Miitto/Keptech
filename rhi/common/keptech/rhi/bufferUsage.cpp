#include "bufferUsage.hpp"

#include <spdlog/fmt/bundled/ranges.h>

namespace kt::rhi {
  RawBufferUsage raw(BufferUsage usage) { return static_cast<RawBufferUsage>(static_cast<uint32_t>(usage) & 0xFFFF); }
} // namespace kt::rhi

fmt::format_context::iterator fmt::formatter<kt::rhi::BufferUsage>::format(const kt::rhi::BufferUsage& usage,
                                                                           fmt::format_context& ctx) const {
  std::string_view name;
  switch (usage) {
  case kt::rhi::BufferUsage::None:
    name = "None";
    break;
  case kt::rhi::BufferUsage::Vertex:
    name = "Vertex";
    break;
  case kt::rhi::BufferUsage::Index:
    name = "Index";
    break;
  case kt::rhi::BufferUsage::Uniform:
    name = "Uniform";
    break;
  case kt::rhi::BufferUsage::Storage:
    name = "Storage";
    break;
  case kt::rhi::BufferUsage::Indirect:
    name = "Indirect";
    break;
  case kt::rhi::BufferUsage::TransferSrc:
    name = "TransferSrc";
    break;
  case kt::rhi::BufferUsage::TransferDst:
    name = "TransferDst";
    break;
  }
  return fmt::formatter<std::string_view>::format(name, ctx);
}

fmt::format_context::iterator fmt::formatter<kt::Bitflag<kt::rhi::BufferUsage>>::format(const kt::Bitflag<kt::rhi::BufferUsage>& usage,
                                                                                        fmt::format_context& ctx) const {
  std::string_view name;
  if (usage == kt::rhi::BufferUsage::None) {
    name = "None";
    return fmt::formatter<std::string_view>::format(name, ctx);
  }

  using U = kt::rhi::BufferUsage;

  std::vector<std::string_view> names;
  if (usage.has(U::Vertex)) {
    names.push_back("Vertex");
  }
  if (usage.has(U::Index)) {
    names.push_back("Index");
  }
  if (usage.has(U::Uniform)) {
    names.push_back("Uniform");
  }
  if (usage.has(U::Storage)) {
    names.push_back("Storage");
  }
  if (usage.has(U::Indirect)) {
    names.push_back("Indirect");
  }
  if (usage.has(U::TransferSrc)) {
    names.push_back("TransferSrc");
  }
  if (usage.has(U::TransferDst)) {
    names.push_back("TransferDst");
  }

  return fmt::format_to(ctx.out(), "{}", fmt::join(names, " | "));
}
