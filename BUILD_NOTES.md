# OpenRGB Wake Plugin — Hardened Build Notes

**Репозиторий:** https://github.com/Tyoman1/openrgb-wake-plugin-hardened  
**Версия:** 1.3.2-hardened.1  
**Основа:** https://github.com/Tyoman1/openrgb-wake-plugin (upstream v1.3.2)

## Описание

Security-hardened fork оригинального OpenRGB Wake Plugin.  
Восстанавливает подсветку устройств при пробуждении мыши или выходе ПК из сна.

## Ключевые изменения относительно upstream

- Убран небезопасный raw-controller fallback (`GetRGBControllers()`)
- JSON-типизация и try/catch при чтении/записи настроек
- `idle_resume_sec` ограничен 10–600 секундами
- Фильтрация `LLMHF_INJECTED | LLMHF_LOWER_IL_INJECTED`
- Безопасный lifecycle Windows mouse hook
- Исправленный `armOneShot()` через флаг
- Без двойной обработки Windows Resume
- MSVC hardening: CFG, ASLR, DEP, CET, HE-VA, SDL

## CI

GitHub Actions workflow `.github/workflows/build-hardened.yml` собирает DLL из `main`,  
генерирует `BUILD_REPORT.md`, SHA-256 и source ZIP.

Artefact `OpenRGBWakePlugin-1.3.2-hardened.1` содержит:
- `OpenRGBWakePlugin.dll`
- `OpenRGBWakePlugin.sha256.txt`
- `BUILD_REPORT.md`
- `OpenRGBWakePlugin-1.3.2-hardened.1-source.zip`

## Как собрать локально

```powershell
cmake -S . -B build -A x64 -DCMAKE_PREFIX_PATH=C:\Qt\6.8.3\msvc2022_64
cmake --build build --config Release
```

Требуется: Windows, MSVC 2022, Qt 6.8.3 `win64_msvc2022_64`.