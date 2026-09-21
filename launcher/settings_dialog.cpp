#include "settings_dialog.h"
#include "i18n.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>
#include <QDialogButtonBox>

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent) {
    resize(540, 320);

    QFormLayout *form = new QFormLayout();

    // 言語設定
    m_langLabel = new QLabel(this);
    m_languageCombo = new QComboBox(this);
    m_languageCombo->addItem("English", "en");
    m_languageCombo->addItem("Japanese (日本語)", "jp");
    form->addRow(m_langLabel, m_languageCombo);

    // ROM パス
    m_romDirLabel = new QLabel(this);
    QHBoxLayout *romLayout = new QHBoxLayout();
    m_romPathEdit = new QLineEdit(this);
    m_romBrowseBtn = new QPushButton(this);
    romLayout->addWidget(m_romPathEdit);
    romLayout->addWidget(m_romBrowseBtn);
    form->addRow(m_romDirLabel, romLayout);

    // M1 実行ファイルパス
    m_m1BinLabel = new QLabel(this);
    QHBoxLayout *binLayout = new QHBoxLayout();
    m_m1BinEdit = new QLineEdit(this);
    m_binBrowseBtn = new QPushButton(this);
    binLayout->addWidget(m_m1BinEdit);
    binLayout->addWidget(m_binBrowseBtn);
    form->addRow(m_m1BinLabel, binLayout);

    // サウンドドライバ
    m_driverLabel = new QLabel(this);
    m_driverCombo = new QComboBox(this);
    m_driverCombo->addItem(I18n::driverPulseAudio(), 3);
    m_driverCombo->addItem("1: ALSA", 1);
    m_driverCombo->addItem("0: SDL", 0);
    m_driverCombo->addItem("2: OSS", 2);
    form->addRow(m_driverLabel, m_driverCombo);

    // サンプルレート
    m_sampleRateLabel = new QLabel(this);
    m_sampleRateCombo = new QComboBox(this);
    m_sampleRateCombo->addItem(I18n::sampleRateDefault(), 44100);
    m_sampleRateCombo->addItem("48000 Hz", 48000);
    m_sampleRateCombo->addItem("22050 Hz", 22050);
    form->addRow(m_sampleRateLabel, m_sampleRateCombo);

    // 外部ターミナル起動
    m_execModeLabel = new QLabel(this);
    m_terminalCheck = new QCheckBox(this);
    form->addRow(m_execModeLabel, m_terminalCheck);

    connect(m_romBrowseBtn, &QPushButton::clicked, this, &SettingsDialog::onBrowseRomPath);
    connect(m_binBrowseBtn, &QPushButton::clicked, this, &SettingsDialog::onBrowseM1Bin);
    connect(m_languageCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsDialog::onLanguageChanged);

    // ボタンボックス
    m_btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(m_btnBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(form);
    mainLayout->addStretch();
    mainLayout->addWidget(m_btnBox);

    // 初期言語を現在の I18n 設定から反映
    setLanguage(I18n::language());
    retranslateUi();
}

void SettingsDialog::retranslateUi() {
    setWindowTitle(I18n::settingsTitle());
    m_langLabel->setText(I18n::langLabel());
    m_romDirLabel->setText(I18n::romDirLabel());
    m_romBrowseBtn->setText(I18n::browseBtn());
    m_m1BinLabel->setText(I18n::m1BinLabel());
    m_binBrowseBtn->setText(I18n::browseBtn());
    m_driverLabel->setText(I18n::audioOutputLabel());
    m_sampleRateLabel->setText(I18n::sampleRateLabel());
    m_execModeLabel->setText(I18n::execModeLabel());
    m_terminalCheck->setText(I18n::terminalCheckText());
    m_terminalCheck->setToolTip(I18n::terminalCheckTooltip());

    // ドライバ PulseAudio ラベルの更新
    int paIdx = m_driverCombo->findData(3);
    if (paIdx >= 0) {
        m_driverCombo->setItemText(paIdx, I18n::driverPulseAudio());
    }

    // サンプルレート 44100Hz ラベルの更新
    int srIdx = m_sampleRateCombo->findData(44100);
    if (srIdx >= 0) {
        m_sampleRateCombo->setItemText(srIdx, I18n::sampleRateDefault());
    }
}

void SettingsDialog::onLanguageChanged(int index) {
    QString lang = m_languageCombo->itemData(index).toString();
    I18n::setLanguage(lang);
    retranslateUi();
}

void SettingsDialog::setLanguage(const QString &lang) {
    int idx = m_languageCombo->findData(lang);
    if (idx >= 0) {
        m_languageCombo->setCurrentIndex(idx);
    }
}

QString SettingsDialog::language() const {
    return m_languageCombo->currentData().toString();
}

void SettingsDialog::setRomPath(const QString &path) {
    m_romPathEdit->setText(path);
}

QString SettingsDialog::romPath() const {
    return m_romPathEdit->text().trimmed();
}

void SettingsDialog::setM1BinPath(const QString &path) {
    m_m1BinEdit->setText(path);
}

QString SettingsDialog::m1BinPath() const {
    return m_m1BinEdit->text().trimmed();
}

void SettingsDialog::setSoundDriver(int driver) {
    int idx = m_driverCombo->findData(driver);
    if (idx >= 0) m_driverCombo->setCurrentIndex(idx);
}

int SettingsDialog::soundDriver() const {
    return m_driverCombo->currentData().toInt();
}

void SettingsDialog::setSampleRate(int rate) {
    int idx = m_sampleRateCombo->findData(rate);
    if (idx >= 0) m_sampleRateCombo->setCurrentIndex(idx);
}

int SettingsDialog::sampleRate() const {
    return m_sampleRateCombo->currentData().toInt();
}

void SettingsDialog::setUseTerminal(bool use) {
    m_terminalCheck->setChecked(use);
}

bool SettingsDialog::useTerminal() const {
    return m_terminalCheck->isChecked();
}

void SettingsDialog::onBrowseRomPath() {
    QString dir = QFileDialog::getExistingDirectory(this, I18n::browseRomDialogTitle(), m_romPathEdit->text());
    if (!dir.isEmpty()) {
        m_romPathEdit->setText(dir);
    }
}

void SettingsDialog::onBrowseM1Bin() {
    QString file = QFileDialog::getOpenFileName(this, I18n::browseM1BinDialogTitle(), m_m1BinEdit->text(), "Executables (*);;All Files (*)");
    if (!file.isEmpty()) {
        m_m1BinEdit->setText(file);
    }
}
