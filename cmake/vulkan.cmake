function(find_vulkan VERSION)
  find_package(Vulkan REQUIRED)

  target_compile_definitions(Vulkan::Vulkan INTERFACE
    VULKAN_HPP_CPP_VERSION=${VERSION}
    VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
    VULKAN_HPP_NO_STRUCT_CONSTRUCTORS=1
    VULKAN_HPP_NO_CONSTRUCTORS=1
    VULKAN_HPP_NO_EXCEPTIONS=1
    VULKAN_HPP_RAII_NO_EXCEPTIONS=1
    VK_NO_PROTOTYPES=1
)

  set_target_properties(Vulkan::Vulkan PROPERTIES
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
      "${Vulkan_INCLUDE_DIR}/vma"
      "${Vulkan_INCLUDE_DIR}/Volk"
  )

  if (KT_USE_PCH)
    target_precompile_headers(${target} ${ACCESS}
      <Volk/volk.h>
    )
  endif()
endfunction()
