# OpenRGB Wake-Plugin: "re-apply lighting when the wireless mouse wakes"

Fix the Razer DeathAdder V2 Pro losing its lighting after wireless sleep,
without Razer Synapse.

## Background (verified on this machine, 2026-09-22)

- Mouse: **Razer DeathAdder V2 Pro** (VID_1532 / PID_007D), wireless via USB dongle.
- Keyboard: **Razer Cynosa V2** (VID_1532 / PID_025E), wired.
- OpenRGB: **1.0 stable** installed at `C:\Program Files\OpenRGB`
  (exe dated 2026-09-11, day of release). Qt **6.8.3 (MSVC)** DLLs bundled.
- Runs as Windows service `OpenRGB` **and** a second GUI instance
  `OpenRGB.exe --startminimized` is running at the same time
  (PID 19076 in user session + service process).
  → Symptom factory: two instances opening the same HID devices.
  → Must leave ONE control path before relying on plugin behavior.

### Root cause of the wireless-mouse problem

1. OpenRGB holds lighting state in RAM and only re-applies on device
   (re)detection. 1.0 added USB-HID hotplugging + auto-apply of profile on
   detection (feature request [#3842], closed by CalcProgrammer1 2026-07-10).
2. When the mouse sleeps, its dongle may **not** drop from the USB tree, so no
   hotplug event fires and 1.0 never re-applies. Synapse works because Razer's
   own driver gets a wake notification from the device itself. This is the
   exact open issue [#3187].
3. `tools/watch-mouse-sleep.ps1` tells us whether the device actually drops
   from Windows on sleep (case A) or stays enumerated (case B).
   That decides the plugin's detection trigger.

**Measured 2026-09-22:** during ~5 min of idle + wake the mouse stayed
enumerated the whole time (no `ABSENT` entries in `tools/mouse-presence.log`).
→ case B: the plugin is designed around periodic re-apply (poll), with
device-change and PC-wake triggers as fast paths.

## Plugin design (OpenRGB Plugin API v5, in-process, Qt 6)

What the plugin has available (from `OpenRGBPluginInterface.h`,
`ResourceManagerCallback.h`, `RGBControllerInterface.h`):

| Need | API |
|---|---|
| Know device list changed | `ResourceManagerUpdated(reason)` — `DETECTION_STARTED/COMPLETE`, `DEVICE_LIST_UPDATED` |
| Get controllers | `GetRGBControllers()` → `RGBControllerInterface*` |
| Identify the mouse | `GetName()`, `GetDescription()`, `GetLocation()`, `GetSerial()` |
| Re-apply state directly | `SetActiveMode()`, `SetCustomMode()`, `SetAllColors()`, `UpdateMode()`, `UpdateLEDs()` |
| Or load a profile | `LoadProfile(name)` / `GetProfileList()` |
| Force a rescan | `RescanDevices()` |
| Timer poll (Qt) | own `QTimer` inside plugin widget/object |

### Detection layers (implemented, first one that fires wins)

1. **Poll** (configurable interval, default 20 s) — primary layer for case B:
   re-apply the saved snapshot to the matched device. Trade-off: for animated
   modes the animation restarts on each tick; lower the frequency or disable
   the layer to taste.
2. **Device-list hook** — on `DEVICE_LIST_UPDATED`/`DETECTION_COMPLETE`,
   re-apply after a short delay (handles case A and PC boot / USB re-enumeration).
3. **Power hook** — native Windows power events (WM_POWERBROADCAST resume) via
   a `QAbstractNativeEventFilter` installed from the plugin → re-apply after
   resume.

Re-apply = iterate matched controllers, restore the saved mode/colors snapshot
(global modes + per-zone modes + per-LED colors), call `UpdateMode()` /
`UpdateLEDs()` / `UpdateZoneMode()` / `UpdateZoneLEDs()`.

Snapshot is taken at plugin load, after profile load, and manually via the
"Запомнить текущую подсветку" button in the plugin's settings tab. Settings
live in OpenRGB's settings manager under the `OpenRGBWakePlugin` key.

## Toolchain decision: CI build (GitHub Actions)

Installed OpenRGB is Qt 6.8.3 MSVC build → a plugin DLL must match that ABI:
**MSVC + Qt 6.8.x (win64_msvc2022_64)**. The machine has no Qt / MSVC / cmake /
vcpkg, so the plugin is built in the cloud:

`.github/workflows/build.yml` — on every push to `main`:
`windows-latest` + Python 3.12 (pinned) + Qt 6.8.3 (via `aqtinstall`,
`win64_msvc2022_64` — the only MSVC arch Qt 6.8 ships) + CMake (auto-detects
the installed Visual Studio generator) → produces
`build/Release/OpenRGBWakePlugin.dll` → uploaded as a GitHub Actions artifact.

Local structure:

- `src/` — plugin implementation (WakePlugin, SettingsWidget, PowerWatcher)
- `vendor/OpenRGBPluginSDK/` — plugin API v5 headers pinned from OpenRGB master
- `tools/watch-mouse-sleep.ps1` — device presence logger

## Steps

- [x] Research plugin API v5 + OpenRGB 1.0 behavior (issues #3842, #3187)
- [x] Run `tools/watch-mouse-sleep.ps1`, read log → case B (device stays)
- [x] Scaffold plugin (Qt 6.8.3 + plugin API v5 headers), push to GitHub
- [x] Implement detection + re-apply, settings tab
- [ ] CI build green → download DLL from Actions artifact
- [ ] Install into OpenRGB plugins dir, restart, test sleep/wake

[#3842]: https://gitlab.com/CalcProgrammer1/OpenRGB/-/issues/3842
[#3187]: https://gitlab.com/CalcProgrammer1/OpenRGB/-/issues/3187