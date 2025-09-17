"""lwlog module extension for Bazel - builds from external git repository"""

def _lwlog_repo_impl(repository_ctx):
    # Клонируем lwlog репозиторий
    repository_ctx.download_and_extract(
        url = "https://github.com/ChristianPanov/lwlog/archive/refs/heads/master.zip",
        stripPrefix = "lwlog-master",
    )
    
    # Создаем директории для сборки
    build_dir = "build"
    install_dir = "install"
    
    # Выполняем cmake configure
    cmake_configure_result = repository_ctx.execute([
        "cmake", 
        "-B", build_dir,
        "-S", ".",
        "-DCMAKE_INSTALL_PREFIX=" + install_dir,
        "-DCMAKE_BUILD_TYPE=Release"
    ])
    
    if cmake_configure_result.return_code != 0:
        fail("CMake configure failed: " + cmake_configure_result.stderr)
    
    # Выполняем cmake build и install
    cmake_build_result = repository_ctx.execute([
        "cmake",
        "--build", build_dir,
        "--target", "install",
        "--config", "Release"
    ])
    
    if cmake_build_result.return_code != 0:
        fail("CMake build failed: " + cmake_build_result.stderr)
    
    # Создаем BUILD файл для использования собранной библиотеки
    repository_ctx.file("BUILD.bazel", """
cc_library(
    name = "lwlog",
    hdrs = glob([
        "install/include/**/*.h",
    ], allow_empty = True) + glob([
        "include/**/*.h",
    ], allow_empty = True),
    includes = [
        "install/include",
        "install/include/lwlog",  # для внутренних includes
        "include",  # fallback
    ],
    linkopts = ["-pthread"],
    visibility = ["//visibility:public"],
)
""")

_lwlog_repo = repository_rule(
    implementation = _lwlog_repo_impl,
    attrs = {},
)

def _lwlog_ext_impl(module_ctx):
    # Создаем репозиторий lwlog из git
    _lwlog_repo(name = "lwlog_src")

lwlog = module_extension(implementation = _lwlog_ext_impl)
