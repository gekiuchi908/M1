#pragma once

#include "models.h"
#include "game_table_model.h"
#include "rom_verifier.h"
#include "m1_process.h"

#include <QMainWindow>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QTableView>
#include <QTreeWidget>
#include <QLabel>
#include <QProgressBar>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QTextEdit>
#include <QKeyEvent>

class GameFilterProxyModel : public QSortFilterProxyModel {
    Q_OBJECT

public:
    enum StatusFilter {
        FilterAll = 0,
        FilterAvailable,     // 所持 (OK + 不一致)
        FilterOkOnly,        // 完全一致のみ
        FilterMismatched,    // 不一致・エラーのみ
        FilterMissingOnly    // 未所持のみ
    };

    explicit GameFilterProxyModel(QObject *parent = nullptr);

    void setStatusFilter(StatusFilter filter);
    void setSearchText(const QString &text);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
    bool lessThan(const QModelIndex &sourceLeft, const QModelIndex &sourceRight) const override;

private:
    StatusFilter m_statusFilter = FilterAll;
    QString m_searchText;
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onSearchTextChanged(const QString &text);
    void onStatusFilterChanged(int index);
    void onGameSelectionChanged(const QModelIndex &current, const QModelIndex &previous);
    void onGameDoubleClicked(const QModelIndex &index);
    void onRescanClicked();

    // プレイヤーコントロール
    void onPlayClicked();
    void onStopClicked();
    void onPrevTrackClicked();
    void onNextTrackClicked();
    void onPauseToggleClicked();
    void onRestartTrackClicked();

    void onSettingsClicked();
    void onToggleLogClicked();

    // 検証バッチ通知
    void onBatchVerified(const std::vector<VerifiedItem> &items);
    void onVerificationProgress(int current, int total);
    void onVerificationFinished();

    // M1 プロセス関連
    void onM1Started(const QString &romName);
    void onM1Finished(int exitCode);
    void onM1LogOutput(const QString &text);
    void onM1SongChanged(int songNum, const QString &trackName);
    void onM1ErrorOccurred(const QString &error);

private:
    void setupUi();
    void loadConfiguration();
    void saveConfiguration();
    void loadXml();
    void updateDetailPanel(const GameEntry &game);
    void updateStatusBarStats();

    // UI コンポーネント
    QLineEdit *m_searchEdit;
    QComboBox *m_filterCombo;
    QPushButton *m_rescanBtn;
    QPushButton *m_settingsBtn;

    // プレイヤーボタン群
    QPushButton *m_playBtn;
    QPushButton *m_stopBtn;
    QPushButton *m_prevBtn;
    QPushButton *m_nextBtn;
    QPushButton *m_pauseBtn;
    QPushButton *m_restartBtn;
    QPushButton *m_toggleLogBtn;

    QTableView *m_tableView;
    GameTableModel *m_tableModel;
    GameFilterProxyModel *m_proxyModel;

    // 詳細パネル
    QLabel *m_detailTitle;
    QLabel *m_detailInfo;
    QLabel *m_detailStatusBadge;
    QTreeWidget *m_romTreeWidget;

    // ログ / 進行状況
    QTextEdit *m_logView;
    QProgressBar *m_progressBar;
    QLabel *m_statsLabel;
    QLabel *m_playStatusLabel;

    // バックエンド
    RomVerifier *m_verifier;
    M1Process *m_m1Process;

    std::vector<std::string> m_romPaths;
    QString m_m1BinPath;
    int m_soundDriver = 3; // PulseAudio default!
    int m_sampleRate = 44100;
    bool m_useTerminal = false;
    bool m_isPaused = false;
};
