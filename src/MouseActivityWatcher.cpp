#include "MouseActivityWatcher.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <QMetaObject>

MouseActivityWatcher* MouseActivityWatcher::active_instance = nullptr;
void*                 MouseActivityWatcher::active_hook     = nullptr;

LRESULT CALLBACK MouseActivityWatcher::LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if ((nCode == HC_ACTION) && (active_instance != nullptr))
    {
        /* Minimal work: just record the event and maybe schedule a signal. */
        active_instance->OnMouseEvent();
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

MouseActivityWatcher::MouseActivityWatcher(QObject* parent)
    : QObject(parent)
{
}

MouseActivityWatcher::~MouseActivityWatcher()
{
    stop();
}

void MouseActivityWatcher::setIdleThresholdMs(quint64 ms)
{
    idle_threshold_ms_ = ms;
}

void MouseActivityWatcher::armOneShot()
{
    /* Pretend the last event happened exactly one idle gap ago: the next
       event then satisfies the wake condition once, and normal gap logic
       resumes afterwards. */
    quint64 now = static_cast<quint64>(GetTickCount64());

    last_event_ms_ = (now > idle_threshold_ms_) ? (now - idle_threshold_ms_) : 1;
}

bool MouseActivityWatcher::start()
{
    if (active_hook != nullptr)
    {
        return true;
    }

    last_event_ms_  = 0;
    active_instance = this;

    /* The hook procedure lives in this DLL: hand Windows this module's own
       handle. GetModuleHandle(NULL) would return the host EXE's handle, and
       a global low-level hook whose procedure is not inside the given module
       installs successfully but is never called. */
    HMODULE self_module = nullptr;

    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&LowLevelMouseProc),
                       &self_module);

    HHOOK hook = SetWindowsHookExW(WH_MOUSE_LL, LowLevelMouseProc, self_module, 0);
    if (hook == nullptr)
    {
        active_instance = nullptr;
        return false;
    }

    active_hook = hook;
    return true;
}

void MouseActivityWatcher::stop()
{
    if (active_hook != nullptr)
    {
        UnhookWindowsHookEx(static_cast<HHOOK>(active_hook));
        active_hook = nullptr;
    }

    if (active_instance == this)
    {
        active_instance = nullptr;
    }

    last_event_ms_ = 0;
}

bool MouseActivityWatcher::isActive() const
{
    return active_hook != nullptr;
}

void MouseActivityWatcher::OnMouseEvent()
{
    /* Runs on the GUI thread during windows message dispatch; keep this cheap. */

    quint64 now = static_cast<quint64>(GetTickCount64());

    bool woke_up = (last_event_ms_ != 0) &&
                   (now - last_event_ms_) >= idle_threshold_ms_;

    last_event_ms_ = now;

    if (woke_up)
    {
        /* Defer to the event loop instead of emitting from inside the hook. */
        QMetaObject::invokeMethod(this, "mouseResumed", Qt::QueuedConnection);
    }
}