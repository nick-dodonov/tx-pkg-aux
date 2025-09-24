"""Starlark build definitions for tx_binary using cc_binary."""
load("@rules_cc//cc:cc_binary.bzl", "cc_binary")
load("@rules_shell//shell:sh_binary.bzl", "sh_binary")
load("@emsdk//emscripten_toolchain:wasm_rules.bzl", "wasm_cc_binary")

# Объединенная сборка с платформо-специфичными целями для запуска через bazel run
def tx_binary(name, *args, **kwargs):
    """Creates a multi-platform binary target that works for native and WASM platforms.
    
    Args:
        name: The name of the target.
        *args: Additional arguments passed to cc_binary.
        **kwargs: Additional keyword arguments passed to cc_binary.
    """

    # Default platform-specific linkopts
    default_linkopts = select({
        "@platforms//cpu:wasm32": [
            "--oformat=html",
            "--emrun",  # Поддержка emrun для headless запуска
            # Need for exit(status)
            # - https://emscripten.org/docs/api_reference/emscripten.h.html#c.emscripten_force_exit
            # - https://emscripten.org/docs/getting_started/FAQ.html#what-does-exiting-the-runtime-mean-why-don-t-atexit-s-run
            # - https://github.com/emscripten-core/emscripten/blob/main/src/settings.js
            "-sEXIT_RUNTIME=1",
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
    
    # Merge user linkopts with defaults
    user_linkopts = kwargs.pop("linkopts", [])
    merged_linkopts = default_linkopts + user_linkopts

    # Бинарник целевой сборки
    cc_binary(
        name = name + "-bin",
        linkopts = merged_linkopts,
        *args,
        **kwargs,
    )

    # WASM-специфичные цели создаются только для WASM платформы
    wasm_cc_binary(
        name = name + "-wasm",
        cc_target = ":" + name + "-bin",
        # По-умолчанию output'ом будет .wasm, но можно указать и другие
        # outputs = [
        #     "some.html",
        #     "some.js", 
        #     "some.wasm",
        # ],
        visibility = ["//visibility:public"],
        target_compatible_with = ["@platforms//cpu:wasm32"],
    )

    # WASM запуск в браузере через emrun и получение stdout/exit code
    sh_binary(
        name = name + "-runner",
        srcs = ["//tools/bazel/builder:run-wasm.sh"],
        args = ["$(execpaths :" + name + "-wasm)"],
        data = [":" + name + "-wasm"],
        visibility = ["//visibility:public"],
        target_compatible_with = ["@platforms//cpu:wasm32"],
    )

    # Alias allowing to run executable on for different target platforms
    native.alias(
        name = name,
        actual = select({
            "@platforms//cpu:wasm32": ":" + name + "-runner",
            "//conditions:default": ":" + name + "-bin",
        }),
        visibility = ["//visibility:public"],
    )
