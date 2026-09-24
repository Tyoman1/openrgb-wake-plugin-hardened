#pragma once
#include <QObject>

/* Watches global mouse activity to detect when the mouse "wakes up".
 * A Windows low-level hook notes the gap between real mouse events; the next
 * event arriving after a long quiet gap emits mouseResumed(). */
class MouseActivityWatcher : public QObject
{
    Q_OBJECT

public:
    explicit MouseActivityWatcher(QObject* parent = nullptr);
    ~MouseActivityWatcher() override;

    /* Idle gap (in ms) after which the next mouse event counts as a wake.
       Hardened implementation clamps this to 10..600 seconds. */
    void setIdleThresholdMs(quint64 ms);

    /* Make the very next real mouse event count as a wake, exactly once. */
    void armOneShot();

    /* Install/remove the OS-level mouse hook. */
    bool start();
    bool stop();
    bool isActive() const;

signals:
    void mouseResumed();

private:
    static long long __stdcall LowLevelMouseProc(int nCode,
                                                 unsigned long long wParam,
                                                 long long lParam);

    void OnMouseEvent();

    static MouseActivityWatcher* active_instance;
    static void*                 active_hook;    /* HHOOK */
    static void*                 active_module;  /* HMODULE, extra DLL ref while hook lives */

    quint64 last_event_ms_     = 0;
    quint64 idle_threshold_ms_ = 60000;
    bool    one_shot_armed_    = false;
};
