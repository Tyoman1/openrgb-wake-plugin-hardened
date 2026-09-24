#include "MouseActivityWatcher.h"

#include <algorithm>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <QMetaObject>

namespace
{
constexpr quint64 kMinIdleThresholdMs = 10ULL * 1000ULL;
constexpr quint64 kMaxIdleThresholdMs = 600ULL * 1000ULL;
}

MouseActivityWatcher* MouseActivityWatcher::active_instance = nullptr;
void*                 MouseActivityWatcher::active_hook     = nullptr;
void*                 MouseActivityWatcher::active_module   = nullptr;

LRESULT CALLBACK MouseActivityWatcher::LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if ((nCode == HC_ACTION) && (active_instance != nullptr) && (lParam != 0))
    {
        const auto* mouse_info = reinterpret_cast<const MSLLHOOKSTRUCT*>(lParam);

        /* Do not let SendInput/mouse_event or lower-integrity injected input
           impersonate a physical mouse wake. */
        constexpr DWORD kInjectedFlags = LLMHF_INJECTED | LLMHF_LOWER_IL_INJECTED;
        if ((mouse_info->flags & kInjectedFlags) == 0)
        {
            active_instance->OnMouseEvent();
        }
    }

    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

MouseActivityWatcher::MouseActivityWatcher(QObject* parent)
    : QObject(parent)
{
}

MouseActivityWatcher::~MouseActivityWatcher()
{
    (void)stop();
}

void MouseActivityWatcher::setIdleThresholdMs(quint64 ms)
{
    idle_threshold_ms_ = std::clamp(ms, kMinIdleThresholdMs, kMaxIdleThresholdMs);
}

void MouseActivityWatcher::armOneShot()
{
    one_shot_armed_ = true;
}

bool MouseActivityWatcher::start()
{
    if (active_hook != nullptr)
    {
        /* Never silently reuse a stale hook owned by another watcher. */
        return active_instance == this;
    }

    last_event_ms_  = 0;
    one_shot_armed_ = false;
    active_instance = this;

    /* Hold an extra DLL reference for as long as Windows owns a callback
       address inside this module. */
    HMODULE self_module = nullptr;
    if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
            reinterpret_cast<LPCWSTR>(&LowLevelMouseProc),
            &self_module)
        || self_module == nullptr)
    {
        active_instance = nullptr;
        return false;
    }

    HHOOK hook = SetWindowsHookExW(WH_MOUSE_LL, LowLevelMouseProc, self_module, 0);
    if (hook == nullptr)
    {
        FreeLibrary(self_module);
        active_instance = nullptr;
        return false;
    }

    active_module = self_module;
    active_hook   = hook;
    return true;
}

bool MouseActivityWatcher::stop()
{
    bool removed = true;

    if (active_hook != nullptr)
    {
        if (UnhookWindowsHookEx(static_cast<HHOOK>(active_hook)))
        {
            active_hook = nullptr;

            if (active_module != nullptr)
            {
                FreeLibrary(static_cast<HMODULE>(active_module));
                active_module = nullptr;
            }
        }
        else
        {
            /* Keep the extra DLL reference and hook handle. The callback code
               stays mapped, but it is detached from this QObject below. */
            removed = false;
        }
    }

    if (active_instance == this)
    {
        active_instance = nullptr;
    }

    last_event_ms_  = 0;
    one_shot_armed_ = false;
    return removed;
}

bool MouseActivityWatcher::isActive() const
{
    return (active_hook != nullptr) && (active_instance == this);
}

void MouseActivityWatcher::OnMouseEvent()
{
    const quint64 now = static_cast<quint64>(GetTickCount64());

    const bool woke_up = one_shot_armed_ ||
                         ((last_event_ms_ != 0) &&
                          (now - last_event_ms_) >= idle_threshold_ms_);

    one_shot_armed_ = false;
    last_event_ms_  = now;

    if (woke_up)
    {
        QMetaObject::invokeMethod(this, "mouseResumed", Qt::QueuedConnection);
    }
}
