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

bool MouseActivityWatcher::start()
{
    if (active_hook != nullptr)
    {
        return true;
    }

    last_event_ms_  = 0;
    active_instance = this;

    HHOOK hook = SetWindowsHookExW(WH_MOUSE_LL, LowLevelMouseProc, GetModuleHandleW(nullptr), 0);
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