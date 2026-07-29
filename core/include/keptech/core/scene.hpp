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

    /// @brief Sets the active scene.
    /// @param scene The scene to set as active.
    /// @return The previous active scene.
    static Scene setActive(Scene&& scene);

    static Scene& active();

  private:
    static Scene s_active;

    ecs::Ecs ecs{};
    ecs::EntityHandle activeCamera;
  };
} // namespace kt
