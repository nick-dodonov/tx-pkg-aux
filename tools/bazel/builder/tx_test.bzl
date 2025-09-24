"""Starlark build definitions for tx_test using cc_binary."""
load("@rules_cc//cc:cc_binary.bzl", "cc_binary")
load("@rules_shell//shell:sh_test.bzl", "sh_test")
load("@emsdk//emscripten_toolchain:wasm_rules.bzl", "wasm_cc_binary")
load(":tx_common.bzl", "merge_copts", "merge_linkopts")

def tx_test(name, *args, **kwargs):
    """Creates a multi-platform test target that works for native and WASM platforms.
    
    The main test target works directly for both native and WASM platforms.
    For WASM, it compiles to WASM binary and can be executed in browser.
    
    Usage:
    - Native: bazel test //test:name
    - WASM: bazel test //test:name --platforms=@emsdk//:platform_wasm
    - WASM run: bazel run //test:name-run --platforms=@emsdk//:platform_wasm
    
    Args:
        name: The name of the target.
        *args: Additional arguments passed to cc_test.
        **kwargs: Additional keyword arguments passed to cc_test.
    """

    # Merge user options with defaults
    user_copts = kwargs.pop("copts", [])
    user_linkopts = kwargs.pop("linkopts", [])
    merged_copts = merge_copts(user_copts)
    merged_linkopts = merge_linkopts(user_linkopts)
    
    # Extract test-specific kwargs that don't apply to cc_binary
    test_size = kwargs.pop("size", "small")
    test_timeout = kwargs.pop("timeout", None)
    test_flaky = kwargs.pop("flaky", None)
    test_shard_count = kwargs.pop("shard_count", None)

    # Native test binary - not a test target itself, just a compilation target
    cc_binary(
        name = name + "-native",
        copts = merged_copts,
        linkopts = merged_linkopts,
        testonly = True,
        visibility = ["//visibility:private"],
        *args,
        **kwargs,
    )

    # Extract WASM results for execution via emrun
    wasm_cc_binary(
        name = name + "-wasm",
        cc_target = ":" + name + "-native",
        target_compatible_with = ["@platforms//cpu:wasm32"],
        visibility = ["//visibility:public"],
        testonly = True,
    )

    # Main test wrapper - works for both native and WASM platforms
    sh_test(
        name = name,
        srcs = select({
            "@platforms//cpu:wasm32": ["//tools/bazel/builder:run-wasm.sh"],
            "//conditions:default": [":" + name + "-native"],
        }),
        args = select({
            "@platforms//cpu:wasm32": ["$(execpaths :" + name + "-wasm)"],
            "//conditions:default": [],
        }),
        data = select({
            "@platforms//cpu:wasm32": [":" + name + "-wasm"],
            "//conditions:default": [":" + name + "-native"],
        }),
        testonly = True,
        size = test_size,
        visibility = ["//visibility:public"],
    )
