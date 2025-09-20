#!/bin/bash
# Финальный тест всех детекторов инструментов

set -euo pipefail

echo "🔧 ТЕСТ ДЕТЕКТОРОВ ИНСТРУМЕНТОВ"
echo "==============================="

echo ""
echo "1️⃣ Текущая система:"
echo "------------------"
echo "ОС: $(uname -s) $(uname -m)"
if command -v sw_vers >/dev/null 2>&1; then
    echo "macOS: $(sw_vers -productVersion)"
fi

echo ""
echo "2️⃣ Homebrew статус:"
echo "------------------"
if command -v brew >/dev/null 2>&1; then
    echo "✅ Homebrew: $(brew --prefix)"
    echo "   Версия: $(brew --version | head -1)"
else
    echo "❌ Homebrew не установлен"
fi

echo ""
echo "3️⃣ Детектированные инструменты:"
echo "------------------------------"
if [[ -f ".bazelrc.local" ]]; then
    echo "Из .bazelrc.local:"
    grep "action_env" .bazelrc.local | while read -r line; do
        tool=$(echo "$line" | sed 's/.*action_env=\([^=]*\)=.*/\1/')
        path=$(echo "$line" | sed 's/.*action_env=[^=]*=\(.*\)/\1/')
        if [[ -f "$path" ]]; then
            echo "   ✅ $tool: $path"
        else
            echo "   ❌ $tool: $path (не найден)"
        fi
    done
else
    echo "❌ .bazelrc.local не найден"
fi

echo ""
echo "4️⃣ Проверка версий:"
echo "------------------"

check_tool_version() {
    local tool_name="$1"
    local env_var="$2"
    
    if [[ -f ".bazelrc.local" ]] && grep -q "$env_var=" .bazelrc.local; then
        local tool_path=$(grep "$env_var=" .bazelrc.local | head -1 | sed "s/.*$env_var=\(.*\)/\1/")
        if [[ -f "$tool_path" ]]; then
            echo "🔍 $tool_name ($tool_path):"
            case "$tool_name" in
                make)
                    "$tool_path" --version 2>/dev/null | head -1 | sed 's/^/   /'
                    ;;
                cmake)
                    "$tool_path" --version 2>/dev/null | head -1 | sed 's/^/   /'
                    ;;
                ninja)
                    "$tool_path" --version 2>/dev/null | sed 's/^/   /'
                    ;;
                pkg-config)
                    "$tool_path" --version 2>/dev/null | sed 's/^/   /'
                    ;;
            esac
        else
            echo "❌ $tool_name: путь не существует ($tool_path)"
        fi
    else
        echo "⚠️  $tool_name: не настроен"
    fi
}

check_tool_version "make" "MAKE"
check_tool_version "cmake" "CMAKE"
check_tool_version "ninja" "NINJA"
check_tool_version "pkg-config" "PKG_CONFIG"

echo ""
echo "5️⃣ Альтернативные локации:"
echo "-------------------------"

echo "Make варианты:"
for path in "/usr/bin/make" "/opt/homebrew/bin/make" "/opt/homebrew/bin/gmake" "/usr/local/bin/make" "/usr/local/bin/gmake"; do
    if [[ -f "$path" ]]; then
        version=$("$path" --version 2>/dev/null | head -1)
        echo "   ✅ $path - $version"
    else
        echo "   ❌ $path"
    fi
done

echo ""
echo "CMake варианты:"
for path in "/Applications/CMake.app/Contents/bin/cmake" "/opt/homebrew/bin/cmake" "/usr/local/bin/cmake" "/usr/bin/cmake"; do
    if [[ -f "$path" ]]; then
        version=$("$path" --version 2>/dev/null | head -1)
        echo "   ✅ $path - $version"
    else
        echo "   ❌ $path"
    fi
done

echo ""
echo "6️⃣ Тест сборки:"
echo "--------------"
echo "Запуск тестовой сборки..."
if bazel build //src:pkg-log --nobuild >/dev/null 2>&1; then
    echo "✅ Конфигурация валидна - сборка инициализируется без ошибок"
else
    echo "❌ Ошибка конфигурации"
fi

echo ""
echo "7️⃣ Рекомендации:"
echo "---------------"

# Проверяем что можно улучшить
recommendations=()

# Проверка make
if grep -q "MAKE=/usr/bin/make" .bazelrc.local 2>/dev/null; then
    recommendations+=("🔄 Обновить make: brew install make (текущий устаревший)")
fi

# Проверка cmake
if ! grep -q "CMAKE=" .bazelrc.local 2>/dev/null; then
    recommendations+=("📦 Установить CMake: brew install cmake")
fi

# Проверка ninja
if ! grep -q "NINJA=" .bazelrc.local 2>/dev/null; then
    recommendations+=("⚡ Установить Ninja: brew install ninja (ускорит сборку)")
fi

if [[ ${#recommendations[@]} -eq 0 ]]; then
    echo "✅ Все инструменты оптимально настроены!"
else
    for rec in "${recommendations[@]}"; do
        echo "$rec"
    done
fi

echo ""
echo "8️⃣ Команды для улучшения:"
echo "------------------------"
echo "# Переопределить автодетект:"
echo "./tools/bazel/platform_detection.sh"
echo ""
echo "# Установить оптимальный набор:"
echo "brew install make cmake ninja pkg-config"
echo ""
echo "# Проверить результат:"
echo "./tools/bazel/test_tool_detection.sh"

echo ""
echo "🎯 Финальный статус:"
echo "==================="
if bazel info >/dev/null 2>&1; then
    echo "✅ Bazel работает корректно"
    echo "✅ Инструменты настроены"
    echo "✅ Система готова к разработке"
else
    echo "❌ Проблемы с конфигурацией Bazel"
fi