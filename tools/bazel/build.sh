#!/bin/bash
# Удобный wrapper для Bazel с предустановленными конфигурациями

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Функция помощи
show_help() {
    cat << EOF
🚀 Bazel Build Wrapper

Использование: $0 [PLATFORM] [TARGET] [MODE] [ARGS...]

ПЛАТФОРМЫ:
  host     - Текущая платформа (по умолчанию)
  wasm     - WebAssembly/Emscripten
  android  - Android
  ios      - iOS

РЕЖИМЫ:
  dev      - Разработка (по умолчанию)
  release  - Релизная сборка
  debug    - Отладочная сборка
  hermetic - Воспроизводимая сборка

ПРИМЕРЫ:
  $0                           # host dev //...
  $0 host release //...        # Релиз для текущей платформы
  $0 wasm release //src:app    # WebAssembly релиз
  $0 host --config=profile     # С профилированием

СПЕЦИАЛЬНЫЕ КОМАНДЫ:
  $0 clean          # Очистка
  $0 platforms      # Показать доступные платформы
  $0 configs        # Показать доступные конфигурации
  $0 init           # Инициализация (определение платформы)
  $0 test           # Запуск тестов с оптимальными настройками

ГОТОВЫЕ КОМБИНАЦИИ:
  $0 mac-dev        # macOS development
  $0 mac-release    # macOS release
  $0 wasm-dev       # WebAssembly development
  $0 linux-ci       # Linux CI build
EOF
}

# Инициализация (автоопределение платформы)
init_platform() {
    echo "🔧 Инициализация Bazel конфигурации..."
    "$SCRIPT_DIR/platform_detection.sh"
}

# Обработка специальных команд
case "${1:-}" in
    help|--help|-h)
        show_help
        exit 0
        ;;
    clean)
        cd "$PROJECT_ROOT"
        bazel clean --expunge
        exit 0
        ;;
    platforms)
        echo "📱 Доступные платформы:"
        echo "  host     - Текущая операционная система"
        echo "  wasm     - WebAssembly/Emscripten"
        echo "  android  - Android (требует Android SDK/NDK)"
        echo "  ios      - iOS (только на macOS с Xcode)"
        exit 0
        ;;
    configs)
        echo "⚙️  Доступные конфигурации:"
        cd "$PROJECT_ROOT"
        echo "Платформы:"
        grep -E "^build:[a-zA-Z]" tools/bazel/configs/platforms.bazelrc 2>/dev/null | \
            sed 's/.*build:\([a-zA-Z0-9_-]*\).*/  \1/' | sort -u || echo "  (файл не найден)"
        echo "Цели:"
        grep -E "^build:[a-zA-Z]" tools/bazel/configs/targets.bazelrc 2>/dev/null | \
            sed 's/.*build:\([a-zA-Z0-9_-]*\).*/  \1/' | sort -u || echo "  (файл не найден)"
        echo "Режимы:"
        grep -E "^build:[a-zA-Z]" tools/bazel/configs/modes.bazelrc 2>/dev/null | \
            sed 's/.*build:\([a-zA-Z0-9_-]*\).*/  \1/' | sort -u || echo "  (файл не найден)"
        echo "Готовые комбинации:"
        grep -E "^build:[a-zA-Z].*--config" .bazelrc 2>/dev/null | \
            sed 's/.*build:\([a-zA-Z0-9_-]*\).*/  \1/' | sort -u || echo "  (файл не найден)"
        exit 0
        ;;
    init)
        init_platform
        exit 0
        ;;
    test)
        shift
        cd "$PROJECT_ROOT"
        # Автоинициализация если нет .bazelrc.local
        [[ ! -f .bazelrc.local ]] && init_platform
        echo "🧪 Запуск тестов..."
        exec bazel test --config=unit "$@"
        ;;
esac

# Проверка инициализации
cd "$PROJECT_ROOT"
if [[ ! -f .bazelrc.local ]]; then
    echo "⚠️  Файл .bazelrc.local не найден. Запускаем инициализацию..."
    init_platform
    echo ""
fi

# Проверка готовых комбинаций
case "${1:-}" in
    mac-dev|mac-release|mac-debug|win-dev|win-release|linux-dev|linux-ci|wasm-dev|wasm-release)
        READY_CONFIG="$1"
        shift
        echo "🚀 Сборка с готовой конфигурацией: $READY_CONFIG"
        if [[ $# -eq 0 ]]; then
            set -- "//..."
        fi
        exec bazel build --config="$READY_CONFIG" "$@"
        ;;
esac

# Парсинг аргументов для кастомной конфигурации
PLATFORM="${1:-host}"
TARGET_ARG=""
MODE="${2:-dev}"

# Проверяем является ли первый аргумент платформой
if [[ "$PLATFORM" =~ ^(host|wasm|android|ios)$ ]]; then
    TARGET_ARG="$1"
    shift
    if [[ $# -gt 0 ]] && [[ "$1" =~ ^(dev|release|debug|hermetic)$ ]]; then
        MODE="$1"
        shift
    fi
else
    # Если не платформа, то возможно это цель сборки
    TARGET_ARG="host"
    MODE="dev"
fi

# Определение targets по умолчанию
if [[ $# -eq 0 ]]; then
    set -- "//..."
fi

# Формирование команды
CONFIGS=""

# Добавляем платформенную конфигурацию
if [[ -n "$TARGET_ARG" && "$TARGET_ARG" != "host" ]]; then
    CONFIGS="--config=$TARGET_ARG"
fi

# Добавляем режим только если он не задан в .bazelrc.local как default
DEFAULT_CONFIGS_IN_LOCAL=""
if [[ -f .bazelrc.local ]]; then
    DEFAULT_CONFIGS_IN_LOCAL=$(grep "build --config=" .bazelrc.local | sed 's/.*--config=\([a-zA-Z0-9_-]*\).*/\1/' | tr '\n' ' ')
fi

# Добавляем режим только если он не задан в .bazelrc.local
if [[ "$DEFAULT_CONFIGS_IN_LOCAL" != *"$MODE"* ]]; then
    CONFIGS="$CONFIGS --config=$MODE"
fi

echo "🚀 Сборка Bazel:"
echo "   Платформа: $TARGET_ARG"
echo "   Режим: $MODE"
if [[ -n "$DEFAULT_CONFIGS_IN_LOCAL" ]]; then
    echo "   Конфигурации по умолчанию: $DEFAULT_CONFIGS_IN_LOCAL"
fi
echo "   Цели: $*"
echo "   Дополнительные конфигурации: ${CONFIGS:-'(нет)'}"
echo ""

# Выполнение
if [[ -n "$CONFIGS" ]]; then
    exec bazel build $CONFIGS "$@"
else
    exec bazel build "$@"
fi