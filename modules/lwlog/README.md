# lwlog Bazel Module

Этот модуль позволяет собирать различные версии библиотеки lwlog из git репозитория.

## Использование

### В корневом проекте

В `MODULE.bazel` просто объявите зависимость:

```starlark
bazel_dep(name = "lwlog", version = "1.4.0")

# Для локальной разработки
local_path_override(
    module_name = "lwlog",
    path = "./modules/lwlog",
)
```

# lwlog Bazel Module

Этот модуль позволяет собирать различные версии библиотеки lwlog из git репозитория.

## Как работает передача версии

### 1. Версия задается в корневом модуле:
```starlark
# MODULE.bazel (корневой)
bazel_dep(name = "lwlog", version = "1.4.0")  # Объявляем нужную версию
local_path_override(module_name = "lwlog", path = "./modules/lwlog")
```

### 2. Версия синхронизируется в локальном модуле:
```starlark
# modules/lwlog/MODULE.bazel
module(name = "lwlog", version = "1.4.0")  # Должна совпадать с корневой
```

### 3. Extension использует версию модуля:
Extension читает версию из `module(version = "...")` и собирает соответствующую версию из git.

## Переключение версий

Для смены версии lwlog:

1. **Обновите корневой MODULE.bazel:**
```starlark
bazel_dep(name = "lwlog", version = "1.3.1")  # Новая версия
```

2. **Обновите локальный MODULE.bazel:**
```starlark
module(name = "lwlog", version = "1.3.1")  # Та же версия
```

3. **Пересоберите:**
```bash
bazel clean && bazel build @lwlog//:lwlog
```

## Поддерживаемые версии

## Поддерживаемые версии

- `"master"` или `"latest"` - последняя версия из master ветки
- `"1.4.0"`, `"1.3.1"`, etc. - конкретные теги версий
- Любая версия с тегом `v<version>` в репозитории

## Структура

```
modules/lwlog/
├── MODULE.bazel      # Конфигурация модуля и версии
├── BUILD.bazel       # Переэкспорт библиотеки
└── lwlog_ext.bzl     # Extension для сборки из git
```

## Сборка

```bash
# Собрать только lwlog
bazel build @lwlog//:lwlog

# Собрать проект с lwlog
bazel build //src:my-project
```
