#pragma once

#ifdef _KT_GNU
#define KT_FN_ATTR(...) __attribute__((__VA_ARGS__))
#endif

#ifdef _KT_MSVC
#define KT_FN_ATTR(...)
#endif