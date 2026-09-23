#pragma once

#include <QObject>

/* Watches global mouse activity to detect when the mouse "wakes up".
 * A Windows low-level hook notes the gap between mouse events; the next
 * event arriving after a long quiet gap means the user just started using
 * a mouse that was asleep, so mouseResumed() is emitted. */
class MouseActivityWatcher : public QObject
{
    Q_OBJECT

public:
    explicit MouseActivityWatcher(QObject* parent = nullptr);
    ~MouseActivityWatcher() override;

    /* Idle gap (in ms) after which the next mouse event counts as a wake. */
    void setIdleThresholdMs(quint64 ms);

    /* Make the very next mouse event count as a wake, exactly once.
       Used after plugin start / PC resume, when the mouse is likely
       asleep and the first touch means it just woke up. */
    void armOneShot();

    /* Install the OS-level mouse hook. Returns false on failure. */
    bool start();
    void stop();
    bool isActive() const;

signals:
    void mouseResumed();

private:
    /* OS low-level mouse hook callback (HHOOK proc). Declared with the
       Windows calling-convention types spelled out so that this header
       does not have to include <windows.h>. */
    static long long __stdcall LowLevelMouseProc(int nCode,
                                                 unsigned long long wParam,
                                                 long long lParam);

    void OnMouseEvent();

    static MouseActivityWatcher* active_instance;
    static void*                 active_hook;   /* HHOOK */

    quint64 last_event_ms_     = 0;
    quint64 idle_threshold_ms_ = 60000;
};