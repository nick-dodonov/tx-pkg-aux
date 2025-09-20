#!/bin/bash
# Проверка конфигурации и доступности Registry

set -euo pipefail

echo "🔍 ПРОВЕРКА КОНФИГУРАЦИИ REGISTRY"
echo "================================="

echo ""
echo "1️⃣ Настроенные registry в .bazelrc:"
echo "-----------------------------------"
if [[ -f ".bazelrc" ]]; then
    echo "Из .bazelrc:"
    grep -E "(common|build|query|mod).*--registry" .bazelrc | sed 's/^/   /'
else
    echo "❌ .bazelrc не найден"
fi

echo ""
echo "2️⃣ Проверка доступности registry:"
echo "---------------------------------"

check_registry() {
    local registry_url="$1"
    echo "🌐 Проверка: $registry_url"
    
    # Убираем завершающий слеш если есть
    registry_url="${registry_url%/}"
    
    if curl -I --connect-timeout 5 --max-time 10 "$registry_url" >/dev/null 2>&1; then
        echo "   ✅ Доступен"
        
        # Дополнительная проверка для Bazel Central Registry
        if [[ "$registry_url" == *"bcr.bazel.build"* ]]; then
            if curl -I --connect-timeout 5 --max-time 10 "$registry_url/modules" >/dev/null 2>&1; then
                echo "   ✅ /modules endpoint доступен"
            else
                echo "   ⚠️  /modules endpoint недоступен"
            fi
        fi
        
        # Проверка для GitHub raw registry
        if [[ "$registry_url" == *"raw.githubusercontent.com"* ]]; then
            local test_path="$registry_url/modules"
            if curl -I --connect-timeout 5 --max-time 10 "$test_path" >/dev/null 2>&1; then
                echo "   ✅ GitHub registry структура корректна"
            else
                echo "   ⚠️  GitHub registry может не иметь modules директории"
            fi
        fi
    else
        echo "   ❌ Недоступен или медленно отвечает"
    fi
}

# Извлекаем все registry из .bazelrc
if [[ -f ".bazelrc" ]]; then
    grep -E "\--registry=" .bazelrc | while IFS= read -r line; do
        if [[ "$line" =~ --registry=([^ ]+) ]]; then
            registry="${BASH_REMATCH[1]}"
            check_registry "$registry"
        fi
    done
fi

echo ""
echo "3️⃣ Проверка резолюции модулей:"
echo "------------------------------"
echo "Тестируем загрузку информации о модулях..."

# Проверяем что Bazel может видеть registry
if bazel mod graph --output=text >/dev/null 2>&1; then
    echo "✅ Bazel успешно читает конфигурацию registry"
    
    # Проверяем конкретные модули из нашего проекта
    echo ""
    echo "📦 Используемые модули:"
    if [[ -f "MODULE.bazel" ]]; then
        grep "bazel_dep" MODULE.bazel | while read -r line; do
            if [[ "$line" =~ name\ =\ \"([^\"]+)\".*version\ =\ \"([^\"]+)\" ]]; then
                module_name="${BASH_REMATCH[1]}"
                module_version="${BASH_REMATCH[2]}"
                echo "   🔍 $module_name@$module_version"
            fi
        done
    fi
else
    echo "❌ Ошибка при чтении конфигурации модулей"
    echo "   Возможные причины:"
    echo "   - Registry недоступен"
    echo "   - Некорректный MODULE.bazel"  
    echo "   - Проблемы с сетью"
fi

echo ""
echo "4️⃣ Диагностика флагов Bazel:"
echo "----------------------------"
echo "Флаги registry, используемые Bazel:"

# Показываем какие флаги реально применяются
bazel build //... --announce_rc --nobuild 2>&1 | grep -E "(registry|Registry)" | head -10 | sed 's/^/   /' || echo "   Флаги registry не найдены в выводе"

echo ""
echo "5️⃣ Тест конкретных модулей:"
echo "---------------------------"

test_module_availability() {
    local module_name="$1"
    local registry_base="$2"
    
    echo "🔍 Тест модуля: $module_name"
    
    # Для GitHub registry проверяем прямую доступность
    if [[ "$registry_base" == *"github"* ]]; then
        local module_url="$registry_base/modules/$module_name"
        if curl -I --connect-timeout 5 --max-time 10 "$module_url" >/dev/null 2>&1; then
            echo "   ✅ Модуль $module_name доступен в $registry_base"
        else
            echo "   ❌ Модуль $module_name недоступен в $registry_base"
        fi
    fi
}

# Тестируем наши кастомные модули
if grep -q "tx-kit-repo" .bazelrc 2>/dev/null; then
    kit_repo_url=$(grep "tx-kit-repo" .bazelrc | head -1 | sed 's/.*--registry=\([^ ]*\).*/\1/')
    test_module_availability "lwlog" "$kit_repo_url"
fi

echo ""
echo "6️⃣ Рекомендации:"
echo "---------------"

recommendations=()

# Проверяем оптимизацию конфигурации
registry_lines=$(grep -c -E "--registry=" .bazelrc 2>/dev/null || echo "0")
if [[ "$registry_lines" -gt 4 ]]; then
    recommendations+=("🔄 Используйте 'common --registry=' вместо повторения для каждой команды")
fi

# Проверяем HTTPS
if grep -q "http://" .bazelrc 2>/dev/null; then
    recommendations+=("🔒 Замените HTTP registry на HTTPS для безопасности")
fi

# Проверяем порядок registry
if grep -n "bcr.bazel.build" .bazelrc | head -1 | grep -q ":1:"; then
    recommendations+=("📊 Рассмотрите размещение приватных registry перед bcr.bazel.build")
fi

if [[ ${#recommendations[@]} -eq 0 ]]; then
    echo "✅ Конфигурация registry оптимальна!"
else
    for rec in "${recommendations[@]}"; do
        echo "$rec"
    done
fi

echo ""
echo "7️⃣ Команды для отладки:"
echo "----------------------"
echo "# Показать граф модулей:"
echo "bazel mod graph"
echo ""
echo "# Показать используемые флаги:"
echo "bazel build //... --announce_rc | grep registry"
echo ""
echo "# Обновить lock файл модулей:"
echo "bazel mod deps --lockfile_mode=update"
echo ""
echo "# Проверить конкретный модуль:"
echo "bazel mod show_repo <module_name>"

echo ""
echo "🎯 Статус registry:"
echo "=================="

if bazel info >/dev/null 2>&1; then
    echo "✅ Bazel работает корректно"
    if bazel mod graph --output=text >/dev/null 2>&1; then
        echo "✅ Registry настроены корректно"
        echo "✅ Модули резолвятся успешно"
    else
        echo "⚠️  Проблемы с резолюцией модулей"
    fi
else
    echo "❌ Проблемы с конфигурацией Bazel"
fi