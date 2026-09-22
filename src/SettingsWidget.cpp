#include "SettingsWidget.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>

SettingsWidget::SettingsWidget(const Settings& settings, QWidget* parent)
    : QWidget(parent)
{
    target_edit_ = new QLineEdit(settings.target_key, this);
    target_edit_->setToolTip("Часть названия устройства, к которому применяется плагин");

    wake_check_ = new QCheckBox("Переприменять после пробуждения компьютера", this);
    wake_check_->setChecked(settings.reapply_on_wake);

    activity_check_ = new QCheckBox("Возвращать при первом движении мыши (мгновенно)", this);
    activity_check_->setChecked(settings.activity_detect_enabled);
    activity_check_->setToolTip("Плагин следит за движениями мыши и возвращает подсветку сразу, "
                                "как только после длительной паузы снова наступает движение — "
                                "значит, мышь только что проснулась");

    idle_spin_ = new QSpinBox(this);
    idle_spin_->setRange(10, 600);
    idle_spin_->setSingleStep(5);
    idle_spin_->setSuffix(" сек");
    idle_spin_->setValue(settings.idle_resume_sec);
    idle_spin_->setToolTip("Если мышь не трогают дольше этого времени — считаем, что она уснула. "
                           "Первое движение после такой паузы мгновенно вернёт подсветку");
    idle_spin_->setEnabled(activity_check_->isChecked());
    connect(activity_check_, &QCheckBox::toggled, idle_spin_, &QWidget::setEnabled);

    log_check_ = new QCheckBox("Писать действия в журнал OpenRGB", this);
    log_check_->setChecked(settings.log_enabled);

    status_label_ = new QLabel("", this);
    status_label_->setWordWrap(true);

    QLabel* hint_label = new QLabel(
        "Плагин не хранит цвета. Он берёт действующую подсветку из OpenRGB и повторно "
        "отправляет её в мышь при запуске, при первом движении мыши после сна и после "
        "пробуждения компьютера. Чтобы свет появлялся при входе, сохраните профиль в "
        "OpenRGB и включите «Load Profile on Open».", this);
    hint_label->setWordWrap(true);

    QFormLayout* form = new QFormLayout();
    form->addRow("Устройство (часть названия):", target_edit_);
    form->addRow("", activity_check_);
    form->addRow("Пауза без движения, считающаяся «сном»:", idle_spin_);
    form->addRow("", wake_check_);
    form->addRow("", log_check_);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(status_label_);
    layout->addWidget(hint_label);
    layout->addStretch();

    connect(target_edit_, &QLineEdit::textChanged, this, &SettingsWidget::EmitSettingsChanged);
    connect(activity_check_, &QCheckBox::toggled, this, &SettingsWidget::EmitSettingsChanged);
    connect(idle_spin_, qOverload<int>(&QSpinBox::valueChanged), this, &SettingsWidget::EmitSettingsChanged);
    connect(wake_check_, &QCheckBox::toggled, this, &SettingsWidget::EmitSettingsChanged);
    connect(log_check_, &QCheckBox::toggled, this, &SettingsWidget::EmitSettingsChanged);
}

SettingsWidget::Settings SettingsWidget::GetSettings() const
{
    Settings s;

    s.target_key               = target_edit_->text();
    s.reapply_on_wake          = wake_check_->isChecked();
    s.activity_detect_enabled  = activity_check_->isChecked();
    s.idle_resume_sec          = idle_spin_->value();
    s.log_enabled              = log_check_->isChecked();

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