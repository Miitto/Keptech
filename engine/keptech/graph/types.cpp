#include "types.hpp"

namespace kt {
  bool BufferInfo::isHostAccessible() const { return type != rhi::BufferType::Default; }
  void LifetimeRange::readFrom(size_t passIndex) {
    firstReadPass = std::min(firstReadPass, passIndex);
    lastReadPass = std::max(lastReadPass, passIndex);
  }
  void LifetimeRange::writtenFrom(size_t passIndex) {
    firstWritePass = std::min(firstWritePass, passIndex);
    lastWritePass = std::max(lastWritePass, passIndex);
  }

  bool LifetimeRange::hasReads() const { return firstReadPass <= lastReadPass; }
  bool LifetimeRange::hasWrites() const { return firstWritePass <= lastWritePass; }
  bool LifetimeRange::used() const { return hasReads() || hasWrites(); }
  bool LifetimeRange::canBeCombined() const { return !(blocked || (hasReads() && hasWrites() && firstReadPass <= firstWritePass)); }
  size_t LifetimeRange::lastUsedPass() const { return std::max(lastReadPass, lastWritePass); }
  size_t LifetimeRange::firstUsedPass() const { return std::min(firstReadPass, firstWritePass); }
  bool LifetimeRange::canCombineWith(const LifetimeRange& other) const {
    if (!used() || !other.used() || !canBeCombined() || !other.canBeCombined())
      return false;

    bool thisBeforeOther = lastUsedPass() < other.firstUsedPass();
    bool otherBeforeThis = other.lastUsedPass() < firstUsedPass();

    return thisBeforeOther || otherBeforeThis;
  }
  void LifetimeRange::mergeWith(const LifetimeRange& other) {
    firstReadPass = std::min(firstReadPass, other.firstReadPass);
    lastReadPass = std::max(lastReadPass, other.lastReadPass);
    firstWritePass = std::min(firstWritePass, other.firstWritePass);
    lastWritePass = std::max(lastWritePass, other.lastWritePass);
    blocked |= other.blocked;
  }

} // namespace kt