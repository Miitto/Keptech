#pragma once

#include "componentArray.hpp"
#include "ecs-logger.hpp"
#include <memory>
#include <unordered_map>

namespace keptech::ecs {
  class ComponentManager {
    using TypeHash = size_t;

  public:
    ComponentManager() = default;
    ComponentManager(const ComponentManager&) = delete;
    ComponentManager& operator=(const ComponentManager&) = delete;
    ComponentManager(ComponentManager&&) noexcept = default;
    ComponentManager& operator=(ComponentManager&&) noexcept = default;
    ~ComponentManager() = default;

    template <typename T> ComponentType getComponentType() {
      TypeHash typeName = typeid(T).hash_code();

      if (componentTypes.find(typeName) == componentTypes.end()) {
        registerComponent<T>();
      }

      return componentTypes[typeName];
    }

    template <typename T>
    ComponentManager& add(EntityHandle entity, T&& component) {
      all<T>().insert(entity, std::forward<T>(component));
      return *this;
    }

    template <typename T> ComponentManager& remove(EntityHandle entity) {
      all<T>().erase(entity);
      return *this;
    }

    template <typename T> bool has(EntityHandle entity) {
      return all<T>().has(entity);
    }

    template <typename T> T& at(EntityHandle entity) {
      return all<T>().at(entity);
    }

    template <typename T> T* get(EntityHandle entity) {
      return all<T>().get(entity);
    }

    void onEntityDestroyed(EntityHandle entity) {
      for (auto& callback : deleteCallbacks) {
        callback(entity);
      }
    }

    template <typename T> ComponentArray<T>& all() {
      TypeHash typeName = typeid(T).hash_code();

      if (componentArrays.find(typeName) == componentArrays.end()) {
        return registerComponent<T>();
      }

      return *static_cast<ComponentArray<T>*>(componentArrays[typeName].get());
    }

  private:
    template <typename T> ComponentArray<T>& registerComponent() {
      ECS_DEBUG("Registering component type: {}", typeid(T).name());

      TypeHash typeName = typeid(T).hash_code();

      assert(componentTypes.find(typeName) == componentTypes.end() &&
             "Registering component type more than once.");

      componentTypes[typeName] = nextComponentTypeIndex++;

      componentArrays[typeName] = std::make_unique<ComponentArray<T>>();
      ComponentArray<T>& array =
          *static_cast<ComponentArray<T>*>(componentArrays[typeName].get());

      deleteCallbacks.push_back(array.getDeleteCallback());

      return array;
    }

    std::unordered_map<TypeHash, ComponentType> componentTypes{};
    std::unordered_map<TypeHash, std::unique_ptr<IComponentArray>>
        componentArrays;
    uint8_t nextComponentTypeIndex = 0;

    std::vector<std::function<void(EntityHandle)>> deleteCallbacks{};
  };
} // namespace keptech::ecs
