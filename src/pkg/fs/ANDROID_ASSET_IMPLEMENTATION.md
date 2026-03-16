# Android Asset Reading Implementation

## Overview

This document describes the implementation of Android asset reading support for the `Fs` package. Assets embedded in the APK via `embedded_data` in multi_app can now be read using the Android AssetManager API.

## Implementation Status

### ✅ Completed
1. **multi_app.bzl**: Added `embedded_data` support for Android platform
   - Assets are packaged into `assets/` folder within the APK
   - Uses `droid_embedded_assets` rule similar to `multi_lib`

2. **AndroidDrive class**: Created new Drive implementation for Android assets
   - Located in `src/pkg/fs/Fs/AndroidDrive.{h,cpp}`
   - Uses Android NDK `<android/asset_manager.h>` API
   - Reads files from APK assets via `AAssetManager`

### 🔨 Integration Steps

#### 1. Update BUILD.bazel

Add AndroidDrive to the fs library build (only for Android platform):

```python
# In src/pkg/fs/BUILD.bazel
load("@tx-kit-ext//rules:multi_lib.bzl", "multi_lib")

multi_lib(
    name = "fs",
    srcs = glob(["**/*.cpp"]),
    hdrs = glob(["**/*.h"]),
    strip_include_prefix = ".",
    visibility = ["//visibility:public"],
    deps = [
        "//src/pkg/boot",
        "//src/pkg/coro",
        "@rules_cc//cc/runfiles",
    ] + select({
        "@platforms//os:android": [],  # Android NDK headers included automatically
        "//conditions:default": [],
    }),
)
```

#### 2. Update System.cpp for Android

Modify `System::GetDefaultDrive()` to return `OverlayDrive` with `AndroidDrive` on Android:

```cpp
#include "System.h"
#include "NativeDrive.h"
#include "OverlayDrive.h"
#include "Boot/Boot.h"
#include <filesystem>

#ifdef __ANDROID__
#include "AndroidDrive.h"
#include <android/native_activity.h>

// Global pointer to ANativeActivity, set by droid_glue
extern ANativeActivity* g_NativeActivity;
#endif

namespace Fs
{
    Drive& System::GetDefaultDrive()
    {
#ifdef __ANDROID__
        static OverlayDrive drive = []() {
            std::vector<Path> prefixes;
            prefixes.push_back(std::filesystem::current_path());

            const auto& args = Boot::GetDefaultArgs();
            if (args.size() >= 1) {
                auto exePath = Path(args[0]);
                prefixes.push_back(exePath.parent_path());
            }

            static NativeDrive nativeDrive(std::move(prefixes));
            static AndroidDrive androidDrive(g_NativeActivity);

            // Try Android assets first, then fall back to native filesystem
            return OverlayDrive({&androidDrive, &nativeDrive});
        }();
#else
        static auto drive = []() {
            std::vector<Path> prefixes;
            prefixes.push_back(std::filesystem::current_path());

            const auto& args = Boot::GetDefaultArgs();
            if (args.size() >= 1) {
                auto exePath = Path(args[0]);
                prefixes.push_back(exePath.parent_path());
            }

            return NativeDrive(std::move(prefixes));
        }();
#endif

        return drive;
    }
}
```

#### 3. Expose ANativeActivity in droid_glue

Update `tx-kit-ext/rules/droid/src/cpp/droid_glue.cc`:

```cpp
#include <jni.h>
#include <android/native_activity.h>

#include "droid_argv.h"
#include "droid_log.h"
#include "redirect_stdout.h"

int main(int argc, char* argv[]);

// Global ANativeActivity pointer for access from application code
ANativeActivity* g_NativeActivity = nullptr;

namespace 
{
    JNIEnv* _env = nullptr;
}

// ... existing code ...

JNIEXPORT void ANativeActivity_onCreate(ANativeActivity* activity, void* savedState, size_t savedStateSize)
{
    LOGD("onCreate: %p savedState=%p savedStateSize=%zu", activity, savedState, savedStateSize);

    g_NativeActivity = activity;  // Store globally

    if (activity->vm->AttachCurrentThread(&_env, nullptr) != JNI_OK) {
        LOGE("AttachCurrentThread failed");
        ANativeActivity_finish(activity);
        return;
    }

    activity->callbacks->onStart = onStart;
    activity->callbacks->onStop = onStop;

    redirect_stdout_to_logcat();
}
```

Add header file `tx-kit-ext/rules/droid/src/cpp/droid_activity.h`:

```cpp
#pragma once

struct ANativeActivity;

// Global pointer to ANativeActivity, available after ANativeActivity_onCreate
extern ANativeActivity* g_NativeActivity;
```

#### 4. Update droid_native BUILD target

Export the header for application use:

```python
# In tx-kit-ext/rules/droid/BUILD.bazel
cc_library(
    name = "droid_native",
    srcs = glob(["src/cpp/**/*.cc"]),
    hdrs = glob(["src/cpp/**/*.h"]),
    linkopts = [
        "-llog",
        "-landroid",
        "-Wl,--undefined=ANativeActivity_onCreate",
    ],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "droid_activity",
    hdrs = ["src/cpp/droid_activity.h"],
    visibility = ["//visibility:public"],
)
```

## How It Works

### Asset Path Resolution

1. **Embedded data** in `BUILD.bazel`:
   ```python
   multi_app(
       name = "fs",
       srcs = ["main.cpp"],
       embedded_data = [":data"],
       deps = ["//src/pkg/fs"],
   )
   ```

2. **Packaging**: Files are copied to `assets/test_data/test.txt` in the APK

3. **Reading**: Application code calls:
   ```cpp
   auto result = co_await drive.ReadAllBytesAsync("test_data/test.txt");
   ```

4. **Resolution** (with OverlayDrive):
   - First tries `AndroidDrive::ReadAllBytesAsync()` → opens from APK assets
   - If not found, falls back to `NativeDrive::ReadAllBytesAsync()` → reads from filesystem

### Advantages

- **Unified API**: Same `Drive` interface works on all platforms
- **Transparent**: Application doesn't need Android-specific code
- **Composable**: `OverlayDrive` allows mixing asset and filesystem access
- **Zero-copy**: `AASSET_MODE_BUFFER` uses memory mapping when possible

## Testing

Build and run Android demo:

```bash
bazel build --config=droid //demo/pkg/fs
bazel run --config=droid //demo/pkg/fs
```

Verify assets in APK:

```bash
unzip -l bazel-bin/demo/pkg/fs/fs-droid-apk.apk | grep assets
```

Expected output:
```
13  01-01-2010 00:00   assets/test_data/test.txt
```

## Platform Support Matrix

| Platform | embedded_data | Drive Implementation | Notes |
|----------|---------------|---------------------|-------|
| Host     | ✅ Runfiles    | NativeDrive         | Files in bazel-bin/.../runfiles |
| WASM     | ✅ Preloaded   | NativeDrive         | Emscripten preloads into virtual FS |
| Android  | ✅ Assets      | AndroidDrive        | Files in APK assets/ |

## References

- [Android NDK AAssetManager API](https://developer.android.com/ndk/reference/group/asset)
- [Bazel embedded.bzl implementation](../../tx-kit-ext/rules/embedded.bzl)
- [multi_app.bzl implementation](../../tx-kit-ext/rules/multi_app.bzl)
