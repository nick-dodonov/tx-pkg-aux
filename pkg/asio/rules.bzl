load("@rules_cc//cc/common:cc_info.bzl", "CcInfo")

# rules/boost_aware_transition.bzl
def _boost_ssl_transition_impl(settings, attr):
    """Sets boost.asio SSL flag based on target platform."""
    attr = attr  # Suppress unused variable warning

    # Default to implementation for SSL support in boost.asio
    result_ssl = "boringssl"

    # User can explicitly disable SSL support via the disable_ssl flag
    disable_ssl = settings.get("//pkg/asio:disable_ssl", False)
    if disable_ssl:
        result_ssl = "no_ssl"
    else:
        # For WASM, we need to disable SSL support in boost.asio since it doesn't work in that environment anyway
        platforms = settings.get("//command_line_option:platforms", [])
        for platform in platforms:
            platform_str = str(platform)
            if "wasm" in platform_str:
                result_ssl = "no_ssl"
                break

    # print("BOOST.ASIO SETTINGS:", settings, " -> ", result_ssl)
    return {
        "@boost.asio//:ssl": result_ssl
    }

_boost_ssl_transition = transition(
    implementation = _boost_ssl_transition_impl,
    inputs = [
        "//command_line_option:platforms",
        "//pkg/asio:disable_ssl",
        "@boost.asio//:ssl",
    ],
    outputs = [
        "@boost.asio//:ssl"
    ],
)

def _boost_asio_wrapped_impl(ctx):
    actual = ctx.attr.actual
    # print("BOOST.ASIO WRAPPED", actual)
    return [
        actual[DefaultInfo],
        actual[CcInfo],
        actual[InstrumentedFilesInfo],
        actual[OutputGroupInfo],
    ]

boost_asio_wrapped = rule(
    implementation = _boost_asio_wrapped_impl,
    attrs = {
        "actual": attr.label(
            mandatory = True,
        ),
    },
    cfg = _boost_ssl_transition,
)
