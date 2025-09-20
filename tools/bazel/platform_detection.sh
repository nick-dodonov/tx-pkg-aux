#!/bin/bash
# Автоматическое определение платформы и генерация .bazelrc.local

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
LOCAL_RC="$PROJECT_ROOT/.bazelrc.local"

echo "🔍 Автоопределение платформы для Bazel..."

# Создаем .bazelrc.local
echo "# Автогенерированный .bazelrc.local" > "$LOCAL_RC"
echo "# Не добавлять в git!" >> "$LOCAL_RC"
echo "# Дата создания: $(date)" >> "$LOCAL_RC"
echo "" >> "$LOCAL_RC"

# Определение ОС
case "$(uname -s)" in
    Darwin*)
        echo "🍎 Обнаружена macOS"
        echo "# === MACOS DETECTED ===" >> "$LOCAL_RC"
        echo "build --config=macos" >> "$LOCAL_RC"
        
        # Проверка архитектуры
        if [[ "$(uname -m)" == "arm64" ]]; then
            echo "   Архитектура: Apple Silicon (ARM64)"
            echo "build --host_cpu=darwin_arm64" >> "$LOCAL_RC"
        else
            echo "   Архитектура: Intel (x86_64)"
            echo "build --host_cpu=darwin_x86_64" >> "$LOCAL_RC"
        fi
        
        # Поиск Homebrew инструментов
        if command -v brew >/dev/null 2>&1; then
            HOMEBREW_PREFIX=$(brew --prefix)
            echo "   Homebrew обнаружен: $HOMEBREW_PREFIX"
            
            # Проверяем наличие инструментов
            for tool in pkg-config ninja cmake; do
                if [[ -f "$HOMEBREW_PREFIX/bin/$tool" ]]; then
                    tool_upper=$(echo "$tool" | tr '[:lower:]' '[:upper:]' | tr '-' '_')
                    echo "build --action_env=${tool_upper}=$HOMEBREW_PREFIX/bin/$tool" >> "$LOCAL_RC"
                    echo "   ✅ $tool: $HOMEBREW_PREFIX/bin/$tool"
                else
                    echo "   ⚠️  $tool: не найден в Homebrew"
                fi
            done
        else
            echo "   ⚠️  Homebrew не найден"
        fi
        
        # Проверка Xcode
        if command -v xcodebuild >/dev/null 2>&1; then
            echo "   ✅ Xcode обнаружен"
        else
            echo "   ⚠️  Xcode не найден (нужен для iOS сборки)"
        fi
        ;;
        
    Linux*)
        echo "🐧 Обнаружена Linux"
        echo "# === LINUX DETECTED ===" >> "$LOCAL_RC"
        echo "build --config=linux" >> "$LOCAL_RC"
        
        # Проверка архитектуры
        echo "   Архитектура: $(uname -m)"
        
        # Проверка инструментов
        for tool in make cmake pkg-config ninja; do
            if command -v "$tool" >/dev/null 2>&1; then
                tool_path=$(which "$tool")
                tool_upper=$(echo "$tool" | tr '[:lower:]' '[:upper:]' | tr '-' '_')
                echo "build --action_env=${tool_upper}=$tool_path" >> "$LOCAL_RC"
                echo "   ✅ $tool: $tool_path"
            else
                echo "   ⚠️  $tool: не найден"
            fi
        done
        ;;
        
    MINGW*|MSYS*|CYGWIN*)
        echo "🪟 Обнаружена Windows"
        echo "# === WINDOWS DETECTED ===" >> "$LOCAL_RC"
        echo "build --config=windows" >> "$LOCAL_RC"
        
        # Поиск инструментов Windows
        if [[ -f "/c/Program Files/CMake/bin/cmake.exe" ]]; then
            echo "build --action_env=CMAKE=\"C:/Program Files/CMake/bin/cmake.exe\"" >> "$LOCAL_RC"
            echo "   ✅ CMake: C:/Program Files/CMake/bin/cmake.exe"
        fi
        ;;
        
    *)
        echo "❓ Неизвестная ОС, используем Linux настройки"
        echo "# === UNKNOWN OS (using Linux defaults) ===" >> "$LOCAL_RC"
        echo "build --config=linux" >> "$LOCAL_RC"
        ;;
esac

# Определение CI/CD среды
if [[ -n "${CI:-}" ]] || [[ -n "${GITHUB_ACTIONS:-}" ]] || [[ -n "${JENKINS_URL:-}" ]] || [[ -n "${BUILDKITE:-}" ]]; then
    echo "🤖 CI/CD среда обнаружена"
    echo "build --config=ci" >> "$LOCAL_RC"
fi

# Добавление режима по умолчанию
echo "" >> "$LOCAL_RC"
echo "# === DEFAULT MODE ===" >> "$LOCAL_RC"
echo "build --config=dev" >> "$LOCAL_RC"

# Добавление пользовательских настроек по умолчанию
echo "" >> "$LOCAL_RC"
echo "# === ПЕРСОНАЛЬНЫЕ НАСТРОЙКИ ===" >> "$LOCAL_RC"
echo "# Раскомментируйте и настройте по необходимости:" >> "$LOCAL_RC"
echo "# build --jobs=16" >> "$LOCAL_RC"
echo "# build --local_cpu_resources=16" >> "$LOCAL_RC"
echo "# build --local_ram_resources=32768" >> "$LOCAL_RC"

echo ""
echo "✅ Создан файл: $LOCAL_RC"
echo ""
echo "📋 Содержимое:"
echo "$(cat "$LOCAL_RC")"
echo ""
echo "💡 Теперь можно использовать:"
echo "   bazel build //...                    # Автоконфигурация"
echo "   bazel build --config=mac-dev //...   # Явная конфигурация"
echo "   bazel build --config=profile //...   # С профилированием"