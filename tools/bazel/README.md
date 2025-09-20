# Модульные конфигурации Bazel

## 📁 Структура

```
tools/bazel/
├── configs/
│   ├── common.bazelrc      # Общие настройки
│   ├── platforms.bazelrc   # Платформы (host OS)
│   ├── targets.bazelrc     # Целевые платформы
│   └── modes.bazelrc       # Режимы сборки
├── build.sh               # Удобный wrapper
├── platform_detection.sh  # Автоопределение платформы
└── README.md              # Эта документация
```

## 🎯 Использование

### Автоматическая инициализация
```bash
# Первый запуск - автоопределение платформы
./tools/bazel/build.sh init

# Или просто
bazel build //...  # Автоматически создаст .bazelrc.local
```

### Простое использование
```bash
# Автоматическое определение платформы + dev режим
bazel build //...

# Конкретная платформа и режим  
bazel build --config=macos --config=dev //...

# Готовые комбинации
bazel build --config=mac-dev //...
bazel build --config=wasm-release //...
```

### Через удобный wrapper
```bash
# Простая сборка (авто-платформа + dev)
./tools/bazel/build.sh

# WebAssembly в релизном режиме
./tools/bazel/build.sh wasm release //src:app

# Готовые комбинации
./tools/bazel/build.sh mac-dev
./tools/bazel/build.sh wasm-release

# С профилированием
./tools/bazel/build.sh host dev --config=profile //...

# Тестирование
./tools/bazel/build.sh test
```

## ⚙️ Конфигурации

### Платформы (host OS)
- `macos` - macOS с Homebrew инструментами
- `linux` - Linux с системными пакетами
- `windows` - Windows с MSYS2/MinGW
- `ci` - CI/CD среда

### Целевые платформы
- `host` - Текущая платформа (по умолчанию)
- `wasm` - WebAssembly/Emscripten
- `android` - Android (требует SDK/NDK)
- `ios` - iOS (только на macOS)

### Режимы сборки
- `dev` - Разработка (быстро, с отладкой)
- `release` - Релиз (оптимизировано)
- `debug` - Отладка (максимум информации)
- `hermetic` - Воспроизводимо (медленно)

### Тестирование
- `unit` - Юнит-тесты (быстро)
- `integration` - Интеграционные тесты
- `performance` - Тесты производительности

## 🎯 Готовые комбинации

```bash
# Development
bazel build --config=mac-dev //...
bazel build --config=linux-dev //...
bazel build --config=win-dev //...

# Release
bazel build --config=mac-release //...
bazel build --config=linux-ci //...

# WebAssembly
bazel build --config=wasm-dev //...
bazel build --config=wasm-release //...

# Testing
bazel test --config=mac-unit //...
bazel test --config=mac-integration //...
```

## 🔧 Персонализация

### Локальные настройки
Создайте `.bazelrc.local` для персональных настроек:
```bazelrc
# Ваши локальные настройки
build --jobs=16
build --local_cpu_resources=16
build --local_ram_resources=32768

# Персональные пути к инструментам
build --action_env=CMAKE=/custom/path/to/cmake
```

### Добавление новой платформы
1. Добавьте в `tools/bazel/configs/platforms.bazelrc`:
```bazelrc
build:newplatform --action_env=PATH
build:newplatform --action_env=CMAKE=/path/to/cmake
```

2. Обновите `platform_detection.sh` для автоопределения

3. Добавьте alias в `.bazelrc`:
```bazelrc
build:newplatform-dev --config=newplatform --config=dev
```

## 🛠️ Утилиты

### Информация о конфигурациях
```bash
./tools/bazel/build.sh configs     # Список всех конфигураций
./tools/bazel/build.sh platforms   # Список платформ
./tools/bazel/build.sh help        # Справка
```

### Очистка
```bash
./tools/bazel/build.sh clean       # Полная очистка
```

### Профилирование
```bash
# С профилированием
bazel build --config=profile //...

# Или через wrapper
./tools/bazel/build.sh host dev --config=profile //...
```

## 📋 Примеры использования

### Разработка под разные платформы
```bash
# macOS разработка
./tools/bazel/build.sh mac-dev

# Linux CI
./tools/bazel/build.sh linux-ci

# WebAssembly для веба
./tools/bazel/build.sh wasm-release //src:web_app
```

### Тестирование
```bash
# Быстрые юнит-тесты
./tools/bazel/build.sh test //test/unit:all

# Интеграционные тесты
bazel test --config=mac-integration //test/integration:all

# Все тесты
bazel test --config=mac-unit //...
```

### Отладка
```bash
# Debug сборка с полной информацией
bazel build --config=mac-debug //src:my_app

# Hermetic сборка для воспроизводимости
bazel build --config=hermetic //...
```

## 🚀 Преимущества модульного подхода

✅ **Разделение ответственности** - каждый файл отвечает за свою область
✅ **Легкость сопровождения** - изменения локализованы
✅ **Переиспользование** - конфигурации можно комбинировать
✅ **Автоматизация** - автоопределение платформы
✅ **Масштабируемость** - легко добавлять новые платформы/режимы
✅ **Удобство** - готовые комбинации и wrapper скрипты