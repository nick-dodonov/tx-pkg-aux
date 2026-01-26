// Bazel linkstamp file - automatically recompiled on every link
// This file has access to workspace status variables via preprocessor macros
//
// Bazel automatically includes these files during linkstamp compilation:
// - volatile_file.h: BUILD_TIMESTAMP, BUILD_SCM_REVISION, BUILD_SCM_STATUS
// - non_volatile_file.h: BUILD_USER, BUILD_HOST, BUILD_EMBED_LABEL

#include "Info.h"

// Bazel-provided macros from workspace_status command
// These are automatically defined via included headers

#ifndef BUILD_SCM_REVISION
#define BUILD_SCM_REVISION "0"
#endif

#ifndef BUILD_SCM_STATUS
#define BUILD_SCM_STATUS ""
#endif

#ifndef BUILD_TIMESTAMP
#define BUILD_TIMESTAMP unknown
#endif

#ifndef BUILD_USER
#define BUILD_USER "unknown"
#endif

#ifndef BUILD_HOST
#define BUILD_HOST "unknown"
#endif

// Git branch - попробуем получить из BUILD_EMBED_LABEL или используем git напрямую
#ifndef BUILD_GIT_BRANCH
#ifdef BUILD_EMBED_LABEL
#define BUILD_GIT_BRANCH BUILD_EMBED_LABEL
#else
#define BUILD_GIT_BRANCH "unknown"
#endif
#endif

// Helper to stringify macro values
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

namespace Build
{
    const char* Info::GitSha()
    {
        return BUILD_SCM_REVISION;
    }

    const char* Info::GitBranch()
    {
        return BUILD_GIT_BRANCH;
    }

    const char* Info::BuildTime()
    {
        return TOSTRING(BUILD_TIMESTAMP);
    }

    const char* Info::GitStatus()
    {
        return BUILD_SCM_STATUS;
    }

    const char* Info::BuildUser()
    {
        return BUILD_USER;
    }

    const char* Info::BuildHost()
    {
        return BUILD_HOST;
    }
}
