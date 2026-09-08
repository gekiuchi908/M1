#include "m1_process.h"
#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>
#include <QStandardPaths>

M1Process::M1Process(QObject *parent) : QObject(parent), m_process(new QProcess(this)) {
    connect(m_process, &QProcess::readyReadStandardOutput, this, &M1Process::onReadyReadStandardOutput);
    connect(m_process, &QProcess::readyReadStandardError, this, &M1Process::onReadyReadStandardError);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &M1Process::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, &M1Process::onProcessError);
}

M1Process::~M1Process() {
    stop();
}

bool M1Process::isRunning() const {
    return m_process->state() != QProcess::NotRunning;
}

void M1Process::play(const QString &m1BinPath, const QString &romName, const QStringList &extraArgs) {
    m_externalTerminal = false;

    if (isRunning()) {
        m_stoppingByUser = true;
        m_process->terminate();
        if (!m_process->waitForFinished(500)) {
            m_process->kill();
            m_process->waitForFinished(300);
        }
    }

    m_stoppingByUser = false;
    m_currentRom = romName;

    QString program = m1BinPath;
    if (!QFileInfo::exists(program)) {
        program = "./m1";
    }

    QStringList args = extraArgs;
    args << romName;

    m_process->setWorkingDirectory(QFileInfo(program).absolutePath());
    m_process->start(program, args);

    if (m_process->waitForStarted(2000)) {
        emit started(romName);
    }
}

void M1Process::playInTerminal(const QString &m1BinPath, const QString &romName, const QStringList &extraArgs) {
    if (isRunning()) {
        m_stoppingByUser = true;
        m_process->terminate();
        if (!m_process->waitForFinished(500)) {
            m_process->kill();
            m_process->waitForFinished(300);
        }
    }

    m_stoppingByUser = false;
    m_externalTerminal = true;
    m_currentRom = romName;

    QString program = m1BinPath;
    if (!QFileInfo::exists(program)) {
        program = "./m1";
    }

    QString absProgram = QFileInfo(program).canonicalFilePath();
    if (absProgram.isEmpty()) {
        absProgram = QFileInfo(program).absoluteFilePath();
    }

    // 利用可能なターミナルを探索
    QString terminal;
    QStringList termArgs;

    if (!QStandardPaths::findExecutable("x-terminal-emulator").isEmpty()) {
        terminal = "x-terminal-emulator";
        termArgs << "-e" << absProgram;
        termArgs << extraArgs;
        termArgs << romName;
    } else if (!QStandardPaths::findExecutable("gnome-terminal").isEmpty()) {
        terminal = "gnome-terminal";
        termArgs << "--" << absProgram;
        termArgs << extraArgs;
        termArgs << romName;
    } else if (!QStandardPaths::findExecutable("konsole").isEmpty()) {
        terminal = "konsole";
        termArgs << "-e" << absProgram;
        termArgs << extraArgs;
        termArgs << romName;
    } else if (!QStandardPaths::findExecutable("xterm").isEmpty()) {
        terminal = "xterm";
        termArgs << "-e" << absProgram;
        termArgs << extraArgs;
        termArgs << romName;
    } else {
        // 見つからない場合は通常起動にフォールバック
        play(m1BinPath, romName, extraArgs);
        return;
    }

    m_process->setWorkingDirectory(QFileInfo(absProgram).absolutePath());
    m_process->start(terminal, termArgs);

    if (m_process->waitForStarted(2000)) {
        emit started(romName);
    }
}

void M1Process::sendCommand(char key) {
    if (isRunning() && !m_externalTerminal) {
        m_process->write(&key, 1);
        m_process->waitForBytesWritten(50);
    }
}

void M1Process::nextTrack() {
    sendCommand('+');
}

void M1Process::prevTrack() {
    sendCommand('-');
}

void M1Process::togglePause() {
    sendCommand(' ');
}

void M1Process::restartTrack() {
    sendCommand('0');
}

void M1Process::stop() {
    if (isRunning()) {
        m_stoppingByUser = true;
        if (!m_externalTerminal) {
            sendCommand('q');
        }
        m_process->terminate();
        if (!m_process->waitForFinished(500)) {
            m_process->kill();
            m_process->waitForFinished(200);
        }
    }
    m_currentRom.clear();
    m_stoppingByUser = false;
}

void M1Process::onReadyReadStandardOutput() {
    QByteArray data = m_process->readAllStandardOutput();
    QString text = QString::fromUtf8(data);
    emit logOutput(text);

    for (const QString &line : text.split('\n')) {
        parseOutputLine(line.trimmed());
    }
}

void M1Process::onReadyReadStandardError() {
    QByteArray data = m_process->readAllStandardError();
    QString text = QString::fromUtf8(data);
    emit logOutput(text);
}

void M1Process::parseOutputLine(const QString &line) {
    static QRegularExpression rx("Starting song #(\\d+)(?::\\s*(.*))?");
    auto match = rx.match(line);
    if (match.hasMatch()) {
        int songNum = match.captured(1).toInt();
        QString trackName = match.captured(2).trimmed();
        emit songChanged(songNum, trackName);
    }
}

void M1Process::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    Q_UNUSED(exitStatus);
    m_stoppingByUser = false;
    m_currentRom.clear();
    emit finished(exitCode);
}

void M1Process::onProcessError(QProcess::ProcessError error) {
    if (m_stoppingByUser) {
        return; // ユーザーによる停止や切替なのでエラーとして通知しない
    }

    QString errStr;
    switch (error) {
        case QProcess::FailedToStart:
            errStr = "M1 の起動に失敗しました。バイナリパスを確認してください。";
            break;
        case QProcess::Crashed:
            errStr = "M1 が異常終了しました。";
            break;
        case QProcess::Timedout:
            errStr = "M1 プロセスがタイムアウトしました。";
            break;
        default:
            errStr = "M1 プロセスエラーが発生しました。";
            break;
    }
    m_currentRom.clear();
    emit errorOccurred(errStr);
}
