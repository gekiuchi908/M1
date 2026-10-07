#pragma once

#include "models.h"
#include "game_table_model.h"
#include "rom_verifier.h"

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QTableView>
#include <QTreeWidget>
#include <QLabel>
#include <QProgressBar>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QDateTime>

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

class RomBrowserDialog : public QDialog {
    Q_OBJECT

public:
    explicit RomBrowserDialog(const std::vector<std::string> &romPaths, QWidget *parent = nullptr);
    ~RomBrowserDialog();

    void setRomPaths(const std::vector<std::string> &paths);
    GameEntry selectedGame() const { return m_selectedGame; }

    // キャッシュまたは XML からデータをロード
    void ensureLoaded();

    // 言語切り替え時に UI を再翻訳
    void retranslateUi();

private slots:
    void onSearchTextChanged(const QString &text);
    void onStatusFilterChanged(int index);
    void onGameSelectionChanged(const QModelIndex &current, const QModelIndex &previous);
    void onGameDoubleClicked(const QModelIndex &index);
    void onOkPlayClicked();
    void onRescanClicked();

    // 検証バッチ通知
    void onBatchVerified(const std::vector<VerifiedItem> &items);
    void onVerificationProgress(int current, int total);
    void onVerificationFinished();

private:
    void setupUi();
    void loadXml();
    QString getXmlPath() const;
    void updateDetailPanel(const GameEntry &game);
    void updateStatusBarStats();

    // UI コンポーネント
    QLabel *m_searchLabel;
    QLineEdit *m_searchEdit;
    QLabel *m_filterLabel;
    QComboBox *m_filterCombo;
    QPushButton *m_rescanBtn;
    QPushButton *m_okPlayBtn;
    QPushButton *m_cancelBtn;

    QTableView *m_tableView;
    GameTableModel *m_tableModel;
    GameFilterProxyModel *m_proxyModel;

    // 詳細パネル
    QLabel *m_detailTitle;
    QLabel *m_detailInfo;
    QLabel *m_detailStatusBadge;
    QLabel *m_romListLabel;
    QTreeWidget *m_romTreeWidget;

    // 進行状況 / 統計
    QProgressBar *m_progressBar;
    QLabel *m_statsLabel;

    // バックエンド
    RomVerifier *m_verifier;
    std::vector<std::string> m_romPaths;
    GameEntry m_selectedGame;
    bool m_isLoaded = false;
    QDateTime m_xmlLastModified;
};
