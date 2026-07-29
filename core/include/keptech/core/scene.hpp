#pragma once

#include <keptech/components/name.hpp>
#include <keptech/ecs/entity.hpp>

namespace kt {
  class Scene {
  public:
    ecs::Ecs& getEcs();
    [[nodiscard]] const ecs::Ecs& getEcs() const;

    ecs::Entity createEntity(const std::string& name = "");

    void useCamera(ecs::EntityHandle cameraEntity);

    [[nodiscard]] ecs::Entity getActiveCamera();

    template <typename Component, typename FRef> static void registerComponentFunctions(const char* functionName) {
      entt::meta_factory<Component>().template func<FRef>(entt::hashed_string(functionName));
    }

    template <typename... T> auto view() { return ecs.view<T...>(); }

    void clear();

    static Scene& active();

    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;
    Scene(Scene&&) = delete;
    Scene& operator=(Scene&&) = delete;
    ~Scene() = default;

  private:
    Scene() = default;
    static Scene s_active;

    ecs::Ecs ecs{};
    ecs::EntityHandle activeCamera = ecs::INVALID_ENTITY_HANDLE;
  };
} // namespace kt
