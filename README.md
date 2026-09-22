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
3. `tools/watch-mouse-sleep.ps1` will tell us whether the device actually drops
   from Windows on sleep (case A) or stays enumerated (case B).
   That decides the plugin's detection trigger.

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

### Detection layers (all three, whichever fires first)

1. **Device-list hook** — on `DEVICE_LIST_UPDATED`/`DETECTION_COMPLETE`, diff
   controllers, and if the target mouse appeared → re-apply after a short delay
   (handles case A and PC boot).
2. **Power hook** — native Windows power events (WM_POWERBROADCAST resume) via
   a `QAbstractNativeEventFilter` installed from the plugin → re-apply after
   resume.
3. **Poll** (configurable interval) — for case B: re-check the stored
   "last applied" color/mode and re-send when the device is believed to have
   been asleep (interval + hysteresis configurable; a "force re-apply" toggle
   as last resort).

Re-apply = iterate matched controllers, restore the last applied
mode/colors (snapshot kept from the last successful apply), call
`UpdateMode()` / `UpdateLEDs()`.

## Toolchain decision (still open)

Installed OpenRGB is Qt 6.8.3 MSVC build → a plugin DLL must match that ABI:
**MSVC + Qt 6.8.x (msvc2019_64)**. Current machine has no Qt / MSVC / cmake /
vcpkg, so options are:

- **A. Local toolchain**: VS Build Tools (MSVC) + Qt 6.8.3 via aqtinstall +
  cmake. One-time ~3–6 GB install. Fastest iteration.
- **B. CI build**: push plugin source to GitLab/GitHub, pipeline produces the
  Windows plugin zip (no local install). Slower iteration, zero install.

## Steps

- [ ] Fix duplicate instance (service vs `--startminimized` GUI)
- [ ] Run `tools/watch-mouse-sleep.ps1`, let mouse sleep, read log → decide A/B
- [ ] Decide toolchain route (local vs CI)
- [ ] Scaffold plugin (Qt 6.8.3 + plugin API v5 headers)
- [ ] Implement detection + re-apply, settings tab
- [ ] Build, install into OpenRGB (`%APPDATA%`/OpenRGB plugins dir), test sleep/wake

[#3842]: https://gitlab.com/CalcProgrammer1/OpenRGB/-/issues/3842
[#3187]: https://gitlab.com/CalcProgrammer1/OpenRGB/-/issues/3187