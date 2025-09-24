"""Starlark build definitions for tx_test using cc_test."""
load("@rules_cc//cc:cc_test.bzl", "cc_test")
load(":tx_common.bzl", "merge_linkopts", "create_wasm_targets", "create_platform_alias")

def tx_test(name, *args, **kwargs):
    """Creates a multi-platform test target that works for native and WASM platforms.
    
    For native platforms: creates a regular cc_test
    For WASM platforms: creates a runner that executes the test in browser
    
    Usage:
    - Native: bazel test //test:name
    - WASM: bazel test //test:name --platforms=@emsdk//:platform_wasm
    
    Args:
        name: The name of the target.
        *args: Additional arguments passed to cc_test.
        **kwargs: Additional keyword arguments passed to cc_test.
    """

    # Merge user linkopts with defaults
    user_linkopts = kwargs.pop("linkopts", [])
    merged_linkopts = merge_linkopts(user_linkopts)

    # Main test target - works for both native and WASM
    cc_test(
        name = name + "-tbin",
        linkopts = merged_linkopts,
        *args,
        **kwargs,
    )

    # Create WASM targets for WASM platform execution
    create_wasm_targets(name, name + "-tbin", testonly = True)
    create_platform_alias(name, "-tbin", testonly = True)
