option(KT_USE_PCH ON)
option(KT_BUILD_EXAMPLES "Build example applications" OFF)
option(KT_PROFILE "Enable profiling with Tracy" OFF)
option(PIPED_BUILD "Compiler output is piped, disabled colored output" OFF)
option(KT_USE_SCCACHE "Enable sccache for compilation if available" ON)

set(LOG_LEVEL_OPTIONS "TRACE;DEBUG;INFO;WARN;ERROR;CRITICAL")
set(RHI_BACKEND_OPTIONS "Vulkan;DX12")

set(RHI_BACKEND "Vulkan" CACHE STRING "Select the renderer backend to use")

set(RHI_DEBUG_LOG_LEVEL DEBUG CACHE STRING "Set the log level for the rendererer")
set(RHI_RELEASE_LOG_LEVEL ERROR CACHE STRING "Set the log level for the rendererer in release mode")

set(RENDERER_DEBUG_LOG_LEVEL DEBUG CACHE STRING "Set the log level for the rendererer")
set(RENDERER_RELEASE_LOG_LEVEL ERROR CACHE STRING "Set the log level for the rendererer in release mode")

# Properties for options
set_property(CACHE RHI_BACKEND PROPERTY STRINGS ${RHI_BACKEND_OPTIONS})
set_property(CACHE RHI_DEBUG_LOG_LEVEL PROPERTY STRINGS ${LOG_LEVEL_OPTIONS})
set_property(CACHE RHI_RELEASE_LOG_LEVEL PROPERTY STRINGS ${LOG_LEVEL_OPTIONS})

set_property(CACHE RENDERER_DEBUG_LOG_LEVEL PROPERTY STRINGS ${LOG_LEVEL_OPTIONS})
set_property(CACHE RENDERER_RELEASE_LOG_LEVEL PROPERTY STRINGS ${LOG_LEVEL_OPTIONS})