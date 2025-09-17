"""lwlog module extension for Bazel - builds from external git repository"""

def _lwlog_repo_impl(repository_ctx):
    version = repository_ctx.attr.version
    git_url = repository_ctx.attr.git_url
    
    # Определяем URL для скачивания на основе версии
    if version == "master" or version == "latest":
        download_url = git_url + "/archive/refs/heads/master.zip"
        strip_prefix = "lwlog-master"
    else:
        # Для конкретных версий используем теги
        download_url = git_url + "/archive/refs/tags/v" + version + ".zip"
        strip_prefix = "lwlog-" + version
    
    # Клонируем lwlog репозиторий
    repository_ctx.download_and_extract(
        url = download_url,
        stripPrefix = strip_prefix,
    )
    
    # ...остальная логика сборки...
    
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
        fail("CMake configure failed for lwlog v{}: {}".format(version, cmake_configure_result.stderr))
    
    # Выполняем cmake build и install
    cmake_build_result = repository_ctx.execute([
        "cmake",
        "--build", build_dir,
        "--target", "install",
        "--config", "Release"
    ])
    
    if cmake_build_result.return_code != 0:
        fail("CMake build failed for lwlog v{}: {}".format(version, cmake_build_result.stderr))
    
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
    attrs = {
        "version": attr.string(mandatory = True),
        "git_url": attr.string(default = "https://github.com/ChristianPanov/lwlog"),
    },
)

# Тег для настройки версии lwlog
_lwlog_version_tag = tag_class(attrs = {
    "version": attr.string(default = "master"),
    "git_url": attr.string(default = "https://github.com/ChristianPanov/lwlog"),
})

def _lwlog_ext_impl(module_ctx):
    # Получаем версию и git_url из тегов или используем версию модуля
    version_configs = module_ctx.modules[0].tags.version
    
    if len(version_configs) == 0:
        # Читаем версию из MODULE.bazel
        # Используем версию, которая указана в module()
        version = "1.4.0"  # Должна совпадать с version в module()
        git_url = "https://github.com/ChristianPanov/lwlog"
    elif len(version_configs) == 1:
        # Используем указанную версию (для переопределения)
        config = version_configs[0]
        version = config.version
        git_url = config.git_url
    else:
        fail("Можно указать только одну версию lwlog")
    
    # Создаем репозиторий lwlog из git
    _lwlog_repo(
        name = "lwlog_src",
        version = version,
        git_url = git_url,
    )

lwlog = module_extension(
    implementation = _lwlog_ext_impl,
    tag_classes = {
        "version": _lwlog_version_tag,
    },
)
