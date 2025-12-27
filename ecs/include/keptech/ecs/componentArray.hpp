#pragma once

#include "base.hpp"
#include <unordered_map>

namespace keptech::ecs {

  class IComponentArray {};

  template <typename T> class ComponentArray : public IComponentArray {
  public:
    ComponentArray() { components.reserve(MAX_ENTITIES); }

    void insert(EntityHandle entity, T&& component) {
      assert(entityToIndexMap.find(entity) == entityToIndexMap.end() &&
             "Component added to same entity more than once.");

      size_t newIndex = components.size();
      assert(newIndex < MAX_ENTITIES &&
             "Too many components stored in ComponentArray.");
      components.emplace_back(std::move(component));

      entityToIndexMap[entity] = newIndex;
      indexToEntityMap[newIndex] = entity;
    }

    void erase(EntityHandle entity) {
      assert(entityToIndexMap.find(entity) != entityToIndexMap.end() &&
             "Removing non-existent component.");

      size_t indexOfRemovedEntity = entityToIndexMap[entity];
      assert(indexOfRemovedEntity < components.size() && "Attempting to erase "
                                                         "component at invalid "
                                                         "index.");
      size_t indexOfLastElement = components.size() - 1;
      components[indexOfRemovedEntity] =
          std::move(components[indexOfLastElement]);

      EntityHandle entityOfLastElement = indexToEntityMap[indexOfLastElement];
      entityToIndexMap[entityOfLastElement] = indexOfRemovedEntity;
      indexToEntityMap[indexOfRemovedEntity] = entityOfLastElement;

      components.pop_back();
      entityToIndexMap.erase(entity);
      indexToEntityMap.erase(indexOfLastElement);
    }

    [[nodiscard]] bool has(EntityHandle entity) const {
      return entityToIndexMap.find(entity) != entityToIndexMap.end();
    }

    [[nodiscard]] T& at(EntityHandle entity) {
      assert(entityToIndexMap.find(entity) != entityToIndexMap.end() &&
             "Retrieving non-existent component.");

      return components[entityToIndexMap[entity]];
    }

    [[nodiscard]] T* get(EntityHandle entity) {
      if (!entityToIndexMap.contains(entity)) {
        return nullptr;
      }

      return &components[entityToIndexMap[entity]];
    }

    [[nodiscard]] T& operator[](EntityHandle entity) { return at(entity); }

    auto getDeleteCallback() {
      return [this](EntityHandle entity) { this->erase(entity); };
    }

    [[nodiscard]] size_t size() const { return components.size(); }

    [[nodiscard]] const std::vector<T>& getAllComponents() const {
      return components;
    }

    std::vector<T>::const_iterator begin() const { return components.cbegin(); }
    std::vector<T>::const_iterator end() const { return components.cend(); }

  private:
    std::vector<T> components;
    std::unordered_map<EntityHandle, size_t> entityToIndexMap;
    std::unordered_map<size_t, EntityHandle> indexToEntityMap;
  };
} // namespace keptech::ecs
