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

On every trigger the plugin **asks OpenRGB to load the profile the user
configured in OpenRGB's own Profile Manager settings** (`LoadProfile`). The
plugin stores no colors and no profile names: the profile name is read live
from the `ProfileManager` settings key (trigger-appropriate entry first:
`resume_profile` for PC resume, otherwise `open_profile`, then
`service_startup_profile` as fallback). If no auto-load profile is configured,
the plugin falls back to re-sending the currently active lighting of the
matched target device.

### Why echoing the live state was not enough (root cause, 2026-09-23)

OpenRGB matches profile entries to controllers strictly — including **serial
number and version**. A wireless mouse that is *asleep* at detection time is
registered with an **empty serial / v0.0**, so a profile entry saved while the
mouse was awake (real serial) does not match it. The profile had silently
accumulated a **duplicate mouse entry** (empty serial, dark colors); after boot
OpenRGB faithfully applied that dark entry, and any plugin echoing live state
just re-sent darkness. Loading the profile by name sidesteps the stale live
state, and keeping both duplicate entries' colors correct (or removing the
stale one) fixes matching at boot.

### One-time setup in OpenRGB (not in the plugin)

1. Set your lighting (mouse and anything else) the way you want.
2. Save it as a profile in OpenRGB.
3. OpenRGB settings → Profile Manager → enable **“Load Profile on Open”** (and
   “on Resume” if you want resume coverage) and pick that profile.

The plugin follows those settings — no profile names are entered anywhere in
the plugin itself.

### Triggers (first one that fires wins; all configurable)

1. **Fresh session start** — 2 s after the plugin loads, load the configured
   profile. If nothing was applied yet, retry every 1 s, up to 5 times. This
   is a safety net on top of OpenRGB's own profile load.
2. **Mouse activity** — a low-level `WH_MOUSE_LL` hook watches global mouse
   events. When an event arrives after an idle gap `>= "пауза, считающаяся
   сном"` (default 60 s, configurable 10–600 s), load the profile twice
   (700 ms and ~2.6 s; `LoadProfile` reports success even if the device was
   mid-wake and missed the write, so the retry is unconditional). After plugin
   start and after a PC resume the watcher is additionally **armed one-shot**:
   the very first mouse movement then counts as a wake regardless of the idle
   gap — this covers the case where the startup apply went out while the mouse
   was still asleep.
3. **PC resume** — native Windows power events (`WM_POWERBROADCAST` resume) via
   a `QAbstractNativeEventFilter`; load the profile 2.5 s after resume.

Profile load = `LoadProfile(<name from OpenRGB ProfileManager settings>)` —
OpenRGB applies the saved state to every matched device. Fallback (no profile
configured): for matched controllers (default target `DeathAdder`) re-send the
current state via `UpdateMode()` / `UpdateLEDs()` (or the per-zone variants).

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
v1.2.0. Since v1.3.0 the plugin loads the configured OpenRGB profile on its
triggers instead of only echoing the live controller state.

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
- [x] Diagnose boot-dark mouse: profile matched a stale empty-serial duplicate
      entry (dark) instead of the real one; v1.3.0: plugin loads the configured
      profile itself on every trigger; v1.3.1: unconditional wake retry +
      one-shot "first touch after start/resume" arming (a successful profile
      load can still miss a sleeping mouse)
- [ ] CI build green → download DLL from Actions artifact
- [ ] Install into OpenRGB plugins dir
- [ ] One-time setup: save profile + enable “Load Profile on Open” in OpenRGB
- [ ] Acceptance (eyes only): reboot → after ~15 s the mouse is lit → reboot
      again and check once more

[#3842]: https://gitlab.com/CalcProgrammer1/OpenRGB/-/issues/3842
[#3187]: https://gitlab.com/CalcProgrammer1/OpenRGB/-/issues/3187