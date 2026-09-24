#include "PowerWatcher.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <QCoreApplication>

PowerWatcher::PowerWatcher(QObject* parent)
    : QObject(parent)
{
    QCoreApplication* app = QCoreApplication::instance();
    if (app)
    {
        app->installNativeEventFilter(this);
    }
}

PowerWatcher::~PowerWatcher()
{
    QCoreApplication* app = QCoreApplication::instance();
    if (app)
    {
        app->removeNativeEventFilter(this);
    }
}

bool PowerWatcher::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result)
{
#if defined(_WIN32)
    (void)eventType;
    (void)result;

    MSG* msg = static_cast<MSG*>(message);
    if (msg && (msg->message == WM_POWERBROADCAST))
    {
        /* Windows sends PBT_APMRESUMEAUTOMATIC for every normal resume.
           A user-driven wake may then also receive PBT_APMRESUMESUSPEND;
           handling both would execute the restore path twice. */
        if (msg->wParam == PBT_APMRESUMEAUTOMATIC)
        {
            if (on_resume)
            {
                on_resume();
            }
        }
    }
#else
    (void)eventType;
    (void)message;
    (void)result;
#endif

    return false;
}
