#pragma once

#include "base.hpp"
#include "entity.hpp"
#include <queue>
#include <string>

namespace keptech::ecs {
  class EntityManager {
  public:
    Entity& create(const std::string& name) {
      if (!freedEntities.empty()) {
        EntityHandle entity = freedEntities.front();
        freedEntities.pop();
        entities[entity] = Entity(entity, name);
        return entities[entity];
      } else {
        assert(nextEntity < MAX_ENTITIES && "Too many entities in existence.");
        entities[nextEntity] = Entity(nextEntity, name);
        nextEntity++;
        return entities[nextEntity - 1];
      }
    }

    void destroy(EntityHandle entity) {
      assert(entity < nextEntity && "Entity out of range.");

      entities[entity].onDestroy();

      freedEntities.push(entity);
    }

    Entity& at(EntityHandle entity) {
      assert(entity < nextEntity && "Entity out of range.");
      return entities[entity];
    }

    Entity* get(EntityHandle entity) {
      if (entity >= nextEntity || !entities[entity].isValid()) {
        return nullptr;
      }
      return &entities[entity];
    }

    [[nodiscard]] uint16_t getEntityCount() const {
      return nextEntity - static_cast<uint16_t>(freedEntities.size());
    }

    bool has(EntityHandle entity) {
      if (entity >= nextEntity)
        return false;
      return entities[entity].isValid();
    }

  private:
    EntityHandle nextEntity = 0;
    std::array<Entity, MAX_ENTITIES> entities{};
    std::queue<EntityHandle> freedEntities{};
  };
} // namespace keptech::ecs
