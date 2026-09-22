#include "SettingsWidget.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

SettingsWidget::SettingsWidget(const Settings& settings, QWidget* parent)
    : QWidget(parent)
{
    target_edit_ = new QLineEdit(settings.target_key, this);
    target_edit_->setToolTip("Часть названия устройства, к которому применяется плагин");

    change_check_ = new QCheckBox("Переприменять при изменении списка устройств", this);
    change_check_->setChecked(settings.reapply_on_change);

    wake_check_ = new QCheckBox("Переприменять после пробуждения компьютера", this);
    wake_check_->setChecked(settings.reapply_on_wake);

    poll_check_ = new QCheckBox("Периодически (мышь не пропадает из списка)", this);
    poll_check_->setChecked(settings.poll_enabled);
    poll_check_->setToolTip("Если мышь не исчезает из списка при сне, плагин сам периодически возвращает подсветку");

    poll_spin_ = new QSpinBox(this);
    poll_spin_->setRange(5, 3600);
    poll_spin_->setSingleStep(5);
    poll_spin_->setSuffix(" сек");
    poll_spin_->setValue(settings.poll_interval_sec);

    log_check_ = new QCheckBox("Писать действия в журнал OpenRGB", this);
    log_check_->setChecked(settings.log_enabled);

    snapshot_button_ = new QPushButton("Запомнить текущую подсветку", this);
    snapshot_button_->setToolTip("Сохранить снимок подсветки устройства — он будет возвращаться при пробуждении");

    status_label_ = new QLabel("", this);
    status_label_->setWordWrap(true);

    QLabel* hint_label = new QLabel(
        "Плагин запоминает подсветку выбранного устройства и возвращает её, "
        "когда мышь просыпается или подключается заново.", this);
    hint_label->setWordWrap(true);

    QFormLayout* form = new QFormLayout();
    form->addRow("Устройство (часть названия):", target_edit_);
    form->addRow("", change_check_);
    form->addRow("", wake_check_);
    form->addRow("", poll_check_);
    form->addRow("Проверять каждые:", poll_spin_);
    form->addRow("", log_check_);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(snapshot_button_);
    layout->addWidget(status_label_);
    layout->addWidget(hint_label);
    layout->addStretch();

    connect(target_edit_, &QLineEdit::textChanged, this, &SettingsWidget::EmitSettingsChanged);
    connect(change_check_, &QCheckBox::toggled, this, &SettingsWidget::EmitSettingsChanged);
    connect(wake_check_, &QCheckBox::toggled, this, &SettingsWidget::EmitSettingsChanged);
    connect(poll_check_, &QCheckBox::toggled, this, &SettingsWidget::EmitSettingsChanged);
    connect(poll_spin_, qOverload<int>(&QSpinBox::valueChanged), this, &SettingsWidget::EmitSettingsChanged);
    connect(log_check_, &QCheckBox::toggled, this, &SettingsWidget::EmitSettingsChanged);
    connect(snapshot_button_, &QPushButton::clicked, this, &SettingsWidget::snapshotRequested);
}

SettingsWidget::Settings SettingsWidget::GetSettings() const
{
    Settings s;

    s.target_key        = target_edit_->text();
    s.reapply_on_change = change_check_->isChecked();
    s.reapply_on_wake   = wake_check_->isChecked();
    s.poll_enabled      = poll_check_->isChecked();
    s.poll_interval_sec = poll_spin_->value();
    s.log_enabled       = log_check_->isChecked();

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