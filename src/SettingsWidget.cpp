#include "SettingsWidget.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

SettingsWidget::SettingsWidget(const Settings& settings, QWidget* parent)
    : QWidget(parent)
{
    wake_check_ = new QCheckBox("Переприменять после пробуждения компьютера", this);
    wake_check_->setChecked(settings.reapply_on_wake);

    activity_check_ = new QCheckBox("Возвращать при первом движении мыши (мгновенно)", this);
    activity_check_->setChecked(settings.activity_detect_enabled);
    activity_check_->setToolTip(
        "Плагин следит только за физическими событиями мыши и загружает профиль, "
        "когда после длительной паузы снова появляется движение.");

    idle_spin_ = new QSpinBox(this);
    idle_spin_->setRange(10, 600);
    idle_spin_->setSingleStep(5);
    idle_spin_->setSuffix(" сек");
    idle_spin_->setValue(settings.idle_resume_sec);
    idle_spin_->setToolTip(
        "Если мышь не трогают дольше этого времени, следующее физическое движение "
        "считается пробуждением.");
    idle_spin_->setEnabled(activity_check_->isChecked());

    connect(activity_check_, &QCheckBox::toggled, idle_spin_, &QWidget::setEnabled);

    log_check_ = new QCheckBox("Писать действия в журнал OpenRGB", this);
    log_check_->setChecked(settings.log_enabled);

    status_label_ = new QLabel("", this);
    status_label_->setWordWrap(true);

    hint_label_ = new QLabel(
        "Hardened-версия не обращается напрямую к RGB-контроллерам. "
        "Она загружает профиль, выбранный в OpenRGB Profile Manager. "
        "Сохраните нужный профиль и включите «Load Profile on Open»; "
        "для восстановления после сна также можно настроить профиль «on Resume».",
        this);
    hint_label_->setWordWrap(true);

    QFormLayout* form = new QFormLayout();
    form->addRow("", activity_check_);
    form->addRow("Пауза без движения, считающаяся «сном»:", idle_spin_);
    form->addRow("", wake_check_);
    form->addRow("", log_check_);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(status_label_);
    layout->addWidget(hint_label_);
    layout->addStretch();

    connect(activity_check_, &QCheckBox::toggled, this, &SettingsWidget::EmitSettingsChanged);
    connect(idle_spin_, qOverload<int>(&QSpinBox::valueChanged), this, &SettingsWidget::EmitSettingsChanged);
    connect(wake_check_, &QCheckBox::toggled, this, &SettingsWidget::EmitSettingsChanged);
    connect(log_check_, &QCheckBox::toggled, this, &SettingsWidget::EmitSettingsChanged);
}

SettingsWidget::Settings SettingsWidget::GetSettings() const
{
    Settings s;
    s.reapply_on_wake         = wake_check_->isChecked();
    s.activity_detect_enabled = activity_check_->isChecked();
    s.idle_resume_sec         = idle_spin_->value();
    s.log_enabled             = log_check_->isChecked();
    return s;
}

void SettingsWidget::SetStatusText(const QString& text)
{
    status_label_->setText(text);
}

void SettingsWidget::EmitSettingsChanged()
{
    emit settingsChanged();
}
