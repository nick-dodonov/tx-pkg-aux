#pragma once

#if defined(__GNUC__) || defined(__clang__) || defined(__INTEL_COMPILER)
#  define CROSS_NOINLINE __attribute__((noinline))
#elif defined(_MSC_VER)
#  define CROSS_NOINLINE __declspec(noinline)
#else
#  define CROSS_NOINLINE
#endif
