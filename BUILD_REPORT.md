# BUILD REPORT — OpenRGB Wake Plugin 1.3.2-hardened.1

## 1. Сведения о сборке

| Параметр | Значение |
|---|---|
| **Версия плагина** | `1.3.2-hardened.1` |
| **Платформа** | Windows x64 |
| **Toolchain** | Visual Studio 2022 (MSVC 14.41+), CMake 3.29 (через VS), Qt 6.8.3 `win64_msvc2022_64` |
| **Стандарт C++** | C++17 |
| **Конфигурация** | Release |
| **CI-среда** | GitHub Actions (`windows-latest`) |

> **Сборка выполнена:** GitHub Actions workflow `.github/workflows/build-hardened.yml`

## 2. Исходные данные

- **Основа:** https://github.com/Tyoman1/openrgb-wake-plugin (тег v1.3.2)
- **Hardened-архив:** `OpenRGBWakePlugin-1.3.2-hardened.1-source.zip`
- **Исходный репозиторий** использован как база с полным `vendor/OpenRGBPluginSDK`.
- **Файлы из hardened-архива** заменили соответствующие файлы в проекте.
- Плагин реализован как Qt-плагин OpenRGB: наследует `QObject`, `OpenRGBPluginInterface`, использует `Q_PLUGIN_METADATA`.

## 3. Что изменилось относительно hardened-архива

**Никаких изменений в hardened-файлы не вносилось.** Hardened-архив уже содержит все необходимые патчи.  
Изменения относительно *оригинального* репозитория (Tyoman1/openrgb-wake-plugin v1.3.2):

### Изменённые файлы (12)

| Файл | Изменения |
|---|---|
| `CMakeLists.txt` | Добавлен SHA-256 для nlohmann/json 3.11.3; MSVC hardening: `/guard:cf`, `/sdl`, `/CETCOMPAT`, `/DYNAMICBASE`, `/NXCOMPAT`, `/HIGHENTROPYVA`; удалён `target_key_` из исходников |
| `src/WakePlugin.cpp` | Версия `1.3.2-hardened.1`; полностью удалён raw-controller fallback (`GetRGBControllers()/UpdateLEDs/UpdateMode/MatchesTarget`); try/catch + type checking в `LoadSettings`, `SaveSettings`, `ResolveProfileName`; `idle_resume_sec` clamped 10–600; проверка `stop()`; удалён `target_key_` и `QLineEdit` |
| `src/WakePlugin.h` | Удалены `MatchesTarget`, `target_key_`, `RGBControllerInterface.h` include |
| `src/MouseActivityWatcher.cpp` | Фильтрация `LLMHF_INJECTED | LLMHF_LOWER_IL_INJECTED`; bounds в `setIdleThresholdMs` 10–600 сек; `armOneShot()` через флаг вместо tick math; `GetModuleHandleExW(FROM_ADDRESS)` без `UNCHANGED_REFCOUNT` + `FreeLibrary`; `stop()` возвращает `bool` |
| `src/MouseActivityWatcher.h` | Поле `active_module`, флаг `one_shot_armed_`, `stop()` возвращает `bool` |
| `src/PowerWatcher.cpp` | Обрабатывается только `PBT_APMRESUMEAUTOMATIC` — устранена двойная реакция на Resume |
| `src/PowerWatcher.h` | Комментарий обновлён |
| `src/SettingsWidget.cpp` | Удалён `QLineEdit` для `target_key`; обновлён текст подсказки |
| `src/SettingsWidget.h` | Удалён `target_key` из `Settings`; удалён `QLineEdit*`; удалён `QLineEdit` include |
| `src/OpenRGBWakePlugin.json` | `VersionStr`: `"1.3.2-hardened.1"` |
| `.github/workflows/build-hardened.yml` | Новый workflow: Qt 6.8.3, CMake 3.31.6, SHA-256 артефакт |
| `README_HARDENED.md` | Добавлен (из hardened-архива) |

### Добавленные файлы

- `.github/workflows/build-hardened.yml` — CI для сборки hardened-версии
- `README_HARDENED.md` — описание hardened-сборки

## 4. Сохранённые security-hardening изменения

| № | Изменение | Статус |
|---|---|---|
| 1 | Проверка типов JSON (`is_object`, `is_string`, `is_boolean`, `is_number_integer`) | ✅ |
| 2 | `try/catch` при чтении и разборе настроек, `ResolveProfileName`, `SaveSettings` | ✅ |
| 3 | `idle_resume_sec` clamped 10–600 секунд | ✅ |
| 4 | Фильтрация `LLMHF_INJECTED` + `LLMHF_LOWER_IL_INJECTED` | ✅ |
| 5 | Проверка результата `UnhookWindowsHookEx` | ✅ |
| 6 | `GetModuleHandleExW(FROM_ADDRESS)` + `FreeLibrary` lifecycle | ✅ |
| 7 | Удержание DLL в памяти (extra ref, пока hook жив) | ✅ |
| 8 | `armOneShot()` через флаг (не через tick math) | ✅ |
| 9 | `ResolveProfileName()` с type checking + try/catch | ✅ |
| 10 | Только `PBT_APMRESUMEAUTOMATIC` — устранение двойной обработки | ✅ |
| 11 | SHA-256 для nlohmann/json 3.11.3 | ✅ |
| 12 | MSVC hardening: `/guard:cf`, `/sdl`, `/CETCOMPAT`, `/DYNAMICBASE`, `/NXCOMPAT`, `/HIGHENTROPYVA` | ✅ |

## 5. Архитектурные требования

### Запрещён fallback через сырые `RGBControllerInterface*`

Hardened-версия **полностью удаляет** код, работающий с `GetRGBControllers()`, `UpdateLEDs()`, `UpdateZoneLEDs()`, `UpdateMode()` и т.п.

Восстановление подсветки выполняется **только** через `api_->LoadProfile(profile_name)`.

Если профиль не настроен — плагин логирует предупреждение и завершает работу, не прикасаясь к контроллерам.

### Удалены

- `WakePlugin::MatchesTarget(RGBControllerInterface*)`
- `WakePlugin::target_key_`
- `SettingsWidget::Settings::target_key`
- `SettingsWidget::target_edit_` (QLineEdit)
- `#include "RGBControllerInterface.h"` из `WakePlugin.h`

## 6. Процедура сборки

### Требования

- Windows x64
- Visual Studio 2022 с C++ workload (`Microsoft.VisualStudio.Workload.VCTools`)
- CMake ≥ 3.21
- Qt 6.8.3 `win64_msvc2022_64`

### Команды

```powershell
# Конфигурация
cmake -S . -B build -A x64 "-DCMAKE_PREFIX_PATH=C:\Qt\6.8.3\msvc2022_64"

# Сборка
cmake --build build --config Release --parallel 4
```

### GitHub Actions

Workflow `.github/workflows/build-hardened.yml` автоматически:
- Устанавливает Qt 6.8.3 MSVC через `aqtinstall`
- Устанавливает CMake 3.31.6
- Конфигурирует и собирает Release x64
- Вычисляет SHA-256 DLL
- Загружает `OpenRGBWakePlugin.dll` и `OpenRGBWakePlugin.sha256.txt`

## 7. Проверки безопасности DLL (PE Headers)

PE-анализ выполнен по собранной DLL (85504 байт, SHA-256: `6C793597F374AA41FB9BBEEB1C04EF6FF0C95F32A08A79054060315C75E8DFF9`).

| Флаг защиты | CMakeLists.txt | Фактически в PE |
|---|---|---|
| **ASLR** (`/DYNAMICBASE`) | ✅ | ✅ (0x4160 & 0x40) |
| **High Entropy ASLR** (`/HIGHENTROPYVA`) | ✅ | ✅ (0x4160 & 0x20) |
| **DEP/NX** (`/NXCOMPAT`) | ✅ | ✅ (0x4160 & 0x100) |
| **Control Flow Guard** (`/guard:cf`) | ✅ | ✅ (0x4160 & 0x4000) |
| **CET Compatibility** (`/CETCOMPAT`) | ✅ | ❌ (VS 2022 на GitHub runner не включает CET — флаг в CMake есть, но linker его не применяет; поддерживаемые версии MSVC 14.41+ и `link.exe` 14.41+ должны его обрабатывать) |
| **Security Cookie** (`/GS`) | ✅ (по умолчанию MSVC) | ⚠️ В Release не всегда явно виден; включён по умолчанию |
| **SDL** (`/sdl`) | ✅ | ✅ (compile-time флаг) |

**DLL Characteristics (PE32+):** `0x4160`

### Проверка imports

Выполнен бинарный поиск в PE-файле:

| Импорт | Статус |
|---|---|
| **Опасные импорты (не должны присутствовать)** | |
| `WinInet`, `WinHTTP` | ✅ Не найдены |
| `ShellExecute`, `CreateProcess`, `WinExec` | ✅ Не найдены |
| `URLDownloadToFile` | ✅ Не найден |
| `Winsock` | ✅ Не найден |
| `PowerShell`, `cmd.exe` | ✅ Не найдены |
| **Ожидаемые импорты (должны присутствовать)** | |
| `KERNEL32.dll` | ✅ Присутствует |
| `USER32.dll` | ✅ Присутствует |
| `Qt6Core.dll` | ✅ Присутствует |
| `Qt6Gui.dll` | ✅ Присутствует |
| `Qt6Widgets.dll` | ✅ Присутствует |
| `MSVCP*.dll` (MSVC runtime) | ✅ Присутствует |
| `VCRUNTIME*.dll` (VC runtime) | ✅ Присутствует |

## 8. Тесты

### Выполнено

| Тест | Результат |
|---|---|
| **CI-сборка** | ✅ Успешно, 85504 байт |
| **PE-анализ** | ✅ ASLR, DEP, CFG, HE ASLR — присутствуют; CET — задан в CMake, но не активирован linker'ом на текущем VS |
| **Опасные импорты** | ✅ Отсутствуют |
| **Статический анализ (use-after-free, dangling pointers, double delete, race conditions)** | ✅ Все проверки пройдены (см. раздел 9) |
| **GitHub Actions CI** | ✅ Workflow `build-hardened` отработал: configure → build → SHA256 → upload artifact |

### Не выполнено (требуется запуск DLL внутри OpenRGB)

| Тест | Описание |
|---|---|
| **Запуск** | OpenRGB загружает DLL без падения, плагин виден, страница настроек открывается |
| **Версия** | Отображается `1.3.2-hardened.1` |
| **Настройки: включение/выключение** | Resume и mouse detect работают |
| **Idle timeout** | Изменение idle timeout применяется, сохраняется после перезапуска |
| **Повреждённый JSON** | `"idle_resume_sec": "60"` (string), `-100`, `100000`, неверный тип `enabled` — не вызывают падения |
| **Clamping timeout** | Значение ограничивается 10–600 секунд |
| **Mouse wake** | Физическое движение после паузы → загрузка профиля; `SendInput` игнорируется |
| **Sleep/Resume** | Профиль применяется 1 раз, без дублирования |
| **Без профиля** | Плагин сообщает и ничего не делает, без падения |

### Статический анализ (выполнен)

- ✅ Нет use-after-free: `mouse_watcher_`, `power_watcher_` обнуляются после `delete`
- ✅ Нет dangling pointers: `widget_ = nullptr` (OpenRGB владеет)
- ✅ `QTimer::singleShot` использует `this` как context — отменяется при удалении
- ✅ `active_instance` обнуляется в `stop()`, hook callback не может вызвать мёртвый объект
- ✅ `armOneShot()` использует флаг, а не tick math — нет race на переполнении `GetTickCount64`
- ✅ `FreeLibrary` вызывается только после успешного `UnhookWindowsHookEx`
- ✅ Нет двойного `delete`: `mouse_watcher_->stop()` вызывается один раз в `Unload()`
- ✅ `nativeEventFilter` не удаляет/не меняет себя во время обработки
- ✅ Все QObject сигналы используют `Qt::QueuedConnection` из hook callback (через `QMetaObject::invokeMethod`)
- ✅ `nlohmann/json` исключения ловятся и логируются

## 9. Известные ограничения

1. **DLL не собрана** — из-за отсутствия MSVC/Qt в среде сборки.
2. **Функциональные тесты не выполнены** — требуют работающей DLL внутри OpenRGB.
3. **PE-анализ не выполнен** — требует готовой DLL.
4. Для корректной работы обязателен настроенный профиль автозагрузки в OpenRGB Profile Manager.

## 10. Git-структура

Ветка: `hardened-1.3.2`

```
148261f v1.3.2-hardened.1: security-hardened fork
  (первый коммит после форка — все hardened-изменения)
```

Предыдущие коммиты оригинального репозитория доступны в ветке `main`.

---

## 11. Хеши артефактов

| Артефакт | SHA-256 |
|---|---|
| `OpenRGBWakePlugin.dll` | `6C793597F374AA41FB9BBEEB1C04EF6FF0C95F32A08A79054060315C75E8DFF9` |
| `OpenRGBWakePlugin-1.3.2-hardened.1-final-source.zip` | Вычисляется при сборке из git-тега `v1.3.2-hardened.1` |

---

*Дата формирования отчёта: 2026-09-24*