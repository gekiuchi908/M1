#include "settings_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QDialogButtonBox>

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle("M1 ランチャー設定");
    resize(520, 280);

    QFormLayout *form = new QFormLayout();

    // ROM パス
    QHBoxLayout *romLayout = new QHBoxLayout();
    m_romPathEdit = new QLineEdit(this);
    QPushButton *romBrowseBtn = new QPushButton("参照...", this);
    romLayout->addWidget(m_romPathEdit);
    romLayout->addWidget(romBrowseBtn);
    form->addRow("ROM ディレクトリ:", romLayout);

    // M1 実行ファイルパス
    QHBoxLayout *binLayout = new QHBoxLayout();
    m_m1BinEdit = new QLineEdit(this);
    QPushButton *binBrowseBtn = new QPushButton("参照...", this);
    binLayout->addWidget(m_m1BinEdit);
    binLayout->addWidget(binBrowseBtn);
    form->addRow("M1 実行ファイル:", binLayout);

    // サウンドドライバ (PulseAudio 推奨)
    m_driverCombo = new QComboBox(this);
    m_driverCombo->addItem("3: PulseAudio (推奨・標準)", 3);
    m_driverCombo->addItem("1: ALSA", 1);
    m_driverCombo->addItem("0: SDL", 0);
    m_driverCombo->addItem("2: OSS", 2);
    form->addRow("オーディオ出力 (-s):", m_driverCombo);

    // サンプルレート
    m_sampleRateCombo = new QComboBox(this);
    m_sampleRateCombo->addItem("44100 Hz (標準)", 44100);
    m_sampleRateCombo->addItem("48000 Hz", 48000);
    m_sampleRateCombo->addItem("22050 Hz", 22050);
    form->addRow("サンプルレート (-r):", m_sampleRateCombo);

    // 外部ターミナル起動
    m_terminalCheck = new QCheckBox("M1 を外部ターミナルウィンドウで起動する", this);
    m_terminalCheck->setToolTip("チェックすると、別ウィンドウのコンソール（x-terminal-emulator等）で M1 を開き、キーボードで直接操作できます。");
    form->addRow("実行モード:", m_terminalCheck);

    connect(romBrowseBtn, &QPushButton::clicked, this, &SettingsDialog::onBrowseRomPath);
    connect(binBrowseBtn, &QPushButton::clicked, this, &SettingsDialog::onBrowseM1Bin);

    // ボタンボックス
    QDialogButtonBox *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(btnBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(form);
    mainLayout->addStretch();
    mainLayout->addWidget(btnBox);
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
    QString dir = QFileDialog::getExistingDirectory(this, "ROM ディレクトリを選択", m_romPathEdit->text());
    if (!dir.isEmpty()) {
        m_romPathEdit->setText(dir);
    }
}

void SettingsDialog::onBrowseM1Bin() {
    QString file = QFileDialog::getOpenFileName(this, "M1 実行ファイルを選択", m_m1BinEdit->text(), "Executables (*);;All Files (*)");
    if (!file.isEmpty()) {
        m_m1BinEdit->setText(file);
    }
}
