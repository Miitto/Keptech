#ifdef KT_PROFILE
#include <tracy/Tracy.hpp>

#define KT_PROFILE_SCOPE(name) ZoneScopedN(name)
#define KT_PROFILE_FUNCTION ZoneScoped
#define KT_PROFILE_SCOPE_COLOR(color) ZoneScopedC(color)
#define KT_MARK_FRAME FrameMark
#else
#define KT_PROFILE_SCOPE(name)
#define KT_PROFILE_FUNCTION
#define KT_PROFILE_SCOPE_COLOR(color)
#define KT_MARK_FRAME
#endif
