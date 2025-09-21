#!/bin/bash
# Инициализация локальной конфигурации Bazel (.local.bazelrc)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
LOCAL_RC="$PROJECT_ROOT/.local.bazelrc"

echo "🔍 Инициализация локальной конфигурации Bazel..."

# Создаем .local.bazelrc
echo "# Автогенерированный .local.bazelrc" > "$LOCAL_RC"
echo "# Не добавлять в git!" >> "$LOCAL_RC"
echo "# Генератор: tools/bazel/local-init.sh" >> "$LOCAL_RC"
echo "# Дата создания: $(date)" >> "$LOCAL_RC"
echo "" >> "$LOCAL_RC"

# Определение ОС
case "$(uname -s)" in
    Darwin*)
        echo "🍎 Обнаружена macOS"
        echo "# === MACOS DETECTED ===" >> "$LOCAL_RC"
        echo "# build --config=macos # добавляется неявно из-за --enable_platform_specific_config" >> "$LOCAL_RC"
        
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
        
        # Проверка и настройка EMSDK
        echo "" >> "$LOCAL_RC"
        echo "# === EMSDK DETECTION ===" >> "$LOCAL_RC"
        if [[ -n "${EMSDK:-}" ]] && [[ -d "$EMSDK" ]]; then
            echo "build --action_env=EMSDK=$EMSDK" >> "$LOCAL_RC"
            echo "build --action_env=EMSCRIPTEN_ROOT=$EMSDK/upstream/emscripten" >> "$LOCAL_RC"
            echo "build --action_env=EM_CACHE=$EMSDK/upstream/emscripten/cache" >> "$LOCAL_RC"
            echo "   ✅ EMSDK: $EMSDK (из переменной окружения)"
        elif [[ -d "/Users/rix/emsdk" ]]; then
            echo "build --action_env=EMSDK=/Users/rix/emsdk" >> "$LOCAL_RC"
            echo "build --action_env=EMSCRIPTEN_ROOT=/Users/rix/emsdk/upstream/emscripten" >> "$LOCAL_RC"
            echo "build --action_env=EM_CACHE=/Users/rix/emsdk/upstream/emscripten/cache" >> "$LOCAL_RC"
            echo "   ✅ EMSDK: /Users/rix/emsdk (стандартная локация)"
        elif [[ -d "$HOME/emsdk" ]]; then
            echo "build --action_env=EMSDK=$HOME/emsdk" >> "$LOCAL_RC"
            echo "build --action_env=EMSCRIPTEN_ROOT=$HOME/emsdk/upstream/emscripten" >> "$LOCAL_RC"
            echo "build --action_env=EM_CACHE=$HOME/emsdk/upstream/emscripten/cache" >> "$LOCAL_RC"
            echo "   ✅ EMSDK: $HOME/emsdk (в домашней директории)"
        else
            echo "# build --action_env=EMSDK=/path/to/emsdk  # EMSDK не найден" >> "$LOCAL_RC"
            echo "# build --action_env=EMSCRIPTEN_ROOT=/path/to/emsdk/upstream/emscripten" >> "$LOCAL_RC"
            echo "# build --action_env=EM_CACHE=/path/to/emsdk/upstream/emscripten/cache" >> "$LOCAL_RC"
            echo "   ⚠️  EMSDK: не найден"
            echo "      Установка: git clone https://github.com/emscripten-core/emsdk.git ~/emsdk"
            echo "      Активация: cd ~/emsdk && ./emsdk install latest && ./emsdk activate latest"
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
        echo "# build --config=linux # добавляется неявно из-за --enable_platform_specific_config" >> "$LOCAL_RC"
        
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
        
        # Проверка и настройка EMSDK для Linux
        echo "" >> "$LOCAL_RC"
        echo "# === EMSDK DETECTION ===" >> "$LOCAL_RC"
        if [[ -n "${EMSDK:-}" ]] && [[ -d "$EMSDK" ]]; then
            echo "build --action_env=EMSDK=$EMSDK" >> "$LOCAL_RC"
            echo "build --action_env=EMSCRIPTEN_ROOT=$EMSDK/upstream/emscripten" >> "$LOCAL_RC"
            echo "build --action_env=EM_CACHE=$EMSDK/upstream/emscripten/cache" >> "$LOCAL_RC"
            echo "   ✅ EMSDK: $EMSDK (из переменной окружения)"
        elif [[ -d "$HOME/emsdk" ]]; then
            echo "build --action_env=EMSDK=$HOME/emsdk" >> "$LOCAL_RC"
            echo "build --action_env=EMSCRIPTEN_ROOT=$HOME/emsdk/upstream/emscripten" >> "$LOCAL_RC"
            echo "build --action_env=EM_CACHE=$HOME/emsdk/upstream/emscripten/cache" >> "$LOCAL_RC"
            echo "   ✅ EMSDK: $HOME/emsdk (в домашней директории)"
        else
            echo "# build --action_env=EMSDK=/path/to/emsdk  # EMSDK не найден" >> "$LOCAL_RC"
            echo "# build --action_env=EMSCRIPTEN_ROOT=/path/to/emsdk/upstream/emscripten" >> "$LOCAL_RC"
            echo "# build --action_env=EM_CACHE=/path/to/emsdk/upstream/emscripten/cache" >> "$LOCAL_RC"
            echo "   ⚠️  EMSDK: не найден"
            echo "      Установка: git clone https://github.com/emscripten-core/emsdk.git ~/emsdk"
            echo "      Активация: cd ~/emsdk && ./emsdk install latest && ./emsdk activate latest"
        fi
        ;;
        
    MINGW*|MSYS*|CYGWIN*)
        echo "🪟 Обнаружена Windows"
        echo "# === WINDOWS DETECTED ===" >> "$LOCAL_RC"
        echo "# build --config=windows # добавляется неявно из-за --enable_platform_specific_config" >> "$LOCAL_RC"
        
        # Поиск инструментов Windows
        if [[ -f "/c/Program Files/CMake/bin/cmake.exe" ]]; then
            echo "build --action_env=CMAKE=\"C:/Program Files/CMake/bin/cmake.exe\"" >> "$LOCAL_RC"
            echo "   ✅ CMake: C:/Program Files/CMake/bin/cmake.exe"
        fi
        
        # Проверка и настройка EMSDK для Windows
        echo "" >> "$LOCAL_RC"
        echo "# === EMSDK DETECTION ===" >> "$LOCAL_RC"
        if [[ -n "${EMSDK:-}" ]] && [[ -d "$EMSDK" ]]; then
            # Конвертируем Unix путь в Windows путь
            local win_emsdk=$(echo "$EMSDK" | sed 's|^/c/|C:/|')
            echo "build --action_env=EMSDK=\"$win_emsdk\"" >> "$LOCAL_RC"
            echo "build --action_env=EMSCRIPTEN_ROOT=\"$win_emsdk/upstream/emscripten\"" >> "$LOCAL_RC"
            echo "build --action_env=EM_CACHE=\"$win_emsdk/upstream/emscripten/cache\"" >> "$LOCAL_RC"
            echo "   ✅ EMSDK: $win_emsdk (из переменной окружения)"
        elif [[ -d "/c/emsdk" ]]; then
            echo "build --action_env=EMSDK=\"C:/emsdk\"" >> "$LOCAL_RC"
            echo "build --action_env=EMSCRIPTEN_ROOT=\"C:/emsdk/upstream/emscripten\"" >> "$LOCAL_RC"
            echo "build --action_env=EM_CACHE=\"C:/emsdk/upstream/emscripten/cache\"" >> "$LOCAL_RC"
            echo "   ✅ EMSDK: C:/emsdk (стандартная локация)"
        else
            echo "# build --action_env=EMSDK=\"C:/path/to/emsdk\"  # EMSDK не найден" >> "$LOCAL_RC"
            echo "# build --action_env=EMSCRIPTEN_ROOT=\"C:/path/to/emsdk/upstream/emscripten\"" >> "$LOCAL_RC"
            echo "# build --action_env=EM_CACHE=\"C:/path/to/emsdk/upstream/emscripten/cache\"" >> "$LOCAL_RC"
            echo "   ⚠️  EMSDK: не найден"
            echo "      Установка: git clone https://github.com/emscripten-core/emsdk.git C:/emsdk"
            echo "      Активация: cd C:/emsdk && emsdk install latest && emsdk activate latest"
        fi
        ;;
        
    *)
        echo "❓ Неизвестная ОС, используем Linux настройки"
        echo "# === UNKNOWN OS (using Linux defaults) ===" >> "$LOCAL_RC"
        echo "# Не добавляем --config=linux автоматически (избегаем дублирования)" >> "$LOCAL_RC"
        echo "# build --config=linux  # ОТКЛЮЧЕНО: будет установлено через aliases" >> "$LOCAL_RC"
        
        # Проверка и настройка EMSDK для неизвестной ОС
        echo "" >> "$LOCAL_RC"
        echo "# === EMSDK DETECTION ===" >> "$LOCAL_RC"
        if [[ -n "${EMSDK:-}" ]] && [[ -d "$EMSDK" ]]; then
            echo "build --action_env=EMSDK=$EMSDK" >> "$LOCAL_RC"
            echo "build --action_env=EMSCRIPTEN_ROOT=$EMSDK/upstream/emscripten" >> "$LOCAL_RC"
            echo "build --action_env=EM_CACHE=$EMSDK/upstream/emscripten/cache" >> "$LOCAL_RC"
            echo "   ✅ EMSDK: $EMSDK (из переменной окружения)"
        elif [[ -d "$HOME/emsdk" ]]; then
            echo "build --action_env=EMSDK=$HOME/emsdk" >> "$LOCAL_RC"
            echo "build --action_env=EMSCRIPTEN_ROOT=$HOME/emsdk/upstream/emscripten" >> "$LOCAL_RC"
            echo "build --action_env=EM_CACHE=$HOME/emsdk/upstream/emscripten/cache" >> "$LOCAL_RC"
            echo "   ✅ EMSDK: $HOME/emsdk (в домашней директории)"
        else
            echo "# build --action_env=EMSDK=/path/to/emsdk  # EMSDK не найден" >> "$LOCAL_RC"
            echo "# build --action_env=EMSCRIPTEN_ROOT=/path/to/emsdk/upstream/emscripten" >> "$LOCAL_RC"
            echo "# build --action_env=EM_CACHE=/path/to/emsdk/upstream/emscripten/cache" >> "$LOCAL_RC"
            echo "   ⚠️  EMSDK: не найден"
            echo "      Установка: git clone https://github.com/emscripten-core/emsdk.git ~/emsdk"
            echo "      Активация: cd ~/emsdk && ./emsdk install latest && ./emsdk activate latest"
        fi
        ;;
esac

# Определение CI/CD среды
if [[ -n "${CI:-}" ]] || [[ -n "${GITHUB_ACTIONS:-}" ]] || [[ -n "${JENKINS_URL:-}" ]] || [[ -n "${BUILDKITE:-}" ]]; then
    echo "🤖 CI/CD среда обнаружена"
    echo "build --config=ci" >> "$LOCAL_RC"
fi

# Добавление пользовательских настроек по умолчанию
echo "" >> "$LOCAL_RC"
echo "# === ПЕРСОНАЛЬНЫЕ НАСТРОЙКИ ===" >> "$LOCAL_RC"
echo "# Раскомментируйте и настройте по необходимости:" >> "$LOCAL_RC"
echo "# build --jobs=16" >> "$LOCAL_RC"
echo "# build --local_cpu_resources=16" >> "$LOCAL_RC"
echo "# build --local_ram_resources=32768" >> "$LOCAL_RC"

echo "" >> "$LOCAL_RC"
echo "# === РЕШЕНИЕ ПРОБЛЕМЫ С rules_foreign_cc ===" >> "$LOCAL_RC"
echo "# Принудительное использование системных инструментов (частично работает)" >> "$LOCAL_RC"
echo "build --action_env=BAZEL_USE_CPP_ONLY_TOOLCHAIN=1" >> "$LOCAL_RC"

# Добавление режима по умолчанию
echo "" >> "$LOCAL_RC"
echo "# === DEFAULT MODE ===" >> "$LOCAL_RC"
echo "# Используем dev вместо debug/release для избежания конфликтов" >> "$LOCAL_RC"
echo "# build --config=dev  # ОТКЛЮЧЕНО: будет установлено через aliases" >> "$LOCAL_RC"

echo ""
echo "✅ Создан файл: $LOCAL_RC"
echo ""
echo "📋 Содержимое:"
echo "$(cat "$LOCAL_RC")"
echo ""
echo "💡 Теперь можно использовать комбинированные конфигурации:"
echo "   bazel build --config=mac-dev //...       # macOS + host + dev"
echo "   bazel build --config=mac-release //...   # macOS + host + release"
echo "   bazel build --config=mac-debug //...     # macOS + host + debug"
echo "   bazel build --config=wasm-dev //...      # macOS + wasm + dev"
echo ""
echo "ℹ️  Платформенные и режимные конфигурации устанавливаются"
echo "   только через aliases в .bazelrc для избежания дублирования"
echo ""
echo "🔄 Для обновления локальной конфигурации запустите:"
echo "   tools/bazel/local-init.sh"
