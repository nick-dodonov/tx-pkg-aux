# 📋 Конфигурация Registry в Bazel

## 🎯 Проблема дублирования

### ❌ **Было (дублирование):**
```bazelrc
build --registry=https://bcr.bazel.build/
build --registry=https://raw.githubusercontent.com/nick-dodonov/tx-kit-repo/main
query --registry=https://bcr.bazel.build/
query --registry=https://raw.githubusercontent.com/nick-dodonov/tx-kit-repo/main
mod --registry=https://bcr.bazel.build/
mod --registry=https://raw.githubusercontent.com/nick-dodonov/tx-kit-repo/main
```

### ✅ **Стало (оптимизировано):**
```bazelrc
# Общие настройки для всех команд
common --registry=https://bcr.bazel.build/
common --registry=https://raw.githubusercontent.com/nick-dodonov/tx-kit-repo/main
```

## 🔧 Способы конфигурации Registry

### 1. **`common` - Универсальный (рекомендуется)**
```bazelrc
common --registry=https://bcr.bazel.build/
common --registry=https://raw.githubusercontent.com/your-org/bazel-registry/main
```

**Применяется к:** `build`, `query`, `test`, `mod`, `run`, `coverage`, etc.
**Преимущества:** 
- ✅ Нет дублирования
- ✅ Работает для всех команд
- ✅ Легко поддерживать

### 2. **Отдельно для каждой команды**
```bazelrc
build --registry=https://bcr.bazel.build/
query --registry=https://bcr.bazel.build/
mod --registry=https://bcr.bazel.build/
test --registry=https://bcr.bazel.build/
```

**Когда использовать:** Если нужны разные registry для разных команд

### 3. **Через переменные окружения**
```bash
export BAZEL_REGISTRY="https://bcr.bazel.build/,https://your-registry.com/modules"
```

**Преимущества:** Не привязано к проекту, можно переопределять

### 4. **Комбинированный подход**
```bazelrc
# Базовые registry для всех команд
common --registry=https://bcr.bazel.build/

# Дополнительные для специфических команд
build --registry=https://private-registry.company.com/
mod --registry=https://dev-registry.company.com/
```

## 📊 Сравнение подходов

| Подход | Строк кода | Гибкость | Поддержка | Рекомендация |
|--------|------------|----------|-----------|--------------|
| `common` | 2 | ⭐⭐⭐ | ⭐⭐⭐⭐⭐ | ✅ **Лучший** |
| По командам | 6+ | ⭐⭐⭐⭐⭐ | ⭐⭐ | Для сложных случаев |
| Переменные | 0 | ⭐⭐⭐⭐ | ⭐⭐⭐ | Для локальной разработки |
| Комбинированный | 4+ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐ | Для enterprise |

## 🎛️ Расширенная конфигурация

### **Приоритет registry (порядок важен!):**
```bazelrc
# 1. Сначала проверяется приватный registry
common --registry=https://private.company.com/bazel-modules/
# 2. Потом основной Bazel Central Registry  
common --registry=https://bcr.bazel.build/
# 3. Fallback registry
common --registry=https://backup-registry.company.com/
```

### **Условные registry (по окружению):**
```bazelrc
# Базовый registry
common --registry=https://bcr.bazel.build/

# Для CI/CD
common:ci --registry=https://ci-registry.company.com/
# Для разработки
common:dev --registry=https://dev-registry.company.com/
```

### **Registry с аутентификацией:**
```bazelrc
common --registry=https://bcr.bazel.build/
common --registry=https://user:pass@private-registry.com/modules/
```

## 🔍 Диагностика Registry

### **Проверить какие registry используются:**
```bash
# Показать все флаги включая registry
bazel build //... --announce_rc | grep registry

# Проверить конкретную команду
bazel query --announce_rc | grep registry
```

### **Отладка модулей:**
```bash
# Показать граф модулей
bazel mod graph

# Показать откуда берутся модули
bazel mod deps --verbose

# Показать все доступные версии модуля
bazel mod show_repo lwlog
```

### **Проверить доступность registry:**
```bash
# Проверить основной registry
curl -I https://bcr.bazel.build/

# Проверить ваш приватный registry
curl -I https://raw.githubusercontent.com/nick-dodonov/tx-kit-repo/main/

# Проверить конкретный модуль
curl -I https://raw.githubusercontent.com/nick-dodonov/tx-kit-repo/main/modules/lwlog/1.4.0/MODULE.bazel
```

## 🚀 Лучшие практики

### ✅ **Рекомендуется:**
1. **Использовать `common`** для большинства случаев
2. **Порядок registry имеет значение** - сначала приватные, потом публичные
3. **Документировать registry** в README проекта
4. **Тестировать доступность** registry в CI/CD

### ❌ **Избегать:**
1. Дублирования настроек registry
2. Хардкода credentials в .bazelrc (использовать переменные)
3. Слишком много registry (замедляет резолюцию)
4. Registry без HTTPS (небезопасно)

## 🎯 Итоговая конфигурация

### **Простая (для большинства проектов):**
```bazelrc
# === MODULE REGISTRY ===
common --registry=https://bcr.bazel.build/
common --registry=https://your-company-registry.com/modules/
```

### **Продвинутая (для enterprise):**
```bazelrc
# === MODULE REGISTRY ===
# Базовые registry для всех команд
common --registry=https://bcr.bazel.build/

# Приватный registry компании  
common --registry=https://internal-registry.company.com/

# Специфичные для окружения
common:ci --registry=https://ci-registry.company.com/
common:dev --registry=https://dev-registry.company.com/

# Fallback для критических модулей
common --registry=https://backup-registry.company.com/
```

## 📈 Результат оптимизации

**В вашем случае:**
- **Было:** 6 строк дублирования
- **Стало:** 2 строки с `common`
- **Экономия:** 67% меньше кода
- **Поддержка:** Одно место для изменений

**Теперь все команды автоматически используют настроенные registry!** 🎉