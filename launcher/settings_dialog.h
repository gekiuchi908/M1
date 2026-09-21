#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);

    void setLanguage(const QString &lang);
    QString language() const;

    void setRomPath(const QString &path);
    QString romPath() const;

    void setM1BinPath(const QString &path);
    QString m1BinPath() const;

    void setSoundDriver(int driver);
    int soundDriver() const;

    void setSampleRate(int rate);
    int sampleRate() const;

    void setUseTerminal(bool use);
    bool useTerminal() const;

private slots:
    void onBrowseRomPath();
    void onBrowseM1Bin();
    void onLanguageChanged(int index);

private:
    void retranslateUi();

    QComboBox *m_languageCombo;
    QLineEdit *m_romPathEdit;
    QPushButton *m_romBrowseBtn;
    QLineEdit *m_m1BinEdit;
    QPushButton *m_binBrowseBtn;
    QComboBox *m_driverCombo;
    QComboBox *m_sampleRateCombo;
    QCheckBox *m_terminalCheck;

    class QLabel *m_langLabel;
    class QLabel *m_romDirLabel;
    class QLabel *m_m1BinLabel;
    class QLabel *m_driverLabel;
    class QLabel *m_sampleRateLabel;
    class QLabel *m_execModeLabel;
    class QDialogButtonBox *m_btnBox;
};
