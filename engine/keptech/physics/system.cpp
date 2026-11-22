#include "system.hpp"
#include "phys-logger.hpp"
#include <cstdarg>

namespace kt::phys {
  namespace {

    [[gnu::format(printf, 1, 2)]] void TraceImpl(const char* inFMT, ...) {
      va_list list{};
      va_start(list, inFMT);
      std::array<char, 1024> buffer{};
      vsnprintf(buffer.data(), buffer.size(), inFMT, list);
      va_end(list);
      PHYS_TRACE("{}", buffer);
    }

#ifdef JPH_ENABLE_ASSERTS
    bool AssertFailedImpl(const char* inExpression, const char* inMessage, const char* inFile, JPH::uint inLine) {
      PHYS_CRITICAL("{}:{}:({}) ", inFile, inLine, inExpression, inMessage != nullptr ? inMessage : "");
      return true;
    }
#endif
  } // namespace

  void PhysicsSystem::initialize() {
    static bool init = false;
    if (init) {
      PHYS_WARN("Physics system already initialized");
      return;
    }
    JPH::RegisterDefaultAllocator();
    JPH::Trace = TraceImpl;
    JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = AssertFailedImpl;)
    JPH::Factory::sInstance = new JPH::Factory(); // NOLINT
    JPH::RegisterTypes();

    init = true;
  }

  PhysicsSystem::PhysicsSystem(PhysicsConfig config)
      : tempAllocator(10ull * 1024ull * 1024ull),
        jobSystem(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers,
                  static_cast<int>(std::thread::hardware_concurrency() == 0u ? 0u : std::thread::hardware_concurrency() - 1u)) {
    physicsSystem.Init(config.maxBodies, config.numBodyMutexes, config.maxBodyPairs, config.maxContactConstraints, bpLayerInterface,
                       objVsBpFilter, objVsObjFilter);
  }
} // namespace kt::phys