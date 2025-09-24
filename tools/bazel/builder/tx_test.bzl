"""Starlark build definitions for tx_test using cc_test."""
load("@rules_cc//cc:cc_test.bzl", "cc_test")
load(":tx_common.bzl", "merge_copts", "merge_linkopts", "create_wasm_targets")

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

    # Main test target - works for both native and WASM platforms
    cc_test(
        name = name,
        copts = merged_copts,
        linkopts = merged_linkopts,
        *args,
        **kwargs,
    )

    # Create WASM runner for manual execution in browser
    create_wasm_targets(name + "-run", name, testonly = True)
