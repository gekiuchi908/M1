#include "mainwindow.h"
#include "settings_dialog.h"
#include "game_cache.h"
#include "i18n.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QMessageBox>
#include <QSettings>
#include <QFileInfo>
#include <QDir>
#include <QFont>
#include <QCoreApplication>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_m1Process(new M1Process(this)),
      m_playTimer(new QTimer(this))
{
    setWindowTitle("M1-Launcher");
    setMinimumWidth(440);
    resize(480, 640);

    loadConfiguration();
    setupUi();

    // タイマー設定 (1秒ごとに tick)
    m_playTimer->setInterval(1000);
    connect(m_playTimer, &QTimer::timeout, this, &MainWindow::onPlayTimerTick);

    // M1 プロセスシグナル
    connect(m_m1Process, &M1Process::started, this, &MainWindow::onM1Started);
    connect(m_m1Process, &M1Process::finished, this, &MainWindow::onM1Finished);
    connect(m_m1Process, &M1Process::logOutput, this, &MainWindow::onM1LogOutput);
    connect(m_m1Process, &M1Process::songChanged, this, &MainWindow::onM1SongChanged);
    connect(m_m1Process, &M1Process::songStopped, this, &MainWindow::onM1SongStopped);
    connect(m_m1Process, &M1Process::hardwareInfoDetected, this, &MainWindow::onM1HardwareInfoDetected);
    connect(m_m1Process, &M1Process::errorOccurred, this, &MainWindow::onM1ErrorOccurred);

    updateLcdDisplay();
}

MainWindow::~MainWindow() {
    saveConfiguration();
}

void MainWindow::setupUi() {
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QVBoxLayout *rootLayout = new QVBoxLayout(centralWidget);
    rootLayout->setContentsMargins(8, 8, 8, 8);
    rootLayout->setSpacing(6);

    // ==========================================
    // 1. LCD パネル (オレンジ色の液晶ディスプレイ)
    // ==========================================
    m_lcdPanel = new QWidget(this);
    m_lcdPanel->setObjectName("lcdPanel");
    m_lcdPanel->setStyleSheet(
        "QWidget#lcdPanel {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ffaa00, stop:0.35 #ff8c00, stop:0.7 #e67e00, stop:1 #cc6600);"
        "  border: 2px solid #3a2200;"
        "  border-radius: 6px;"
        "}"
    );

    QVBoxLayout *lcdLayout = new QVBoxLayout(m_lcdPanel);
    lcdLayout->setContentsMargins(10, 8, 10, 8);
    lcdLayout->setSpacing(4);

    // 行 1: タイム表示 & モードボタン (NORM, NEXT)
    QHBoxLayout *lcdRow1 = new QHBoxLayout();
    m_timeLabel = new QLabel("--- ■ 00:00", m_lcdPanel);
    QFont timeFont("Monospace", 18, QFont::Bold);
    timeFont.setStyleHint(QFont::TypeWriter);
    m_timeLabel->setFont(timeFont);
    m_timeLabel->setStyleSheet("color: #2c1200;");

    QHBoxLayout *modeLayout = new QHBoxLayout();
    modeLayout->setSpacing(4);

    QLabel *speakerLabel = new QLabel("🔊", m_lcdPanel);
    speakerLabel->setStyleSheet("color: #5a2e00; font-size: 10pt;");
    modeLayout->addWidget(speakerLabel);

    m_normBtn = new QPushButton("NORM", m_lcdPanel);
    m_normBtn->setCursor(Qt::PointingHandCursor);
    connect(m_normBtn, &QPushButton::clicked, this, &MainWindow::onNormClicked);

    m_autoNextBtn = new QPushButton("NEXT", m_lcdPanel);
    m_autoNextBtn->setCursor(Qt::PointingHandCursor);
    connect(m_autoNextBtn, &QPushButton::clicked, this, &MainWindow::onAutoNextClicked);

    updateModeButtonsStyle();

    modeLayout->addWidget(m_normBtn);
    modeLayout->addWidget(m_autoNextBtn);

    lcdRow1->addWidget(m_timeLabel);
    lcdRow1->addStretch();
    lcdRow1->addLayout(modeLayout);
    lcdLayout->addLayout(lcdRow1);

    // 行 2: ゲームタイトル
    m_gameTitleLabel = new QLabel(m_lcdPanel);
    m_gameTitleLabel->setMinimumHeight(20);
    QFont titleFont = m_gameTitleLabel->font();
    titleFont.setPointSize(12);
    titleFont.setBold(true);
    m_gameTitleLabel->setFont(titleFont);
    m_gameTitleLabel->setStyleSheet("color: #1a0800;");
    m_gameTitleLabel->setWordWrap(true);
    lcdLayout->addWidget(m_gameTitleLabel);

    // 行 3: トラック名 & 曲番号
    m_trackInfoLabel = new QLabel(m_lcdPanel);
    m_trackInfoLabel->setMinimumHeight(18);
    QFont trackFont = m_trackInfoLabel->font();
    trackFont.setPointSize(10);
    trackFont.setBold(true);
    m_trackInfoLabel->setFont(trackFont);
    m_trackInfoLabel->setStyleSheet("color: #2c1200;");
    m_trackInfoLabel->setWordWrap(true);
    lcdLayout->addWidget(m_trackInfoLabel);

    // 行 4: メタ情報 (Manufacturer / System / Hardware / File / Parent)
    m_metaInfoLabel = new QLabel(m_lcdPanel);
    QFont metaFont = m_metaInfoLabel->font();
    metaFont.setPointSize(8);
    m_metaInfoLabel->setFont(metaFont);
    m_metaInfoLabel->setStyleSheet("color: #3b1800; line-height: 1.2;");
    m_metaInfoLabel->setWordWrap(true);
    lcdLayout->addWidget(m_metaInfoLabel);

    rootLayout->addWidget(m_lcdPanel);

    // ==========================================
    // 2. コントロールボタンバー
    // ==========================================
    QHBoxLayout *ctrlLayout = new QHBoxLayout();
    ctrlLayout->setSpacing(4);

    auto createBtn = [this](const QString &text) -> QPushButton* {
        QPushButton *btn = new QPushButton(text, this);
        btn->setMinimumSize(32, 28);
        QFont f = btn->font();
        f.setBold(true);
        btn->setFont(f);
        return btn;
    };

    m_prevBtn = createBtn("⏮");
    m_playBtn = createBtn("▶");
    m_pauseBtn = createBtn("⏸");
    m_stopBtn = createBtn("⏹");
    m_nextBtn = createBtn("⏭");
    m_selectGameBtn = createBtn("▲");

    m_restartBtn = createBtn("🔄");
    m_settingsBtn = createBtn("⚙");
    m_toggleLogBtn = createBtn("📄");

    m_prevBtn->setEnabled(false);
    m_playBtn->setEnabled(false);
    m_pauseBtn->setEnabled(false);
    m_stopBtn->setEnabled(false);
    m_nextBtn->setEnabled(false);
    m_restartBtn->setEnabled(false);

    ctrlLayout->addWidget(m_prevBtn);
    ctrlLayout->addWidget(m_playBtn);
    ctrlLayout->addWidget(m_pauseBtn);
    ctrlLayout->addWidget(m_stopBtn);
    ctrlLayout->addWidget(m_nextBtn);
    ctrlLayout->addWidget(m_selectGameBtn);
    ctrlLayout->addStretch();
    ctrlLayout->addWidget(m_restartBtn);
    ctrlLayout->addWidget(m_settingsBtn);
    ctrlLayout->addWidget(m_toggleLogBtn);

    rootLayout->addLayout(ctrlLayout);

    // ==========================================
    // 3. Track List パネル
    // ==========================================
    m_trackListHeader = new QLabel("Track List", this);
    m_trackListHeader->setStyleSheet(
        "background-color: #383838; color: #ffffff; font-weight: bold; padding: 4px 8px; border-top-left-radius: 4px; border-top-right-radius: 4px;"
    );
    rootLayout->addWidget(m_trackListHeader);

    m_trackListWidget = new QListWidget(this);
    m_trackListWidget->setStyleSheet(
        "QListWidget {"
        "  background-color: #242424;"
        "  color: #d8d8d8;"
        "  border: 1px solid #444444;"
        "  border-bottom-left-radius: 0px;"
        "  border-bottom-right-radius: 0px;"
        "}"
        "QListWidget::item {"
        "  padding: 3px 6px;"
        "}"
        "QListWidget::item:selected {"
        "  background-color: #c97507;"
        "  color: #ffffff;"
        "  font-weight: bold;"
        "}"
        "QListWidget::item:hover {"
        "  background-color: #3a3a3a;"
        "}"
    );
    rootLayout->addWidget(m_trackListWidget, 1);

    // トラックリストフッター (Fixed Volume & Reload)
    QHBoxLayout *listFooterLayout = new QHBoxLayout();
    listFooterLayout->setContentsMargins(4, 2, 4, 2);

    m_fixedVolumeLabel = new QLabel("Fixed Volume: --- %", this);
    m_fixedVolumeLabel->setStyleSheet("color: #888888; font-weight: bold;");
    listFooterLayout->addWidget(m_fixedVolumeLabel);

    listFooterLayout->addStretch();

    m_reloadListBtn = new QPushButton(this);
    listFooterLayout->addWidget(m_reloadListBtn);

    rootLayout->addLayout(listFooterLayout);

    // ==========================================
    // 4. M1 ログビュー (初期非表示)
    // ==========================================
    m_logView = new QTextEdit(this);
    m_logView->setReadOnly(true);
    m_logView->setMaximumHeight(100);
    m_logView->setVisible(false);
    rootLayout->addWidget(m_logView);

    // シグナル接続
    connect(m_selectGameBtn, &QPushButton::clicked, this, &MainWindow::onSelectGameClicked);
    connect(m_playBtn, &QPushButton::clicked, this, &MainWindow::onPlayClicked);
    connect(m_stopBtn, &QPushButton::clicked, this, &MainWindow::onStopClicked);
    connect(m_prevBtn, &QPushButton::clicked, this, &MainWindow::onPrevTrackClicked);
    connect(m_nextBtn, &QPushButton::clicked, this, &MainWindow::onNextTrackClicked);
    connect(m_pauseBtn, &QPushButton::clicked, this, &MainWindow::onPauseToggleClicked);
    connect(m_restartBtn, &QPushButton::clicked, this, &MainWindow::onRestartTrackClicked);
    connect(m_settingsBtn, &QPushButton::clicked, this, &MainWindow::onSettingsClicked);
    connect(m_toggleLogBtn, &QPushButton::clicked, this, &MainWindow::onToggleLogClicked);
    connect(m_reloadListBtn, &QPushButton::clicked, this, &MainWindow::onReloadTrackListClicked);

    connect(m_trackListWidget, &QListWidget::itemDoubleClicked, this, &MainWindow::onTrackItemDoubleClicked);

    retranslateUi();
}

void MainWindow::retranslateUi() {
    m_normBtn->setToolTip(I18n::normTooltip());
    m_autoNextBtn->setToolTip(I18n::nextTooltip());
    m_prevBtn->setToolTip(I18n::prevBtnTooltip());
    m_playBtn->setToolTip(I18n::playBtnTooltip());
    m_pauseBtn->setToolTip(I18n::pauseBtnTooltip());
    m_stopBtn->setToolTip(I18n::stopBtnTooltip());
    m_nextBtn->setToolTip(I18n::nextBtnTooltip());
    m_selectGameBtn->setToolTip(I18n::selectGameBtnTooltip());
    m_restartBtn->setToolTip(I18n::restartBtnTooltip());
    m_settingsBtn->setToolTip(I18n::settingsBtnTooltip());
    m_toggleLogBtn->setToolTip(I18n::toggleLogBtnTooltip());
    m_reloadListBtn->setText(I18n::reloadListBtn());
    m_reloadListBtn->setToolTip(I18n::reloadListTooltip());
}

static QString getLauncherIniPath() {
    return QCoreApplication::applicationDirPath() + "/m1-launcher.ini";
}

void MainWindow::loadConfiguration() {
    QString configPath = getLauncherIniPath();
    QSettings settings(configPath, QSettings::IniFormat);
    QString defaultRomPath = "./roms";

    QFile iniFile(QCoreApplication::applicationDirPath() + "/m1.ini");
    if (iniFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&iniFile);
        bool inRomPath = false;
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (line.startsWith("[ROMPATH]", Qt::CaseInsensitive)) {
                inRomPath = true;
            } else if (line.startsWith("[") && inRomPath) {
                inRomPath = false;
            } else if (inRomPath && !line.isEmpty() && !line.startsWith(";")) {
                defaultRomPath = line;
                break;
            }
        }
    }

    m_language = settings.value("language", "en").toString();
    I18n::setLanguage(m_language);

    QString romDir = settings.value("romPath", defaultRomPath).toString();
    m_m1BinPath = settings.value("m1BinPath", "./m1").toString();
    m_soundDriver = settings.value("soundDriver", 3).toInt(); // PulseAudio default
    m_sampleRate = settings.value("sampleRate", 44100).toInt();
    m_useTerminal = settings.value("useTerminal", false).toBool();
    m_normalize = settings.value("normalize", true).toBool();
    m_autoNext = settings.value("autoNext", true).toBool();

    m_romPaths.clear();
    m_romPaths.push_back(romDir.toStdString());
    if (romDir != "./roms") {
        m_romPaths.push_back("./roms");
    }
}

void MainWindow::saveConfiguration() {
    QString configPath = getLauncherIniPath();
    QSettings settings(configPath, QSettings::IniFormat);

    settings.setValue("language", m_language);
    if (!m_romPaths.empty()) {
        settings.setValue("romPath", QString::fromStdString(m_romPaths[0]));
    }
    settings.setValue("m1BinPath", m_m1BinPath);
    settings.setValue("soundDriver", m_soundDriver);
    settings.setValue("sampleRate", m_sampleRate);
    settings.setValue("useTerminal", m_useTerminal);
    settings.setValue("normalize", m_normalize);
    settings.setValue("autoNext", m_autoNext);
    settings.sync();
}

void MainWindow::onSelectGameClicked() {
    if (!m_browserDialog) {
        m_browserDialog = new RomBrowserDialog(m_romPaths, this);
    }

    m_browserDialog->retranslateUi();
    m_browserDialog->setRomPaths(m_romPaths);
    m_browserDialog->ensureLoaded();

    if (m_browserDialog->exec() == QDialog::Accepted) {
        GameEntry selected = m_browserDialog->selectedGame();
        if (!selected.name.empty()) {
            playGame(selected);
        }
    }
}

void MainWindow::onReloadTrackListClicked() {
    m_trackListWidget->clear();
    if (m_currentGame.name.empty()) return;

    m_trackData = TrackListParser::load(
        QString::fromStdString(m_currentGame.name),
        m_currentGame.minCmd,
        m_currentGame.maxCmd,
        m_currentGame.defaultCmd,
        m_language
    );

    if (m_trackData.fixedVolume >= 0) {
        m_fixedVolumeLabel->setText(QString("Fixed Volume: %1 %").arg(m_trackData.fixedVolume));
    } else {
        m_fixedVolumeLabel->setText("Fixed Volume: --- %");
    }

    for (const auto &it : m_trackData.items) {
        if (it.isSeparator) {
            QListWidgetItem *sepItem = new QListWidgetItem(it.title, m_trackListWidget);
            sepItem->setFlags(Qt::NoItemFlags);
            sepItem->setForeground(QColor(100, 100, 100));
        } else {
            // BridgeM1-3.jpg のように曲番号と曲名を表示
            QString text = QString("%1  %2").arg(it.songNum, 3).arg(it.title);
            QListWidgetItem *item = new QListWidgetItem(text, m_trackListWidget);
            item->setData(Qt::UserRole, it.songNum);
            item->setData(Qt::UserRole + 1, it.title);
            item->setData(Qt::UserRole + 2, it.time);
        }
    }

    if (m_currentSongNum >= 0) {
        highlightTrack(m_currentSongNum);
    }
}

void MainWindow::playGame(const GameEntry &game, int songNum) {
    if (m_currentGame.name != game.name || m_trackData.items.empty()) {
        m_currentGame = game;
        m_detectedDriver.clear();
        m_detectedHardware.clear();
        onReloadTrackListClicked();
    }

    if (songNum < 0) {
        if (m_trackData.defaultSong >= 0) {
            songNum = m_trackData.defaultSong;
        } else if (m_currentGame.defaultCmd >= 0) {
            songNum = m_currentGame.defaultCmd;
        } else if (!m_trackData.items.empty()) {
            for (const auto &it : m_trackData.items) {
                if (!it.isSeparator && it.songNum >= 0) {
                    songNum = it.songNum;
                    break;
                }
            }
        }
    }

    QStringList extraArgs;
    // M1 内部の自動曲送りを無効化し、ランチャー側のタイマーで制御する
    extraArgs << "-t";

    if (m_language == "jp") {
        extraArgs << "-j";
    }

    if (!m_normalize) {
        extraArgs << "-n";
    }
    if (m_soundDriver >= 0) {
        extraArgs << QString("-s%1").arg(m_soundDriver);
    }
    if (m_sampleRate > 0) {
        extraArgs << QString("-r%1").arg(m_sampleRate);
    }

    m_logView->clear();
    m_isPaused = false;
    m_isSongFinished = false;
    m_elapsedSeconds = 0;
    m_autoSkipTriggered = false;
    m_currentSongNum = songNum;

    updateLcdDisplay();

    if (m_useTerminal) {
        m_m1Process->playInTerminal(m_m1BinPath, QString::fromStdString(game.name), songNum, extraArgs);
    } else {
        m_m1Process->play(m_m1BinPath, QString::fromStdString(game.name), songNum, extraArgs);
    }
}

void MainWindow::onPlayClicked() {
    if (m_currentGame.name.empty()) {
        onSelectGameClicked();
        return;
    }
    playGame(m_currentGame, m_currentSongNum >= 0 ? m_currentSongNum : m_currentGame.defaultCmd);
}

void MainWindow::onStopClicked() {
    m_m1Process->stop();
    m_playTimer->stop();
    onM1Finished(0);
}

void MainWindow::onPrevTrackClicked() {
    m_elapsedSeconds = 0;
    m_isSongFinished = false;
    m_playTimer->start();
    updateLcdDisplay();
    m_m1Process->prevTrack();
}

void MainWindow::onNextTrackClicked() {
    m_elapsedSeconds = 0;
    m_isSongFinished = false;
    m_playTimer->start();
    updateLcdDisplay();
    m_m1Process->nextTrack();
}

void MainWindow::onPauseToggleClicked() {
    m_m1Process->togglePause();
    m_isPaused = !m_isPaused;
    if (m_isPaused) {
        m_playTimer->stop();
    } else if (m_m1Process->isRunning()) {
        m_playTimer->start();
    }
    updateLcdDisplay();
}

void MainWindow::onRestartTrackClicked() {
    m_m1Process->restartTrack();
    m_elapsedSeconds = 0;
    m_isSongFinished = false;
    m_playTimer->start();
    updateLcdDisplay();
}

void MainWindow::onTrackItemDoubleClicked(QListWidgetItem *item) {
    if (!item) return;
    int songNum = item->data(Qt::UserRole).toInt();
    if (songNum >= 0 && !m_currentGame.name.empty()) {
        playGame(m_currentGame, songNum);
    }
}

void MainWindow::onNormClicked() {
    m_normalize = !m_normalize;
    updateModeButtonsStyle();
    saveConfiguration();
}

void MainWindow::onAutoNextClicked() {
    m_autoNext = !m_autoNext;
    updateModeButtonsStyle();
    saveConfiguration();
}

void MainWindow::updateModeButtonsStyle() {
    QString onStyle = "QPushButton { color: #200c00; font-weight: bold; font-size: 8pt; background: transparent; border: 1px solid #7a4413; border-radius: 2px; padding: 1px 4px; }"
                      "QPushButton:hover { background-color: rgba(255, 255, 255, 0.25); }";
    QString offStyle = "QPushButton { color: #8c521a; font-weight: normal; font-size: 8pt; background: transparent; border: 1px solid transparent; border-radius: 2px; padding: 1px 4px; opacity: 0.6; }"
                       "QPushButton:hover { background-color: rgba(255, 255, 255, 0.15); border: 1px solid #8c521a; }";

    if (m_normBtn) m_normBtn->setStyleSheet(m_normalize ? onStyle : offStyle);
    if (m_autoNextBtn) m_autoNextBtn->setStyleSheet(m_autoNext ? onStyle : offStyle);
}

int MainWindow::getCurrentTrackDuration() const {
    if (m_currentSongNum < 0) return 0;
    for (const auto &it : m_trackData.items) {
        if (!it.isSeparator && it.songNum == m_currentSongNum) {
            if (it.time.isEmpty()) return 0;
            QStringList parts = it.time.split(':');
            if (parts.size() == 1) {
                return qRound(parts[0].toDouble());
            } else if (parts.size() == 2) {
                int min = parts[0].toInt();
                double sec = parts[1].toDouble();
                return min * 60 + qRound(sec);
            } else if (parts.size() == 3) {
                int hr = parts[0].toInt();
                int min = parts[1].toInt();
                double sec = parts[2].toDouble();
                return hr * 3600 + min * 60 + qRound(sec);
            }
            break;
        }
    }
    return 0;
}

void MainWindow::onPlayTimerTick() {
    if (m_isSongFinished) return;
    m_elapsedSeconds++;
    updateLcdDisplay();

    // Auto Moving (NEXT)
    if (m_autoNext && m_m1Process->isRunning() && !m_isPaused && !m_autoSkipTriggered) {
        int duration = getCurrentTrackDuration();
        if (duration > 0 && m_elapsedSeconds >= duration) {
            m_autoSkipTriggered = true;
            onNextTrackClicked();
        }
    }
}

void MainWindow::updateLcdDisplay() {
    // タイム表示
    QString stateIcon = "■";
    if (m_m1Process->isRunning() && !m_isSongFinished) {
        stateIcon = m_isPaused ? "⏸" : "▶";
    }

    int mm = m_elapsedSeconds / 60;
    int ss = m_elapsedSeconds % 60;
    m_timeLabel->setText(QString("%1 %2:%3")
                             .arg(stateIcon)
                             .arg(mm, 2, 10, QChar('0'))
                             .arg(ss, 2, 10, QChar('0')));

    // ゲームタイトル
    if (!m_currentGame.description.empty()) {
        m_gameTitleLabel->setText(QString::fromStdString(m_currentGame.description));
    } else if (!m_currentGame.name.empty()) {
        m_gameTitleLabel->setText(QString::fromStdString(m_currentGame.name));
    } else {
        m_gameTitleLabel->setText("");
    }

    // トラック情報
    if (m_currentSongNum >= 0) {
        QString trkName;
        QString trkTime;
        for (const auto &it : m_trackData.items) {
            if (!it.isSeparator && it.songNum == m_currentSongNum) {
                trkName = it.title;
                trkTime = it.time;
                break;
            }
        }

        QString trackStr = QString("#%1").arg(m_currentSongNum);
        if (!trkName.isEmpty()) {
            trackStr += " " + trkName;
        }
        if (!trkTime.isEmpty()) {
            trackStr += QString(" <%1>").arg(trkTime);
        }
        m_trackInfoLabel->setText(trackStr);
    } else {
        m_trackInfoLabel->setText("");
    }

    // メタ情報
    QString mfg = !m_currentGame.manufacturer.empty() ? QString::fromStdString(m_currentGame.manufacturer) : "";
    if (!m_currentGame.year.empty() && !mfg.isEmpty()) {
        mfg = QString("%1 %2").arg(QString::fromStdString(m_currentGame.year), mfg);
    } else if (!m_currentGame.year.empty()) {
        mfg = QString::fromStdString(m_currentGame.year);
    }

    QString system = !m_currentGame.board.empty() ? QString::fromStdString(m_currentGame.board) : "";
    if (!m_detectedDriver.isEmpty() && m_detectedDriver != system) {
        if (!system.isEmpty()) {
            system += " (" + m_detectedDriver + ")";
        } else {
            system = m_detectedDriver;
        }
    }

    QString hardware = !m_detectedHardware.isEmpty() ? m_detectedHardware : "";
    QString file = !m_currentGame.name.empty() ? QString::fromStdString(m_currentGame.name) : "";
    QString parent = !m_currentGame.parentZip.empty() ? QString::fromStdString(m_currentGame.parentZip) : "";

    QString metaText = QString(
        "<b>Manufacturer:</b> %1<br>"
        "<b>System:</b> %2<br>"
        "<b>Hardware:</b> %3<br>"
        "<b>File:</b> %4 &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; <b>Parent:</b> %5"
    ).arg(mfg, system, hardware, file, parent);

    m_metaInfoLabel->setText(metaText);
}

void MainWindow::highlightTrack(int songNum) {
    for (int i = 0; i < m_trackListWidget->count(); ++i) {
        QListWidgetItem *item = m_trackListWidget->item(i);
        if (item->data(Qt::UserRole).toInt() == songNum) {
            m_trackListWidget->setCurrentItem(item);
            m_trackListWidget->scrollToItem(item);
            break;
        }
    }
}

void MainWindow::onM1Started(const QString &romName) {
    Q_UNUSED(romName);
    m_playBtn->setEnabled(false);
    m_stopBtn->setEnabled(true);
    m_prevBtn->setEnabled(!m_useTerminal);
    m_nextBtn->setEnabled(!m_useTerminal);
    m_pauseBtn->setEnabled(!m_useTerminal);
    m_restartBtn->setEnabled(!m_useTerminal);

    m_playTimer->start();
    updateLcdDisplay();
}

void MainWindow::onM1Finished(int exitCode) {
    Q_UNUSED(exitCode);
    m_playBtn->setEnabled(!m_currentGame.name.empty());
    m_stopBtn->setEnabled(false);
    m_prevBtn->setEnabled(false);
    m_nextBtn->setEnabled(false);
    m_pauseBtn->setEnabled(false);
    m_restartBtn->setEnabled(false);

    m_playTimer->stop();
    m_isPaused = false;
    m_isSongFinished = false;
    updateLcdDisplay();
}

void MainWindow::onM1LogOutput(const QString &text) {
    m_logView->append(text);
}

void MainWindow::onM1SongChanged(int songNum, const QString &trackName) {
    Q_UNUSED(trackName);
    m_currentSongNum = songNum;
    m_elapsedSeconds = 0;
    m_isSongFinished = false;
    m_autoSkipTriggered = false;
    if (m_m1Process->isRunning() && !m_isPaused) {
        m_playTimer->start();
    }
    updateLcdDisplay();
    highlightTrack(songNum);
}

void MainWindow::onM1SongStopped(int songNum) {
    if (songNum != m_currentSongNum || !m_m1Process->isRunning()) return;

    m_isSongFinished = true;
    m_playTimer->stop();

    // 3秒の無音判定時間を差し引いて実際の音の停止時点を表示
    if (m_elapsedSeconds >= 3) {
        m_elapsedSeconds -= 3;
    } else {
        m_elapsedSeconds = 0;
    }
    updateLcdDisplay();

    // Auto Moving (NEXT) がONの場合は自動的に次の曲へ送る
    if (m_autoNext && !m_autoSkipTriggered) {
        m_autoSkipTriggered = true;
        QTimer::singleShot(500, this, &MainWindow::onNextTrackClicked);
    }
}

void MainWindow::onM1HardwareInfoDetected(const QString &driver, const QString &hardware) {
    if (!driver.isEmpty()) m_detectedDriver = driver;
    if (!hardware.isEmpty()) m_detectedHardware = hardware;
    updateLcdDisplay();
}

void MainWindow::onM1ErrorOccurred(const QString &error) {
    QMessageBox::warning(this, I18n::m1ErrorTitle(), error);
    onM1Finished(-1);
}

void MainWindow::onToggleLogClicked() {
    bool vis = !m_logView->isVisible();
    m_logView->setVisible(vis);
}

void MainWindow::onSettingsClicked() {
    SettingsDialog dlg(this);
    dlg.setLanguage(m_language);
    if (!m_romPaths.empty()) {
        dlg.setRomPath(QString::fromStdString(m_romPaths[0]));
    }
    dlg.setM1BinPath(m_m1BinPath);
    dlg.setSoundDriver(m_soundDriver);
    dlg.setSampleRate(m_sampleRate);
    dlg.setUseTerminal(m_useTerminal);

    if (dlg.exec() == QDialog::Accepted) {
        QString newLang = dlg.language();
        bool langChanged = (newLang != m_language);
        m_language = newLang;
        I18n::setLanguage(m_language);

        QString newRom = dlg.romPath();
        m_m1BinPath = dlg.m1BinPath();
        m_soundDriver = dlg.soundDriver();
        m_sampleRate = dlg.sampleRate();
        m_useTerminal = dlg.useTerminal();

        m_romPaths.clear();
        m_romPaths.push_back(newRom.toStdString());
        if (newRom != "./roms") {
            m_romPaths.push_back("./roms");
        }

        saveConfiguration();

        if (m_browserDialog) {
            m_browserDialog->setRomPaths(m_romPaths);
            if (langChanged) {
                m_browserDialog->retranslateUi();
            }
        }

        if (langChanged) {
            retranslateUi();
            onReloadTrackListClicked();
            updateLcdDisplay();
        }
    }
}

void MainWindow::keyPressEvent(QKeyEvent *event) {
    if (m_m1Process->isRunning()) {
        switch (event->key()) {
            case Qt::Key_Space:
                onPauseToggleClicked();
                event->accept();
                return;
            case Qt::Key_Plus:
            case Qt::Key_Equal:
                onNextTrackClicked();
                event->accept();
                return;
            case Qt::Key_Minus:
                onPrevTrackClicked();
                event->accept();
                return;
            case Qt::Key_0:
                onRestartTrackClicked();
                event->accept();
                return;
            case Qt::Key_Escape:
                onStopClicked();
                event->accept();
                return;
        }
    }

    QMainWindow::keyPressEvent(event);
}
