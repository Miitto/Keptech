#pragma once

#include "keptech/physics/impls.hpp"
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>

namespace kt::phys {
  struct PhysicsConfig {
    JPH::uint maxBodies = 1024;
    JPH::uint numBodyMutexes = 0;
    JPH::uint maxBodyPairs = 1024;
    JPH::uint maxContactConstraints = 1024;
  };

  class PhysicsSystem {
  public:
    PhysicsSystem(PhysicsConfig config = {});

    static void initialize();

  private:
    BPLayerInterface bpLayerInterface{};
    ObjectVsBroadPhaseLayerFilter objVsBpFilter{};
    ObjectLayerPairFilter objVsObjFilter{};
    JPH::PhysicsSystem physicsSystem{};
    JPH::TempAllocatorImpl tempAllocator;
    JPH::JobSystemThreadPool jobSystem;
  };
} // namespace kt::phys