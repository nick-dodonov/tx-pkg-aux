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
            for tool in pkg-config ninja; do
                if [[ -f "$HOMEBREW_PREFIX/bin/$tool" ]]; then
                    tool_upper=$(echo "$tool" | tr '[:lower:]' '[:upper:]' | tr '-' '_')
                    echo "build --action_env=${tool_upper}=$HOMEBREW_PREFIX/bin/$tool" >> "$LOCAL_RC"
                    echo "   ✅ $tool: $HOMEBREW_PREFIX/bin/$tool"
                else
                    echo "   ⚠️  $tool: не найден в Homebrew"
                fi
            done
            
        # Специальная проверка Make с приоритетами на macOS
        MAKE_FOUND=false
        # 1. Сначала ищем GNU Make в Homebrew (предпочтительнее)
        if [[ -f "$HOMEBREW_PREFIX/bin/gmake" ]]; then
            echo "build --action_env=MAKE=$HOMEBREW_PREFIX/bin/gmake" >> "$LOCAL_RC"
            echo "   ✅ make: $HOMEBREW_PREFIX/bin/gmake (GNU Make, Homebrew)"
            MAKE_FOUND=true
        elif [[ -f "$HOMEBREW_PREFIX/bin/make" ]]; then
            echo "build --action_env=MAKE=$HOMEBREW_PREFIX/bin/make" >> "$LOCAL_RC"
            echo "   ✅ make: $HOMEBREW_PREFIX/bin/make (Homebrew)"
            MAKE_FOUND=true
        fi
        
        # 2. Если не найден в Homebrew, ищем в стандартных локациях
        if [[ "$MAKE_FOUND" == "false" ]]; then
            if [[ -f "/usr/local/bin/gmake" ]]; then
                echo "build --action_env=MAKE=/usr/local/bin/gmake" >> "$LOCAL_RC"
                echo "   ✅ make: /usr/local/bin/gmake (GNU Make)"
                MAKE_FOUND=true
            elif [[ -f "/usr/local/bin/make" ]]; then
                echo "build --action_env=MAKE=/usr/local/bin/make" >> "$LOCAL_RC"
                echo "   ✅ make: /usr/local/bin/make"
                MAKE_FOUND=true
            elif [[ -f "/usr/bin/make" ]]; then
                # Проверяем версию системного make
                MAKE_VERSION=$(/usr/bin/make --version 2>/dev/null | head -1)
                echo "build --action_env=MAKE=/usr/bin/make" >> "$LOCAL_RC"
                echo "   ✅ make: /usr/bin/make (системный - $MAKE_VERSION)"
                MAKE_FOUND=true
            elif command -v make >/dev/null 2>&1; then
                MAKE_PATH=$(which make)
                echo "build --action_env=MAKE=$MAKE_PATH" >> "$LOCAL_RC"
                echo "   ✅ make: $MAKE_PATH (в PATH)"
                MAKE_FOUND=true
            fi
            
            if [[ "$MAKE_FOUND" == "false" ]]; then
                echo "   ⚠️  make: не найден"
                echo "      Системный: xcode-select --install"
                echo "      GNU Make: brew install make"
            fi
        fi
            
            # Специальная проверка CMake с несколькими локациями
            CMAKE_FOUND=false
            if [[ -f "$HOMEBREW_PREFIX/bin/cmake" ]]; then
                echo "build --action_env=CMAKE=$HOMEBREW_PREFIX/bin/cmake" >> "$LOCAL_RC"
                echo "   ✅ cmake: $HOMEBREW_PREFIX/bin/cmake (Homebrew)"
                CMAKE_FOUND=true
            fi
        else
            echo "   ⚠️  Homebrew не найден"
        fi
        
        # Специальная проверка Make с приоритетами на macOS
        MAKE_FOUND=false
        # 1. Сначала ищем GNU Make в Homebrew (предпочтительнее)
        if command -v brew >/dev/null 2>&1; then
            HOMEBREW_PREFIX=$(brew --prefix)
            if [[ -f "$HOMEBREW_PREFIX/bin/gmake" ]]; then
                echo "build --action_env=MAKE=$HOMEBREW_PREFIX/bin/gmake" >> "$LOCAL_RC"
                echo "   ✅ make: $HOMEBREW_PREFIX/bin/gmake (GNU Make, Homebrew)"
                MAKE_FOUND=true
            elif [[ -f "$HOMEBREW_PREFIX/bin/make" ]]; then
                echo "build --action_env=MAKE=$HOMEBREW_PREFIX/bin/make" >> "$LOCAL_RC"
                echo "   ✅ make: $HOMEBREW_PREFIX/bin/make (Homebrew)"
                MAKE_FOUND=true
            fi
        fi
        
        # 2. Если не найден в Homebrew, ищем в стандартных локациях
        if [[ "$MAKE_FOUND" == "false" ]]; then
            if [[ -f "/usr/local/bin/gmake" ]]; then
                echo "build --action_env=MAKE=/usr/local/bin/gmake" >> "$LOCAL_RC"
                echo "   ✅ make: /usr/local/bin/gmake (GNU Make)"
                MAKE_FOUND=true
            elif [[ -f "/usr/local/bin/make" ]]; then
                echo "build --action_env=MAKE=/usr/local/bin/make" >> "$LOCAL_RC"
                echo "   ✅ make: /usr/local/bin/make"
                MAKE_FOUND=true
            elif [[ -f "/usr/bin/make" ]]; then
                # Проверяем версию системного make
                MAKE_VERSION=$(/usr/bin/make --version 2>/dev/null | head -1)
                echo "build --action_env=MAKE=/usr/bin/make" >> "$LOCAL_RC"
                echo "   ✅ make: /usr/bin/make (системный - $MAKE_VERSION)"
                MAKE_FOUND=true
            elif command -v make >/dev/null 2>&1; then
                MAKE_PATH=$(which make)
                echo "build --action_env=MAKE=$MAKE_PATH" >> "$LOCAL_RC"
                echo "   ✅ make: $MAKE_PATH (в PATH)"
                MAKE_FOUND=true
            fi
            
            if [[ "$MAKE_FOUND" == "false" ]]; then
                echo "   ⚠️  make: не найден"
                echo "      Системный: xcode-select --install"
                echo "      GNU Make: brew install make"
            fi
        fi
        
        # Проверка CMake в стандартных локациях macOS (если не найден в Homebrew)
        CMAKE_FOUND=false
        if command -v brew >/dev/null 2>&1; then
            HOMEBREW_PREFIX=$(brew --prefix)
            if [[ -f "$HOMEBREW_PREFIX/bin/cmake" ]]; then
                CMAKE_FOUND=true
            fi
        fi
        
        if [[ "$CMAKE_FOUND" == "false" ]]; then
            if [[ -f "/Applications/CMake.app/Contents/bin/cmake" ]]; then
                echo "build --action_env=CMAKE=/Applications/CMake.app/Contents/bin/cmake" >> "$LOCAL_RC"
                echo "   ✅ cmake: /Applications/CMake.app/Contents/bin/cmake (CMake.app)"
                CMAKE_FOUND=true
            elif [[ -f "/usr/local/bin/cmake" ]]; then
                echo "build --action_env=CMAKE=/usr/local/bin/cmake" >> "$LOCAL_RC"
                echo "   ✅ cmake: /usr/local/bin/cmake"
                CMAKE_FOUND=true
            elif command -v cmake >/dev/null 2>&1; then
                CMAKE_PATH=$(which cmake)
                echo "build --action_env=CMAKE=$CMAKE_PATH" >> "$LOCAL_RC"
                echo "   ✅ cmake: $CMAKE_PATH (в PATH)"
                CMAKE_FOUND=true
            fi
            
            if [[ "$CMAKE_FOUND" == "false" ]]; then
                echo "   ⚠️  cmake: не найден"
                echo "      Попробуйте установить: brew install cmake"
                echo "      Или скачать с: https://cmake.org/download/"
            fi
        fi
        
        # Проверка других инструментов в системных путях (если не найдены в Homebrew)
        for tool in pkg-config ninja; do
            tool_upper=$(echo "$tool" | tr '[:lower:]' '[:upper:]' | tr '-' '_')
            # Проверяем, не был ли уже найден в Homebrew
            if ! grep -q "${tool_upper}=" "$LOCAL_RC"; then
                if command -v "$tool" >/dev/null 2>&1; then
                    TOOL_PATH=$(which "$tool")
                    echo "build --action_env=${tool_upper}=$TOOL_PATH" >> "$LOCAL_RC"
                    echo "   ✅ $tool: $TOOL_PATH (системный)"
                else
                    echo "   ⚠️  $tool: не найден в системе"
                    case "$tool" in
                        pkg-config)
                            echo "      Установка: brew install pkg-config"
                            ;;
                        ninja)
                            echo "      Установка: brew install ninja"
                            ;;
                    esac
                fi
            fi
        done
        
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