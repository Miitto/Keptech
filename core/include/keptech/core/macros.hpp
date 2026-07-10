#pragma once

#define BIT(N) (1U << (N))

#ifdef __clang__
#define CLANG_IGNORE_WARNING_PUSH _Pragma("clang diagnostic push") _Pragma("clang diagnostic ignored \"-Wall\"") // NOLINTBEGIN
#define CLANG_IGNORE_WARNING_POP _Pragma("clang diagnostic pop")                                                 // NOLINTEND
#else
#define CLANG_IGNORE_WARNING_PUSH
#define CLANG_IGNORE_WARNING_POP
#endif