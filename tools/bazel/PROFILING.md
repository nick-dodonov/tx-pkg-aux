# 📊 Руководство по профилированию Bazel

## 🎯 Доступные конфигурации профилирования

### 1. `--config=profile` - Полное профилирование
**Использование:** Детальный анализ производительности
```bash
bazel build --config=profile //...
```

**Настройки:**
- `--noslim_profile` - отключает сжатие для максимальной детализации
- `--experimental_profile_include_target_label` - включает метки целей
- `--experimental_profile_include_primary_output` - включает выходные файлы
- `--record_full_profiler_data` - записывает полные данные профайлера

**Результат:** `profile.json` (~3.7MB)
- Полная информация о всех действиях
- Детальные метки целей
- Информация о выходных файлах
- Максимальная детализация для анализа

### 2. `--config=profile-slim` - Быстрое профилирование  
**Использование:** Быстрый анализ, CI/CD, регулярный мониторинг
```bash
bazel build --config=profile-slim //...
```

**Настройки:**
- `--slim_profile` - включает сжатие данных
- `--experimental_profile_include_target_label` - включает метки целей
- Без детальной информации о выходных файлах

**Результат:** `profile-slim.json` (~249KB)
- Сжатые данные профилирования
- Основные метрики производительности
- Подходит для автоматического анализа

## 🔧 Исправленная проблема

### ❌ Была проблема:
```
WARNING: Enabling both --slim_profile and --experimental_profile_include_primary_output: 
the "out" field will be omitted in merged actions.
```

### ✅ Решение:
Разделены две конфигурации:
- **Полное профилирование**: `--noslim_profile` + все детальные флаги
- **Быстрое профилирование**: `--slim_profile` + основные флаги

## 📈 Анализ профилей

### Веб-интерфейс (рекомендуется):
```bash
# Откройте https://ui.perfetto.dev/
# Загрузите profile.json или profile-slim.json
```

### CLI анализ:
```bash
# Общий анализ
bazel analyze-profile profile.json

# Критический путь
bazel analyze-profile --dump=action profile.json | grep "CRITICAL_PATH"

# Самые медленные действия
bazel analyze-profile --dump=action profile.json | head -20
```

### Автоматический анализ:
```bash
# Создать HTML отчет
bazel analyze-profile --html --html_details profile.json

# Экспорт в CSV
bazel analyze-profile --dump=action --format=csv profile.json > actions.csv
```

## 🎯 Рекомендации использования

### Для разработки:
```bash
bazel build --config=profile //...          # Детальный анализ
```

### Для CI/CD:
```bash
bazel build --config=profile-slim //...     # Быстрый мониторинг
```

### Для отладки производительности:
```bash
# 1. Полная очистка
bazel clean --expunge

# 2. Профилирование чистой сборки
bazel build --config=profile //...

# 3. Анализ критического пути
bazel analyze-profile profile.json
```

## 📊 Размеры файлов профилирования

| Конфигурация | Размер файла | Детализация | Время анализа |
|-------------|-------------|-------------|---------------|
| `profile` | ~3.7MB | Максимальная | Медленный |
| `profile-slim` | ~249KB | Основная | Быстрый |

## 🚀 Интеграция в CI/CD

```yaml
# GitHub Actions пример
- name: Build with profiling
  run: bazel build --config=profile-slim //...
  
- name: Upload profile
  uses: actions/upload-artifact@v3
  with:
    name: bazel-profile
    path: profile-slim.json
```

## 🔍 Типичные проблемы производительности

1. **Медленная загрузка пакетов** → проблемы с MODULE.bazel
2. **Долгая компиляция** → проблемы с кешированием
3. **Медленное связывание** → большие зависимости
4. **Bootstrap операции** → отсутствие системных инструментов

Все эти проблемы видны в профиле! 📈