# OpenRGB Wake Plugin 1.3.2-hardened.1

Неофициальная hardened-версия исходного `Tyoman1/openrgb-wake-plugin` 1.3.2.

Репозиторий: https://github.com/Tyoman1/openrgb-wake-plugin-hardened

## Что уже включено

- Безопасное чтение JSON с проверкой типов и обработкой исключений.
- `idle_resume_sec` принудительно ограничен диапазоном 10–600 секунд.
- Искусственно инжектированные mouse events (`LLMHF_INJECTED`,
  `LLMHF_LOWER_IL_INJECTED`) игнорируются.
- Исправлен one-shot после старта/пробуждения Windows.
- Проверяется результат `UnhookWindowsHookEx`.
- DLL удерживает дополнительную module reference, пока Windows использует
  callback глобального mouse hook.
- Убрана двойная реакция на Windows resume.
- Полностью отключён raw-controller fallback через `RGBControllerInterface*`.
- Убрано ставшее ненужным поле выбора устройства.
- `nlohmann/json 3.11.3` закреплён SHA-256.
- Для MSVC включены `/guard:cf`, `/sdl`, `/CETCOMPAT`, ASLR, DEP/NX,
  High Entropy VA.
- Версия явно помечена `1.3.2-hardened.1`, чтобы не путать её с официальной
  1.3.2.

## Важное изменение поведения

Для этой сборки **обязательно нужен профиль OpenRGB**.

1. Настройте нужную подсветку.
2. Сохраните профиль.
3. В OpenRGB Profile Manager включите `Load Profile on Open`.
4. При необходимости настройте профиль для Resume.

Если профиль не настроен, hardened-сборка намеренно не пытается напрямую
обращаться к RGB-контроллерам.

## Как получить DLL

Этот архив содержит финальные изменённые исходники (overlay), но не содержит
скомпилированную DLL: в окружении, где он был подготовлен, нет Windows
MSVC + Qt 6.8.3 ABI, необходимого для совместимой сборки OpenRGB 1.0.

Возьмите исходный репозиторий и замените файлами из этого архива одноимённые
файлы. Каталог `vendor/OpenRGBPluginSDK` должен остаться из исходного
репозитория.

После этого можно запустить workflow:

`.github/workflows/build-hardened.yml`

Он создаст:

- `OpenRGBWakePlugin.dll`
- `OpenRGBWakePlugin.sha256.txt`

## Локальная сборка

Требуются Windows, MSVC 2022 и Qt 6.8.3 `win64_msvc2022_64`.

```powershell
cmake -S . -B build -A x64 "-DCMAKE_PREFIX_PATH=C:\Qt\6.8.3\msvc2022_64"
cmake --build build --config Release
```

Готовая DLL:

`build\Release\OpenRGBWakePlugin.dll`

## Статус проверки

Исходники сведены в одну финальную версию; предыдущие patch-файлы применять
к этим файлам уже не нужно.

Полная ABI-сборка и запуск внутри OpenRGB не выполнялись в текущем окружении,
так как здесь отсутствуют MSVC и Qt 6.8.3 для Windows.
