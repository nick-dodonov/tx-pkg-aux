"""Common utilities for tx_binary and tx_test.

This module provides shared functionality for creating multi-platform targets
that work for both native and WASM platforms:

- get_default_linkopts(): Platform-specific linker options
- merge_linkopts(): Merges user and default linkopts  
- create_wasm_targets(): Creates WASM binary and runner targets
- create_platform_alias(): Creates platform-specific aliases

Usage:
- tx_binary(): Creates executable targets 
- tx_test(): Creates test targets that can run in WASM

Both support:
- Native execution: bazel run //target:name
- WASM execution: bazel run //target:name --platforms=@emsdk//:platform_wasm
- Test WASM execution: bazel run //test:name-run --platforms=@emsdk//:platform_wasm
"""
load("@rules_shell//shell:sh_binary.bzl", "sh_binary")
load("@emsdk//emscripten_toolchain:wasm_rules.bzl", "wasm_cc_binary")

def get_default_copts():
    """Returns default platform-specific copts for tx targets."""
    return select({
        "@platforms//cpu:wasm32": [
            # Keep exceptions disabled for WASM - causes issues with shared memory
            "-fno-exceptions",
            # Disable threading support to avoid shared memory issues
            "-mthread-model", "single",
        ],
        "//conditions:default": [],
    })

def get_default_linkopts():
    """Returns default platform-specific linkopts for tx targets."""
    return select({
        "@platforms//cpu:wasm32": [
            "--oformat=html",
            "--emrun",  # Поддержка emrun для headless запуска
            # Need for exit(status)
            # - https://emscripten.org/docs/api_reference/emscripten.h.html#c.emscripten_force_exit
            # - https://emscripten.org/docs/getting_started/FAQ.html#what-does-exiting-the-runtime-mean-why-don-t-atexit-s-run
            # - https://github.com/emscripten-core/emscripten/blob/main/src/settings.js
            "-sEXIT_RUNTIME=1",
            # Disable threading and shared memory completely
            "-sUSE_PTHREADS=0",
            "-sPROXY_TO_PTHREAD=0",
        ],
        # "@platforms//os:windows": [
        #     "/SUBSYSTEM:CONSOLE",
        # ],
        # "@platforms//os:macos": [
        #     "-framework", "CoreFoundation",
        #     "-lc++",
        # ],
        "//conditions:default": [],
    })

def merge_copts(user_copts):
    """Merges user copts with default platform-specific ones."""
    return get_default_copts() + user_copts

def merge_linkopts(user_linkopts):
    """Merges user linkopts with default platform-specific ones."""
    return get_default_linkopts() + user_linkopts

def create_wasm_targets(name, cc_target_name, testonly = False):
    """Creates WASM-specific targets (wasm_cc_binary and runner).
    
    Args:
        name: Base name for the targets.
        cc_target_name: Name of the cc target to wrap.
        testonly: Whether targets should be marked as testonly.
    """
    # WASM-специфичные цели создаются только для WASM платформы
    wasm_cc_binary(
        name = name + "-wasm",
        cc_target = ":" + cc_target_name,
        visibility = ["//visibility:public"],
        target_compatible_with = ["@platforms//cpu:wasm32"],
        testonly = testonly,
    )

    # WASM запуск в браузере через emrun и получение stdout/exit code
    sh_binary(
        name = name + "-wasm-runner",
        srcs = ["//tools/bazel/builder:run-wasm.sh"],
        args = ["$(execpaths :" + name + "-wasm)"],
        data = [":" + name + "-wasm"],
        visibility = ["//visibility:public"],
        target_compatible_with = ["@platforms//cpu:wasm32"],
        testonly = testonly,
    )

def create_platform_alias(name, native_target_suffix, testonly = False):
    """Creates platform-specific alias for native vs WASM execution.
    
    Args:
        name: Name of the alias target.
        native_target_suffix: Suffix for the native target (e.g., "-bin" or "-test").
        testonly: Whether the alias target should be marked as testonly.
    """
    native.alias(
        name = name,
        actual = select({
            "@platforms//cpu:wasm32": ":" + name + "-wasm-runner",
            "//conditions:default": ":" + name + native_target_suffix,
        }),
        visibility = ["//visibility:public"],
        testonly = testonly,
    )