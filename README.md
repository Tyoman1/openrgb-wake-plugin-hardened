# OpenRGB Wake-Plugin: "re-apply active lighting after boot, wireless wake, or PC resume"

Restore the Razer DeathAdder V2 Pro's lighting when the PC boots, when the
wireless mouse wakes from sleep, or when the PC resumes — without Razer Synapse.

## Background (verified on this machine, 2026-09-22)

- Mouse: **Razer DeathAdder V2 Pro** (VID_1532 / PID_007D), wireless via USB dongle.
- Keyboard: **Razer Cynosa V2** (VID_1532 / PID_025E), wired.
- OpenRGB: **1.0 stable** installed at `C:\Program Files\OpenRGB`
  (exe dated 2026-09-11, day of release). Qt **6.8.3 (MSVC)** DLLs bundled.
- A Windows service `OpenRGB` **and** a GUI instance
  `OpenRGB.exe --startminimized` may run at the same time. The plugin only
  loads in the GUI instance (the service runs as a server without plugins —
  `startup/main_Windows.cpp`: service mode sets `RET_FLAG_START_SERVER` and has
  its own `service_config\` directory).

### Symptoms

**Boot.** After the PC is turned off and on, the mouse does not light up by
itself; even moving the mouse does not bring the light back. Expected: light
after login, once OpenRGB is running.

**Wireless sleep.** The mouse loses its lighting after wireless sleep. OpenRGB
holds lighting state in RAM and only re-applies on device (re)detection; when
the mouse sleeps, its dongle may **not** drop from the USB tree, so no hotplug
event fires and OpenRGB never re-applies (this is the exact open issue
[#3187]).

**Measured 2026-09-22:** during ~5 min of idle + wake the mouse stayed
enumerated the whole time (no `ABSENT` entries in `tools/mouse-presence.log`).
→ the plugin detects the **first mouse move after idle** (the device wakes and
the user touches it again).

## How it works

The plugin **does not store colors anywhere**. It reads the *currently active*
lighting from the target device's controller (what OpenRGB holds in RAM — i.e.
the active profile, once one is loaded) and **re-sends it down to the
hardware**. OpenRGB itself is responsible for loading the profile.

### One-time setup in OpenRGB (not in the plugin)

1. Set your lighting (mouse and anything else) the way you want.
2. Save it as a profile in OpenRGB.
3. OpenRGB settings → Profile Manager → enable **“Load Profile on Open”** and
   pick that profile.

After this, OpenRGB applies the profile itself at login, so the mouse lights
up on boot. The plugin only duplicates that when OpenRGB misses a moment.

### Triggers (first one that fires wins; all configurable)

1. **Fresh session start** — 2 s after the plugin loads, re-send the active
   lighting to the mouse. If the mouse has not been detected yet, retry every
   1 s, up to 5 times. This is a safety net on top of OpenRGB's own profile
   load.
2. **Mouse activity** — a low-level `WH_MOUSE_LL` hook watches global mouse
   events. When an event arrives after an idle gap `>= "пауза, считающаяся
   сном"` (default 60 s, configurable 10–600 s), re-send immediately (700 ms),
   with one retry ~2.6 s later if the first attempt failed. No periodic timer.
3. **PC resume** — native Windows power events (`WM_POWERBROADCAST` resume) via
   a `QAbstractNativeEventFilter`; re-send 2.5 s after resume.

Re-send = for matched controllers (default target `DeathAdder`): for
global-mode devices call `UpdateMode()` / `UpdateLEDs()`; for per-zone devices
call `UpdateZoneMode()` / `UpdateZoneLEDs()` per zone. No values are changed —
the controller already holds the active lighting.

Device-list changes are **ignored**: OpenRGB applies the active profile to
newly detected devices itself (feature request [#3842], closed by
CalcProgrammer1 2026-07-10).

### Settings page (plugin tab)

- **Устройство (часть названия)** — substring matched against device
  name/description/location (default `DeathAdder`).
- **Возвращать при первом движении мыши** + **пауза «сна»** (10–600 s).
- **Переприменять после пробуждения компьютера**.
- **Писать действия в журнал OpenRGB**.

The “remember current lighting” button and all snapshot logic were removed in
v1.2.0.

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

- `src/` — plugin implementation (WakePlugin, SettingsWidget, PowerWatcher,
  MouseActivityWatcher)
- `vendor/OpenRGBPluginSDK/` — plugin API v5 headers pinned from OpenRGB master
- `tools/watch-mouse-sleep.ps1` — device presence logger

## Steps

- [x] Research plugin API v5 + OpenRGB 1.0 behavior (issues #3842, #3187)
- [x] Run `tools/watch-mouse-sleep.ps1`, read log → case B (device stays)
- [x] Scaffold plugin (Qt 6.8.3 + plugin API v5 headers), push to GitHub
- [x] Implement detection + re-apply, settings tab
- [x] Implement v1.2.0: drop snapshot, re-send active lighting, startup timer
- [ ] CI build green → download DLL from Actions artifact
- [ ] Install into OpenRGB plugins dir
- [ ] One-time setup: save profile + enable “Load Profile on Open” in OpenRGB
- [ ] Acceptance (eyes only): reboot → after ~15 s the mouse is lit → reboot
      again and check once more

[#3842]: https://gitlab.com/CalcProgrammer1/OpenRGB/-/issues/3842
[#3187]: https://gitlab.com/CalcProgrammer1/OpenRGB/-/issues/3187