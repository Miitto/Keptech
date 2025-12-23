#pragma once

#include "system.hpp"
#include <memory>
#include <unordered_map>

namespace keptech::ecs {
  class SystemManager {
    using TypeHash = size_t;

  public:
    template <typename T> T& registerSystem(Signature signature = {}) {
      TypeHash typeName = typeid(T).hash_code();

      assert(systems.find(typeName) == systems.end() &&
             "Registering system more than once.");

      auto system = std::make_unique<T>();
      systems[typeName] = std::move(system);
      signatures[typeName] = signature;
      return *systems[typeName].get();
    }

    template <typename T, typename... Args>
    T& registerSystem(Signature signature, Args&&... args) {
      TypeHash typeName = typeid(T).hash_code();

      assert(systems.find(typeName) == systems.end() &&
             "Registering system more than once.");

      auto system = std::make_unique<T>(std::forward<Args>(args)...);
      systems[typeName] = std::move(system);
      signatures[typeName] = signature;
      ecs::System& sys = *systems[typeName].get();
      return static_cast<T&>(sys);
    }

    template <typename T> bool has() {
      TypeHash typeName = typeid(T).hash_code();
      return systems.find(typeName) != systems.end();
    }

    template <typename T> T& get() {
      TypeHash typeName = typeid(T).hash_code();

      assert(systems.find(typeName) != systems.end() &&
             "System used before registered.");

      return *static_cast<T*>(systems[typeName].get());
    }

    template <typename T> void setSignature(Signature signature) {
      TypeHash typeName = typeid(T).hash_code();

      assert(systems.find(typeName) != systems.end() &&
             "System used before registered.");

      signatures[typeName] = signature;
    }

    void onEntityDestroyed(EntityHandle entity) {
      for (auto& [_, system] : systems) {
        system->entities.erase(entity);
      }
    }

    void onEntitySignatureChanged(EntityHandle entity,
                                  const Signature& entitySignature) {
      for (auto& [type, system] : systems) {
        const auto& systemSignature = signatures[type];

        if ((entitySignature & systemSignature) == systemSignature) {
          system->entities.insert(entity);
        } else {
          system->entities.erase(entity);
        }
      }
    }

    inline void preUpdateAllSystems(const FrameData& frameData) {
      for (auto& [_, system] : systems) {
        system->preUpdate(frameData);
      }
    }

    inline void updateAllSystems(const FrameData& frameData) {
      for (auto& [_, system] : systems) {
        system->onUpdate(frameData);
      }
    }

    inline void postUpdateAllSystems(const FrameData& frameData) {
      for (auto& [_, system] : systems) {
        system->postUpdate(frameData);
      }
    }

  private:
    std::unordered_map<TypeHash, std::unique_ptr<System>> systems{};
    std::unordered_map<TypeHash, Signature> signatures{};
  };
} // namespace keptech::ecs
