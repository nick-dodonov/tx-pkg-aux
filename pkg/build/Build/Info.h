#pragma once

namespace Build
{
    /// Provides build-time information about the binary
    class Info
    {
    public:
        /// Get the git commit SHA (full)
        static const char* GitSha();

        /// Get the git branch name
        static const char* GitBranch();

        /// Get the build timestamp (RFC 3339 format)
        static const char* BuildTime();

        /// Get the git status (clean/dirty)
        static const char* GitStatus();

        /// Get the build user
        static const char* BuildUser();

        /// Get the build host
        static const char* BuildHost();
    };
}
