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
        
        # Поиск инструментов с приоритетами
        detect_tool() {
            local tool_name="$1"
            local env_var="$2"
            local homebrew_names=("${@:3}")  # Массив имен в Homebrew
            
            local found=false
            
            # 1. Сначала ищем в Homebrew
            if command -v brew >/dev/null 2>&1; then
                local HOMEBREW_PREFIX=$(brew --prefix)
                for name in "${homebrew_names[@]}"; do
                    if [[ -f "$HOMEBREW_PREFIX/bin/$name" ]]; then
                        echo "build --action_env=${env_var}=$HOMEBREW_PREFIX/bin/$name" >> "$LOCAL_RC"
                        echo "   ✅ $tool_name: $HOMEBREW_PREFIX/bin/$name (Homebrew)"
                        found=true
                        break
                    fi
                done
            fi
            
            # 2. Если не найден в Homebrew, ищем в стандартных локациях
            if [[ "$found" == "false" ]]; then
                for path in "/usr/local/bin" "/usr/bin"; do
                    for name in "${homebrew_names[@]}"; do
                        if [[ -f "$path/$name" ]]; then
                            if [[ "$path" == "/usr/bin" && "$name" == "make" ]]; then
                                # Для системного make показываем версию
                                local version=$($path/$name --version 2>/dev/null | head -1)
                                echo "build --action_env=${env_var}=$path/$name" >> "$LOCAL_RC"
                                echo "   ✅ $tool_name: $path/$name (системный - $version)"
                            else
                                echo "build --action_env=${env_var}=$path/$name" >> "$LOCAL_RC"
                                echo "   ✅ $tool_name: $path/$name"
                            fi
                            found=true
                            break 2
                        fi
                    done
                done
            fi
            
            # 3. Если не найден, ищем в PATH
            if [[ "$found" == "false" ]]; then
                for name in "${homebrew_names[@]}"; do
                    if command -v "$name" >/dev/null 2>&1; then
                        local tool_path=$(which "$name")
                        echo "build --action_env=${env_var}=$tool_path" >> "$LOCAL_RC"
                        echo "   ✅ $tool_name: $tool_path (в PATH)"
                        found=true
                        break
                    fi
                done
            fi
            
            # 4. Если ничего не найдено
            if [[ "$found" == "false" ]]; then
                echo "   ⚠️  $tool_name: не найден"
                case "$tool_name" in
                    make)
                        echo "      Системный: xcode-select --install"
                        echo "      GNU Make: brew install make"
                        ;;
                    cmake)
                        echo "      Homebrew: brew install cmake"
                        echo "      Скачать: https://cmake.org/download/"
                        ;;
                    pkg-config)
                        echo "      Установка: brew install pkg-config"
                        ;;
                    ninja)
                        echo "      Установка: brew install ninja"
                        ;;
                esac
            fi
        }
        
        # Определяем все инструменты
        detect_tool "make" "MAKE" "gmake" "make"
        detect_tool "cmake" "CMAKE" "cmake"
        detect_tool "pkg-config" "PKG_CONFIG" "pkg-config"
        detect_tool "ninja" "NINJA" "ninja"
        
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