#pragma once

#include "system.hpp"
#include <memory>
#include <unordered_map>

namespace keptech::ecs {
  class SystemManager {
    using TypeHash = size_t;

  public:
    template <typename T, typename... Args>
    T& registerSystem(Signature signature, Args&&... args) {
      TypeHash typeName = typeid(T).hash_code();

      assert(systemIndices.find(typeName) == systemIndices.end() &&
             "Registering system more than once.");

      size_t index = systems.size();

      systemIndices[typeName] = index;

      auto system = std::make_unique<T>(std::forward<Args>(args)...);
      preUpdateFunctions.push_back(system->getPreUpdateFunction());
      updateFunctions.push_back(system->getUpdateFunction());
      postUpdateFunctions.push_back(system->getPostUpdateFunction());
      systems.push_back(std::move(system));
      signatures.push_back(signature);
      ecs::System& sys = *systems[index].get();
      return static_cast<T&>(sys);
    }

    template <typename T> bool has() {
      TypeHash typeName = typeid(T).hash_code();
      return systemIndices.find(typeName) != systemIndices.end();
    }

    template <typename T> T& get() {
      TypeHash typeName = typeid(T).hash_code();

      auto found = systemIndices.find(typeName);

      assert(found != systemIndices.end() && "System used before registered.");

      return *static_cast<T*>(systems[found->second].get());
    }

    template <typename T> void setSignature(Signature signature) {
      TypeHash typeName = typeid(T).hash_code();

      auto found = systemIndices.find(typeName);

      assert(found != systemIndices.end() && "System used before registered.");

      signatures[found->second] = signature;
    }

    void onEntityDestroyed(EntityHandle entity) {
      for (auto& system : systems) {
        system->entities.erase(entity);
      }
    }

    void onEntitySignatureChanged(EntityHandle entity,
                                  const Signature& entitySignature) {
      for (size_t i = 0; i < systems.size(); ++i) {
        auto& system = systems[i];
        const auto& systemSignature = signatures[i];

        if ((entitySignature & systemSignature) == systemSignature) {
          system->entities.insert(entity);
        } else {
          system->entities.erase(entity);
        }
      }
    }

    inline void preUpdateAllSystems(const FrameData& frameData) {
      for (auto& f : preUpdateFunctions) {
        f(frameData);
      }
    }

    inline void updateAllSystems(const FrameData& frameData) {
      for (auto& f : updateFunctions) {
        f(frameData);
      }
    }

    inline void postUpdateAllSystems(const FrameData& frameData) {
      for (auto& f : postUpdateFunctions) {
        f(frameData);
      }
    }

  private:
    std::unordered_map<TypeHash, size_t>
        systemIndices{}; // Map from system type to index in systems vector
    std::vector<std::unique_ptr<System>> systems{};
    std::vector<std::function<void(const FrameData&)>> preUpdateFunctions{};
    std::vector<std::function<void(const FrameData&)>> updateFunctions{};
    std::vector<std::function<void(const FrameData&)>> postUpdateFunctions{};
    std::vector<Signature> signatures{};
  };
} // namespace keptech::ecs
