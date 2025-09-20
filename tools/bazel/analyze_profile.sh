#!/bin/bash
# Быстрый анализ профилей Bazel

set -euo pipefail

echo "📊 Анализ профилей Bazel"
echo "========================"

# Проверяем наличие профилей
PROFILES=()
[[ -f "profile.json" ]] && PROFILES+=("profile.json")
[[ -f "profile-slim.json" ]] && PROFILES+=("profile-slim.json")

if [[ ${#PROFILES[@]} -eq 0 ]]; then
    echo "❌ Профили не найдены!"
    echo ""
    echo "💡 Создайте профиль:"
    echo "   bazel build --config=profile //...       # Полный"
    echo "   bazel build --config=profile-slim //...  # Быстрый"
    exit 1
fi

echo "🔍 Найденные профили:"
for profile in "${PROFILES[@]}"; do
    size=$(du -h "$profile" | cut -f1)
    echo "   ✅ $profile ($size)"
done

echo ""

# Анализ каждого профиля
for profile in "${PROFILES[@]}"; do
    echo "📈 Анализ: $profile"
    echo "$(printf '=%.0s' {1..50})"
    
    # Краткая статистика
    echo "⏱️  Анализ производительности:"
    bazel analyze-profile "$profile" 2>/dev/null | grep -E "(Total|Critical path)" | head -10 || echo "Анализ недоступен"
    
    echo ""
    echo "� Основные метрики:"
    echo "   Размер профиля: $(du -h "$profile" | cut -f1)"
    echo "   Создан: $(stat -f %Sm "$profile")"
    
    echo ""
    echo "$(printf '=%.0s' {1..50})"
    echo ""
done

echo "🔗 Веб-анализ:"
echo "=============="
echo "1. Откройте: https://ui.perfetto.dev/"
echo "2. Загрузите профиль: ${PROFILES[0]}"
echo "3. Изучите Timeline и Action Graph"

echo ""
echo "📋 CLI команды:"
echo "==============="
echo "# Детальный анализ"
echo "bazel analyze-profile ${PROFILES[0]}"
echo ""
echo "# HTML отчет"
echo "bazel analyze-profile --html --html_details ${PROFILES[0]}"
echo ""
echo "# CSV экспорт"
echo "bazel analyze-profile --dump=action --format=csv ${PROFILES[0]} > actions.csv"

echo ""
echo "🎯 Рекомендации:"
echo "================"

# Проверяем размер профиля для рекомендаций
if [[ -f "profile.json" ]]; then
    profile_size=$(stat -f%z "profile.json")
    if [[ $profile_size -gt 1000000 ]]; then
        echo "⚠️  Большой профиль (${profile_size} bytes) - рассмотрите --config=profile-slim"
    fi
fi

echo "🚀 Быстрое профилирование: bazel build --config=profile-slim //..."
echo "🔍 Детальное профилирование: bazel build --config=profile //..."
echo "🧹 Очистка перед профилированием: bazel clean --expunge"