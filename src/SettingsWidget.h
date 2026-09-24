#pragma once
#include <QWidget>

class QCheckBox;
class QLabel;
class QSpinBox;

/* Settings page for the hardened build.
   Direct RGBController fallback is intentionally disabled, so there is no
   device-name selector: the plugin always restores through OpenRGB profiles. */
class SettingsWidget : public QWidget
{
    Q_OBJECT

public:
    struct Settings
    {
        bool reapply_on_wake         = true;
        bool activity_detect_enabled = true;
        int  idle_resume_sec         = 60;
        bool log_enabled             = true;
    };

    explicit SettingsWidget(const Settings& settings, QWidget* parent = nullptr);

    Settings GetSettings() const;
    void     SetStatusText(const QString& text);

signals:
    void settingsChanged();

private:
    void EmitSettingsChanged();

    QCheckBox* wake_check_     = nullptr;
    QCheckBox* activity_check_ = nullptr;
    QSpinBox*  idle_spin_      = nullptr;
    QCheckBox* log_check_      = nullptr;
    QLabel*    status_label_   = nullptr;
    QLabel*    hint_label_     = nullptr;
};
