#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);

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

private:
    QLineEdit *m_romPathEdit;
    QLineEdit *m_m1BinEdit;
    QComboBox *m_driverCombo;
    QComboBox *m_sampleRateCombo;
    QCheckBox *m_terminalCheck;
};
