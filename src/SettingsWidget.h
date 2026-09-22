#pragma once

#include <QWidget>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;

/* Simple settings page shown as a tab inside OpenRGB.        */
/* Every change is saved immediately via the settingsChanged  */
/* signal. The "remember current lighting" button tells the   */
/* plugin to take a fresh snapshot.                           */
class SettingsWidget : public QWidget
{
    Q_OBJECT

public:
    struct Settings
    {
        QString target_key;
        bool    reapply_on_change      = true;
        bool    reapply_on_wake        = true;
        bool    activity_detect_enabled = true;
        int     idle_resume_sec        = 60;
        bool    log_enabled            = true;
    };

    explicit SettingsWidget(const Settings& settings, QWidget* parent = nullptr);

    Settings GetSettings() const;
    void     SetStatusText(const QString& text);

signals:
    void settingsChanged();
    void snapshotRequested();

private:
    void EmitSettingsChanged();

    QLineEdit*   target_edit_     = nullptr;
    QCheckBox*   change_check_    = nullptr;
    QCheckBox*   wake_check_      = nullptr;
    QCheckBox*   activity_check_  = nullptr;
    QSpinBox*    idle_spin_       = nullptr;
    QCheckBox*   log_check_       = nullptr;
    QPushButton* snapshot_button_ = nullptr;
    QLabel*      status_label_    = nullptr;
    QLabel*      hint_label_      = nullptr;
};