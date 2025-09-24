"""Starlark build definitions for tx_binary using cc_binary."""
load("@rules_cc//cc:cc_binary.bzl", "cc_binary")
load(":tx_common.bzl", "merge_linkopts", "create_wasm_targets", "create_platform_alias")

def tx_binary(name, *args, **kwargs):
    """Creates a multi-platform binary target that works for native and WASM platforms.
    
    Args:
        name: The name of the target.
        *args: Additional arguments passed to cc_binary.
        **kwargs: Additional keyword arguments passed to cc_binary.
    """

    # Merge user linkopts with defaults
    user_linkopts = kwargs.pop("linkopts", [])
    merged_linkopts = merge_linkopts(user_linkopts)

    # Бинарник целевой сборки
    cc_binary(
        name = name + "-bin",
        linkopts = merged_linkopts,
        *args,
        **kwargs,
    )

    # Create WASM targets and platform alias
    create_wasm_targets(name, name + "-bin")
    create_platform_alias(name, "-bin")
