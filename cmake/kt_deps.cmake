foreach (lang IN ITEMS C CXX)
  set("CMAKE_${lang}_CLANG_TIDY_save" "${CMAKE_${lang}_CLANG_TIDY}")
  set("CMAKE_${lang}_CLANG_TIDY" "")
endforeach ()

include(ktx)
include(glm)
include(sdl)
include(slang)
include(spdlog)
include(mesh_optimizer)
include(fastgltf)
include(entt)

add_library(Keptech_Deps INTERFACE)
add_library(keptech::deps ALIAS Keptech_Deps)

if (KT_PROFILE)
  include(tracy)
  target_compile_definitions(Keptech_Deps INTERFACE TRACY_ENABLE KT_PROFILE)
  target_link_libraries(Keptech_Deps INTERFACE TracyClient)
endif()

if(RHI_BACKEND STREQUAL "Vulkan")
  include(vulkan)
  find_vulkan(23)
  link_vulkan(Keptech_Deps INTERFACE)
  target_compile_definitions(Keptech_Deps INTERFACE KT_VULKAN=1 KT_RHI_STRING="Vulkan")
elseif(RHI_BACKEND STREQUAL "DX12")
  include(d3d12memalloc)
  target_link_libraries(Keptech_Deps INTERFACE D3D12MemoryAllocator d3d12.lib dxgi.lib d3dcompiler.lib)
  target_compile_definitions(Keptech_Deps INTERFACE KT_DX12=1 KT_RHI_STRING="DX12")
endif()

add_subdirectory(vendor/imgui imgui SYSTEM)
add_subdirectory(vendor/stb stb SYSTEM)

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

foreach (lang IN ITEMS C CXX)
  set("CMAKE_${lang}_CLANG_TIDY" "${CMAKE_${lang}_CLANG_TIDY_save}")
endforeach ()