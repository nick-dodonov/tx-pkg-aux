# Примеры переключения версий lwlog

## Конфигурация 1: Стабильная версия 1.4.0

### Корневой MODULE.bazel:
```starlark
bazel_dep(name = "lwlog", version = "1.4.0")
local_path_override(module_name = "lwlog", path = "./modules/lwlog")
```

### modules/lwlog/MODULE.bazel:
```starlark
module(name = "lwlog", version = "1.4.0")
```

## Конфигурация 2: Предыдущая версия 1.3.1

### Корневой MODULE.bazel:
```starlark
bazel_dep(name = "lwlog", version = "1.3.1")
local_path_override(module_name = "lwlog", path = "./modules/lwlog")
```

### modules/lwlog/MODULE.bazel:
```starlark
module(name = "lwlog", version = "1.3.1")
```

## Конфигурация 3: Экспериментальная (с переопределением)

### modules/lwlog/MODULE.bazel:
```starlark
module(name = "lwlog", version = "1.4.0")

lwlog_ext = use_extension(":lwlog_ext.bzl", "lwlog")
lwlog_ext.version(
    version = "master",  # Переопределяем версию
    git_url = "https://github.com/myorg/lwlog-fork"
)
use_repo(lwlog_ext, "lwlog_src")
```

## Быстрое переключение

1. **Изменить версии** в двух файлах MODULE.bazel
2. **Очистить кэш** и пересобрать:
```bash
bazel clean
bazel build @lwlog//:lwlog
```

## Автоматизация

Можно создать скрипт для переключения версий:
```bash
#!/bin/bash
VERSION=$1
# Обновляем корневой MODULE.bazel
sed -i '' "s/version = \".*\"/version = \"$VERSION\"/" MODULE.bazel
# Обновляем локальный MODULE.bazel  
sed -i '' "s/version = \".*\"/version = \"$VERSION\"/" modules/lwlog/MODULE.bazel
# Пересобираем
bazel clean && bazel build @lwlog//:lwlog
```
