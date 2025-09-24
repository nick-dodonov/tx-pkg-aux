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

    # Бинарник целевой сборки
    cc_binary(
        name = name + "-bin",
        *args,
        **kwargs,
    )

    # WASM правило для получения отдельных файлов
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
    )

    # WASM запуск в браузере через emrun и получение stdout/exit code
    sh_binary(
        name = name + "-runner",
        srcs = ["run_wasm_demo.sh"],
        args = ["$(execpaths :" + name + "-wasm)"],
        data = [":" + name + "-wasm"],
        visibility = ["//visibility:public"],
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
