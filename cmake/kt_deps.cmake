include(ktx)
include(glm)
include(sdl)
include(slang)
include(spdlog)
include(mesh_optimizer)
include(fastgltf)
include(entt)

add_subdirectory(vendor/imgui imgui)
add_subdirectory(vendor/stb stb)

add_library(Keptech_Deps INTERFACE)
add_library(keptech::deps ALIAS Keptech_Deps)
target_link_libraries(Keptech_Deps INTERFACE
  imgui::imgui
  spdlog::spdlog
  glm::glm
  EnTT::EnTT
  stb::stb
  kt::ktx
  meshoptimizer
  fastgltf::fastgltf
)
target_compile_definitions(Keptech_Deps INTERFACE GLM_FORCE_RADIANS GLM_FORCE_DEPTH_ZERO_TO_ONE GLM_FORCE_LEFT_HANDED)

if (KT_PROFILE)
  include(tracy)
  target_compile_definitions(Keptech_Deps INTERFACE TRACY_ENABLE KT_PROFILE)
  target_link_libraries(Keptech_Deps INTERFACE TracyClient)
endif()

if(RENDERER STREQUAL "Vulkan")
  include(vulkan)
  find_vulkan(23)
  link_vulkan(Keptech_Deps INTERFACE)
  target_compile_definitions(Keptech_Deps INTERFACE KT_VULKAN=1)
endif()