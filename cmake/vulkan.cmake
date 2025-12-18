function(find_vulkan VERSION)
  find_package(Vulkan REQUIRED)

  FetchContent_Declare(VulkanMemoryAllocator-Hpp
    GIT_REPOSITORY https://github.com/YaaZ/VulkanMemoryAllocator-Hpp.git
    GIT_TAG v3.2.1
    SYSTEM
  )
  FetchContent_MakeAvailable(VulkanMemoryAllocator-Hpp)

  target_compile_definitions(Vulkan::Vulkan INTERFACE
    VULKAN_HPP_CPP_VERSION=${VERSION}
    VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
    VULKAN_HPP_NO_STRUCT_CONSTRUCTORS=1
    VULKAN_HPP_NO_CONSTRUCTORS=1
    VULKAN_HPP_NO_EXCEPTIONS=1
    VULKAN_HPP_RAII_NO_EXCEPTIONS=1
)

  set_target_properties(Vulkan::Vulkan PROPERTIES
    CXX_STANDARD ${VERSION}
    CXX_EXTENSIONS OFF
    CXX_STANDARD_REQUIRED ON
  )

  set_target_properties(VulkanMemoryAllocator-Hpp PROPERTIES
    CXX_STANDARD ${VERSION}
    CXX_EXTENSIONS OFF
    CXX_STANDARD_REQUIRED ON
  )
endfunction()

function(link_vulkan target ACCESS)
  target_link_libraries(${target} ${ACCESS}
    Vulkan::Vulkan
  )
  target_include_directories(${target} ${ACCESS}
      "${Vulkan_INCLUDE_DIR}"
  )

  target_precompile_headers(${target} ${ACCESS}
    <vulkan/vulkan_raii.hpp>
  )
endfunction()

function(link_vma target ACCESS)
  target_link_libraries(${target} ${ACCESS}
    VulkanMemoryAllocator-Hpp::VulkanMemoryAllocator-Hpp
  )
endfunction()
