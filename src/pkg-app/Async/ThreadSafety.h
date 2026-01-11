#pragma once

#if defined(__clang__) && __has_attribute(guarded_by)

// Thread Safety Analysis annotations (Clang only)
#define ASYNC_CAPABILITY(x) __attribute__((capability(x)))
#define ASYNC_SCOPED_CAPABILITY __attribute__((scoped_lockable))
#define ASYNC_GUARDED_BY(x) __attribute__((guarded_by(x)))
#define ASYNC_PT_GUARDED_BY(x) __attribute__((pt_guarded_by(x)))
#define ASYNC_REQUIRES(...) __attribute__((requires_capability(__VA_ARGS__)))
#define ASYNC_EXCLUDES(...) __attribute__((locks_excluded(__VA_ARGS__)))
#define ASYNC_ACQUIRE(...) __attribute__((acquire_capability(__VA_ARGS__)))
#define ASYNC_RELEASE(...) __attribute__((release_capability(__VA_ARGS__)))

#else

//TODO: add support for MSVC Thread Safety Analysis
//#ifdef _MSC_VER
//#include <sal.h>
//#define ASYNC_GUARDED_BY(x) _Guarded_by_(x)
//#define ASYNC_REQUIRES(...) _Requires_lock_held_(__VA_ARGS__)
//#define ASYNC_EXCLUDES(...) _Acquires_lock_(__VA_ARGS__)
//#define ASYNC_ACQUIRE(...) _Acquires_lock_(__VA_ARGS__)
//#define ASYNC_RELEASE(...) _Releases_lock_(__VA_ARGS__)

#define ASYNC_CAPABILITY(x)
#define ASYNC_SCOPED_CAPABILITY
#define ASYNC_GUARDED_BY(x)
#define ASYNC_PT_GUARDED_BY(x)
#define ASYNC_REQUIRES(...)
#define ASYNC_EXCLUDES(...)
#define ASYNC_ACQUIRE(...)
#define ASYNC_RELEASE(...)

#endif
