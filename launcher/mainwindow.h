#pragma once

#include "models.h"
#include "m1_process.h"
#include "track_list.h"
#include "rom_browser_dialog.h"

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QTextEdit>
#include <QTimer>
#include <QKeyEvent>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void playGame(const GameEntry &game, int songNum = -1);

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    // ゲーム選択ダイアログを開く
    void onSelectGameClicked();

    // プレイヤーコントロール
    void onPlayClicked();
    void onStopClicked();
    void onPrevTrackClicked();
    void onNextTrackClicked();
    void onPauseToggleClicked();
    void onRestartTrackClicked();
    void onSettingsClicked();
    void onToggleLogClicked();
    void onReloadTrackListClicked();
    void onNormClicked();
    void onAutoNextClicked();

    // トラックリスト選択・ダブルクリック
    void onTrackItemDoubleClicked(QListWidgetItem *item);

    // タイマー
    void onPlayTimerTick();

    // M1 プロセスシグナル
    void onM1Started(const QString &romName);
    void onM1Finished(int exitCode);
    void onM1LogOutput(const QString &text);
    void onM1SongChanged(int songNum, const QString &trackName);
    void onM1HardwareInfoDetected(const QString &driver, const QString &hardware);
    void onM1ErrorOccurred(const QString &error);

private:
    void setupUi();
    void retranslateUi();
    void loadConfiguration();
    void saveConfiguration();
    void updateLcdDisplay();
    void updateModeButtonsStyle();
    int getCurrentTrackDuration() const;
    void highlightTrack(int songNum);

    // UI コンポーネント
    // LCD パネル
    QWidget *m_lcdPanel;
    QLabel *m_timeLabel;
    QPushButton *m_normBtn;
    QPushButton *m_autoNextBtn;
    QLabel *m_gameTitleLabel;
    QLabel *m_trackInfoLabel;
    QLabel *m_metaInfoLabel;

    // コントロールボタン群
    QPushButton *m_prevBtn;
    QPushButton *m_playBtn;
    QPushButton *m_pauseBtn;
    QPushButton *m_stopBtn;
    QPushButton *m_nextBtn;
    QPushButton *m_selectGameBtn;
    QPushButton *m_restartBtn;
    QPushButton *m_settingsBtn;
    QPushButton *m_toggleLogBtn;

    // トラックリストパネル
    QLabel *m_trackListHeader;
    QListWidget *m_trackListWidget;
    QLabel *m_fixedVolumeLabel;
    QPushButton *m_reloadListBtn;

    // ログエリア
    QTextEdit *m_logView;

    // 状態管理
    M1Process *m_m1Process;
    RomBrowserDialog *m_browserDialog = nullptr;
    QTimer *m_playTimer;

    GameEntry m_currentGame;
    TrackListData m_trackData;
    int m_currentSongNum = -1;
    int m_elapsedSeconds = 0;
    bool m_isPaused = false;
    bool m_autoSkipTriggered = false;
    QString m_detectedDriver;
    QString m_detectedHardware;

    // 設定
    QString m_language = "en";
    std::vector<std::string> m_romPaths;
    QString m_m1BinPath;
    int m_soundDriver = 3;
    int m_sampleRate = 44100;
    bool m_useTerminal = false;
    bool m_normalize = true;
    bool m_autoNext = true;
};
