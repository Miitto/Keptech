#pragma once

#include <keptech/core/profile.hpp>

#ifdef KT_PROFILE
#include <Volk/volk.h>

#include <keptech/vulkan/constants.hpp>

#include <tracy/TracyVulkan.hpp>

#define KT_VK_CONTEXT(_PHYS_DEV, _LOG_DEV)                                                                                                 \
  TracyVkContextHostCalibrated(_PHYS_DEV, _LOG_DEV, vkResetQueryPool, kt::vkh::ext::vkGetPhysicalDeviceCalibrateableTimeDomainsEXT,        \
                               kt::vkh::ext::vkGetCalibratedTimestampsEXT)
#define KT_VK_CONTEXT_NAME(_CTX, _NAME) TracyVkContextName(_CTX, _NAME, sizeof(_NAME) - 1)
#define KT_VK_CONTEXT_DESTROY(_CTX) TracyVkDestroy(_CTX)
#define KT_VK_ZONE(_CTX, _CMD, _NAME) TracyVkZone(_CTX, _CMD, _NAME)
#define KT_VK_COLLECT(_CTX, _CMD) TracyVkCollect(_CTX, _CMD)
#else
#define KT_VK_CONTEXT(_PHYS_DEV, _LOG_DEV, _QUEUE, _CMD_BUF) ((void)0)
#define KT_VK_CONTEXT_DESTROY(_CTX) ((void)0)
#define KT_VK_ZONE(_CTX, _CMD, _NAME) ((void)0)
#define KT_VK_COLLECT(_CTX, _CMD) ((void)0)
#endif
