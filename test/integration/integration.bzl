"""Macro for declaring integration tests with an auto-managed external server."""

load("@rules_shell//shell:sh_test.bzl", "sh_test")
load("@tx-kit-ext//rules:exec_binary.bzl", "exec_binary")

def _integration_test_impl(name, client, server, port, health_path, bridge, tags, visibility):
    # Wrap the server binary to run on the exec (host) platform.
    # Required so that cross-platform builds (e.g. --config=wasm) don't try
    # to build the server for the target platform.
    exec_binary(
        name = name + ".server",
        binary = server,
        visibility = ["//visibility:private"],
    )

    # Proxy aliases let sh_test reference cross-package targets using compact
    # ":name" syntax, avoiding canonical label form issues in $(location ...).
    native.alias(
        name = name + ".bridge",
        actual = bridge,
        visibility = ["//visibility:private"],
    )
    native.alias(
        name = name + ".client",
        actual = client,
        visibility = ["//visibility:private"],
    )

    sh_test(
        name = name + ".cmd",
        srcs = ["@tx-kit-ext//runner:sh_wrapper"],
        args = [
            "$(location //:runner)",
            "$(location :" + name + ".bridge)",
            "$(location :" + name + ".server)",
            "$(location :" + name + ".client)",
            "--port",
            str(port),
            "--health-path",
            health_path,
        ],
        data = [
            "//:runner",
            ":" + name + ".bridge",
            ":" + name + ".server",
            ":" + name + ".client",
        ],
        tags = [
            "exclusive",  # [bazel] prevent parallel execution with other tests and in --runs_per_test mode (to avoid port conflicts)
            "requires-network",  # [bazel] test uses network (local server on specific port)
            "integration",  # [local] integration test (longer running, external dependencies, etc.)
        ] + tags,
        visibility = visibility,
    )

    native.test_suite(
        name = name,
        tests = [":" + name + ".cmd"],
        visibility = visibility,
    )

integration_test = macro(
    attrs = {
        "client": attr.label(mandatory = True),
        "server": attr.label(mandatory = True),
        "port": attr.int(default = 8080, configurable = False),
        "health_path": attr.string(default = "/health", configurable = False),
        "bridge": attr.label(
            default = Label("//test/integration:bridge.cmd"),
            configurable = False,
        ),
        "tags": attr.string_list(configurable = False),
    },
    implementation = _integration_test_impl,
)
