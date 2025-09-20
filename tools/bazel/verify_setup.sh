#!/bin/bash
# Проверка работоспособности модульной конфигурации Bazel

set -euo pipefail

# Определяем путь к проекту относительно местоположения скрипта
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$PROJECT_ROOT"

echo "🧪 Проверка модульной конфигурации Bazel"
echo "========================================"

# Проверка структуры файлов
echo ""
echo "📁 ПРОВЕРКА СТРУКТУРЫ ФАЙЛОВ"
echo "----------------------------"

REQUIRED_FILES=(
    ".bazelrc"
    "tools/bazel/configs/common.bazelrc"
    "tools/bazel/configs/platforms.bazelrc"
    "tools/bazel/configs/targets.bazelrc"
    "tools/bazel/configs/modes.bazelrc"
    "tools/bazel/platform_detection.sh"
    "tools/bazel/build.sh"
    "tools/bazel/README.md"
)

ALL_FILES_OK=true
for file in "${REQUIRED_FILES[@]}"; do
    if [[ -f "$file" ]]; then
        echo "✅ $file"
    else
        echo "❌ $file - не найден"
        ALL_FILES_OK=false
    fi
done

if [[ "$ALL_FILES_OK" == "false" ]]; then
    echo ""
    echo "❌ Некоторые файлы отсутствуют!"
    exit 1
fi

# Проверка импортов в главном .bazelrc
echo ""
echo "📋 ПРОВЕРКА ИМПОРТОВ"
echo "--------------------"

IMPORTS_OK=true
for config_file in tools/bazel/configs/*.bazelrc; do
    config_name=$(basename "$config_file")
    if grep -q "try-import.*$config_name" .bazelrc; then
        echo "✅ $config_name импортирован"
    else
        echo "❌ $config_name не импортирован в .bazelrc"
        IMPORTS_OK=false
    fi
done

# Проверка .bazelrc.local
echo ""
echo "🔧 ПРОВЕРКА ЛОКАЛЬНОЙ КОНФИГУРАЦИИ"
echo "----------------------------------"

if [[ -f ".bazelrc.local" ]]; then
    echo "✅ .bazelrc.local существует"
    echo "📋 Содержимое:"
    head -10 .bazelrc.local | sed 's/^/   /'
    if [[ $(wc -l < .bazelrc.local) -gt 10 ]]; then
        echo "   ... (показаны первые 10 строк)"
    fi
else
    echo "⚠️  .bazelrc.local не найден - будет создан автоматически"
fi

# Проверка доступных конфигураций
echo ""
echo "⚙️  ПРОВЕРКА КОНФИГУРАЦИЙ"
echo "------------------------"

echo "Найденные конфигурации:"
echo ""

echo "Платформы:"
grep -E "^build:[a-zA-Z]" tools/bazel/configs/platforms.bazelrc 2>/dev/null | \
    sed 's/.*build:\([a-zA-Z0-9_-]*\).*/  \1/' | sort -u

echo ""
echo "Целевые платформы:"
grep -E "^build:[a-zA-Z]" tools/bazel/configs/targets.bazelrc 2>/dev/null | \
    sed 's/.*build:\([a-zA-Z0-9_-]*\).*/  \1/' | sort -u

echo ""
echo "Режимы:"
grep -E "^build:[a-zA-Z]" tools/bazel/configs/modes.bazelrc 2>/dev/null | \
    sed 's/.*build:\([a-zA-Z0-9_-]*\).*/  \1/' | sort -u

echo ""
echo "Готовые комбинации:"
grep -E "^build:[a-zA-Z].*--config" .bazelrc 2>/dev/null | \
    sed 's/.*build:\([a-zA-Z0-9_-]*\).*/  \1/' | sort -u

# Проверка исполняемых файлов
echo ""
echo "🚀 ПРОВЕРКА СКРИПТОВ"
echo "--------------------"

SCRIPTS=(
    "tools/bazel/platform_detection.sh"
    "tools/bazel/build.sh"
)

for script in "${SCRIPTS[@]}"; do
    if [[ -x "$script" ]]; then
        echo "✅ $script - исполняемый"
    else
        echo "❌ $script - не исполняемый"
        chmod +x "$script"
        echo "   🔧 Права доступа исправлены"
    fi
done

# Тестирование wrapper'а
echo ""
echo "🧪 ТЕСТИРОВАНИЕ WRAPPER'А"
echo "-------------------------"

echo "Тест 1: Справка"
if ./tools/bazel/build.sh help >/dev/null 2>&1; then
    echo "✅ Справка работает"
else
    echo "❌ Справка не работает"
fi

echo "Тест 2: Список конфигураций"
if ./tools/bazel/build.sh configs >/dev/null 2>&1; then
    echo "✅ Список конфигураций работает"
else
    echo "❌ Список конфигураций не работает"
fi

echo "Тест 3: Список платформ"
if ./tools/bazel/build.sh platforms >/dev/null 2>&1; then
    echo "✅ Список платформ работает"
else
    echo "❌ Список платформ не работает"
fi

# Финальная проверка
echo ""
echo "🎯 ИТОГОВАЯ ПРОВЕРКА"
echo "===================="

if [[ "$ALL_FILES_OK" == "true" ]] && [[ "$IMPORTS_OK" == "true" ]]; then
    echo "✅ Модульная конфигурация Bazel настроена корректно!"
    echo ""
    echo "📝 СЛЕДУЮЩИЕ ШАГИ:"
    echo "  1. Инициализация платформы:"
    echo "     ./tools/bazel/build.sh init"
    echo ""
    echo "  2. Простая сборка:"
    echo "     bazel build //..."
    echo "     # или"
    echo "     ./tools/bazel/build.sh"
    echo ""
    echo "  3. Конкретная конфигурация:"
    echo "     bazel build --config=mac-dev //..."
    echo "     # или"
    echo "     ./tools/bazel/build.sh mac-dev"
    echo ""
    echo "  4. С профилированием:"
    echo "     bazel build --config=profile //..."
    echo ""
    echo "📚 Документация: tools/bazel/README.md"
    
else
    echo "❌ Обнаружены проблемы в конфигурации!"
    exit 1
fi

echo ""
echo "🔍 СТРУКТУРА ПРОЕКТА:"
echo "===================="
tree tools/bazel/ 2>/dev/null || find tools/bazel/ -type f | sort