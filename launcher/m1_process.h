#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

class M1Process : public QObject {
    Q_OBJECT

public:
    explicit M1Process(QObject *parent = nullptr);
    ~M1Process();

    bool isRunning() const;
    QString currentRom() const { return m_currentRom; }

    // バックグラウンドプロセス起動 (GUI操作用)
    void play(const QString &m1BinPath, const QString &romName, int songNum = -1, const QStringList &extraArgs = QStringList());

    // 外部ターミナルで起動 (コンソール直接操作用)
    void playInTerminal(const QString &m1BinPath, const QString &romName, int songNum = -1, const QStringList &extraArgs = QStringList());

    // コマンド送信
    void sendCommand(char key);
    void nextTrack();
    void prevTrack();
    void togglePause();
    void restartTrack();
    void stop();

signals:
    void started(const QString &romName);
    void finished(int exitCode);
    void logOutput(const QString &text);
    void songChanged(int songNum, const QString &trackName);
    void songStopped(int songNum);
    void hardwareInfoDetected(const QString &driver, const QString &hardware);
    void errorOccurred(const QString &error);

private slots:
    void onReadyReadStandardOutput();
    void onReadyReadStandardError();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);

private:
    void parseOutputLine(const QString &line);

    QProcess *m_process;
    QString m_currentRom;
    bool m_stoppingByUser = false;
    bool m_externalTerminal = false;
    QString m_currentDriver;
    QString m_currentHardware;
};
