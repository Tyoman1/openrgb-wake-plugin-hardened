#pragma once

#include <QAbstractNativeEventFilter>
#include <QObject>

#include <functional>

/* Watches for the PC resuming from sleep and fires on_resume.            */
/* On Windows this catches WM_POWERBROADCAST PBT_APMRESUME* messages.    */
class PowerWatcher : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT

public:
    explicit PowerWatcher(QObject* parent = nullptr);
    ~PowerWatcher() override;

    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

    std::function<void()> on_resume;
};