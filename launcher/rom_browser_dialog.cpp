#include "rom_browser_dialog.h"
#include "game_cache.h"
#include "xml_parser.h"
#include "i18n.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <QFont>
#include <QDebug>

// ==========================================
// GameFilterProxyModel
// ==========================================

GameFilterProxyModel::GameFilterProxyModel(QObject *parent) : QSortFilterProxyModel(parent) {
    setSortRole(Qt::UserRole);
}

void GameFilterProxyModel::setStatusFilter(StatusFilter filter) {
    m_statusFilter = filter;
    invalidate();
}

void GameFilterProxyModel::setSearchText(const QString &text) {
    m_searchText = text.trimmed().toLower();
    invalidate();
}

bool GameFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const {
    auto *model = qobject_cast<GameTableModel*>(sourceModel());
    if (!model) return true;

    const GameEntry &game = model->getGame(sourceRow);

    // ステータスフィルタ
    switch (m_statusFilter) {
        case FilterAvailable:
            if (game.status == GameStatus::Missing || game.status == GameStatus::Unchecked) {
                return false;
            }
            break;
        case FilterOkOnly:
            if (game.status != GameStatus::Ok) {
                return false;
            }
            break;
        case FilterMismatched:
            if (game.status != GameStatus::CrcMismatch &&
                game.status != GameStatus::Sha1Mismatch &&
                game.status != GameStatus::BothMismatch &&
                game.status != GameStatus::Incomplete) {
                return false;
            }
            break;
        case FilterMissingOnly:
            if (game.status != GameStatus::Missing) {
                return false;
            }
            break;
        case FilterAll:
        default:
            break;
    }

    // 検索テキスト
    if (!m_searchText.isEmpty()) {
        QString desc = QString::fromStdString(game.description).toLower();
        QString name = QString::fromStdString(game.name).toLower();
        QString mfg = QString::fromStdString(game.manufacturer).toLower();
        QString board = QString::fromStdString(game.board).toLower();

        if (!desc.contains(m_searchText) &&
            !name.contains(m_searchText) &&
            !mfg.contains(m_searchText) &&
            !board.contains(m_searchText)) {
            return false;
        }
    }

    return true;
}

static int statusPriority(GameStatus s) {
    switch (s) {
        case GameStatus::Ok:           return 0; // 最優先
        case GameStatus::CrcMismatch:  return 1;
        case GameStatus::Sha1Mismatch: return 2;
        case GameStatus::BothMismatch: return 3;
        case GameStatus::Incomplete:   return 4;
        case GameStatus::Missing:      return 5;
        case GameStatus::Checking:     return 6;
        case GameStatus::Unchecked:    return 7;
    }
    return 8;
}

bool GameFilterProxyModel::lessThan(const QModelIndex &sourceLeft, const QModelIndex &sourceRight) const {
    auto *model = qobject_cast<GameTableModel*>(sourceModel());
    if (model) {
        const GameEntry &leftGame = model->getGame(sourceLeft.row());
        const GameEntry &rightGame = model->getGame(sourceRight.row());

        if (sourceLeft.column() == GameTableModel::ColStatus) {
            return statusPriority(leftGame.status) < statusPriority(rightGame.status);
        }
        if (sourceLeft.column() == GameTableModel::ColDescription) {
            return QString::compare(QString::fromStdString(leftGame.description),
                                   QString::fromStdString(rightGame.description),
                                   Qt::CaseInsensitive) < 0;
        }
    }

    QVariant leftData = sourceModel()->data(sourceLeft, Qt::UserRole);
    QVariant rightData = sourceModel()->data(sourceRight, Qt::UserRole);

    if (sourceLeft.column() == GameTableModel::ColRomCount) {
        return leftData.toInt() < rightData.toInt();
    }

    return QString::compare(leftData.toString(), rightData.toString(), Qt::CaseInsensitive) < 0;
}

// ==========================================
// RomBrowserDialog
// ==========================================

RomBrowserDialog::RomBrowserDialog(const std::vector<std::string> &romPaths, QWidget *parent)
    : QDialog(parent),
      m_tableModel(new GameTableModel(this)),
      m_proxyModel(new GameFilterProxyModel(this)),
      m_verifier(new RomVerifier(this)),
      m_romPaths(romPaths)
{
    setWindowTitle("M1 Rom Browser");
    resize(1050, 720);

    setupUi();

    // 検証バッチシグナル
    connect(m_verifier, &RomVerifier::batchVerified, this, &RomBrowserDialog::onBatchVerified);
    connect(m_verifier, &RomVerifier::progress, this, &RomBrowserDialog::onVerificationProgress);
    connect(m_verifier, &RomVerifier::finished, this, &RomBrowserDialog::onVerificationFinished);
}

RomBrowserDialog::~RomBrowserDialog() {
    if (m_verifier->isRunning()) {
        m_verifier->cancel();
    }
}

void RomBrowserDialog::setRomPaths(const std::vector<std::string> &paths) {
    m_romPaths = paths;
}

void RomBrowserDialog::setupUi() {
    QVBoxLayout *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(8, 8, 8, 8);
    rootLayout->setSpacing(6);

    // 1. トップツールバー
    QHBoxLayout *topBar = new QHBoxLayout();
    
    m_searchLabel = new QLabel(this);
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setClearButtonEnabled(true);
    topBar->addWidget(m_searchLabel);
    topBar->addWidget(m_searchEdit, 2);

    m_filterLabel = new QLabel(this);
    m_filterCombo = new QComboBox(this);
    topBar->addWidget(m_filterLabel);
    topBar->addWidget(m_filterCombo, 1);

    m_rescanBtn = new QPushButton(this);
    topBar->addWidget(m_rescanBtn);

    rootLayout->addLayout(topBar);

    // 2. メインテーブル & スプリッター
    QSplitter *mainSplitter = new QSplitter(Qt::Vertical, this);

    // テーブルビュー
    m_proxyModel->setSourceModel(m_tableModel);
    m_tableView = new QTableView(this);
    m_tableView->setModel(m_proxyModel);
    m_tableView->setSortingEnabled(true);
    m_tableView->sortByColumn(GameTableModel::ColDescription, Qt::AscendingOrder);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableView->verticalHeader()->setVisible(false);
    m_tableView->horizontalHeader()->setStretchLastSection(false);
    m_tableView->horizontalHeader()->setSectionResizeMode(GameTableModel::ColDescription, QHeaderView::Stretch);
    m_tableView->horizontalHeader()->setSectionResizeMode(GameTableModel::ColStatus, QHeaderView::ResizeToContents);
    m_tableView->horizontalHeader()->setSectionResizeMode(GameTableModel::ColYear, QHeaderView::ResizeToContents);
    m_tableView->horizontalHeader()->setSectionResizeMode(GameTableModel::ColRomName, QHeaderView::ResizeToContents);
    m_tableView->horizontalHeader()->setSectionResizeMode(GameTableModel::ColRomCount, QHeaderView::ResizeToContents);
    m_tableView->setAlternatingRowColors(true);
    mainSplitter->addWidget(m_tableView);

    // 3. 詳細パネル (下部)
    QWidget *bottomWidget = new QWidget(this);
    QVBoxLayout *bottomLayout = new QVBoxLayout(bottomWidget);
    bottomLayout->setContentsMargins(4, 4, 4, 4);

    QSplitter *detailSplitter = new QSplitter(Qt::Horizontal, bottomWidget);

    // 左: ゲーム基本情報
    QWidget *infoPanel = new QWidget(this);
    QVBoxLayout *infoLayout = new QVBoxLayout(infoPanel);
    infoLayout->setContentsMargins(4, 4, 4, 4);

    m_detailTitle = new QLabel(this);
    QFont titleFont = m_detailTitle->font();
    titleFont.setPointSize(titleFont.pointSize() + 2);
    titleFont.setBold(true);
    m_detailTitle->setFont(titleFont);
    m_detailTitle->setWordWrap(true);

    m_detailStatusBadge = new QLabel("", this);
    m_detailStatusBadge->setStyleSheet("font-weight: bold; padding: 4px; border-radius: 4px;");

    m_detailInfo = new QLabel("", this);
    m_detailInfo->setWordWrap(true);

    infoLayout->addWidget(m_detailTitle);
    infoLayout->addWidget(m_detailStatusBadge);
    infoLayout->addWidget(m_detailInfo);
    infoLayout->addStretch();
    infoPanel->setMinimumWidth(280);
    detailSplitter->addWidget(infoPanel);

    // 右: 個別 ROM リスト
    QWidget *romPanel = new QWidget(this);
    QVBoxLayout *romLayout = new QVBoxLayout(romPanel);
    romLayout->setContentsMargins(4, 4, 4, 4);

    m_romListLabel = new QLabel(this);
    m_romListLabel->setStyleSheet("font-weight: bold;");
    romLayout->addWidget(m_romListLabel);

    m_romTreeWidget = new QTreeWidget(this);
    m_romTreeWidget->setRootIsDecorated(false);
    m_romTreeWidget->setAlternatingRowColors(true);
    m_romTreeWidget->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_romTreeWidget->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    romLayout->addWidget(m_romTreeWidget);

    detailSplitter->addWidget(romPanel);
    detailSplitter->setStretchFactor(0, 1);
    detailSplitter->setStretchFactor(1, 3);

    bottomLayout->addWidget(detailSplitter);
    mainSplitter->addWidget(bottomWidget);
    mainSplitter->setStretchFactor(0, 3);
    mainSplitter->setStretchFactor(1, 2);

    rootLayout->addWidget(mainSplitter);

    // 4. フッターバー (統計、プログレスバー、操作ボタン)
    QHBoxLayout *footerLayout = new QHBoxLayout();

    m_statsLabel = new QLabel(this);
    footerLayout->addWidget(m_statsLabel, 1);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setMaximumWidth(200);
    m_progressBar->setVisible(false);
    footerLayout->addWidget(m_progressBar);

    m_okPlayBtn = new QPushButton(this);
    m_okPlayBtn->setStyleSheet("font-weight: bold; padding: 6px 16px; background-color: #2e7d32; color: white;");
    
    m_cancelBtn = new QPushButton(this);
    m_cancelBtn->setStyleSheet("padding: 6px 14px;");

    footerLayout->addWidget(m_okPlayBtn);
    footerLayout->addWidget(m_cancelBtn);

    rootLayout->addLayout(footerLayout);

    // シグナル接続
    connect(m_searchEdit, &QLineEdit::textChanged, this, &RomBrowserDialog::onSearchTextChanged);
    connect(m_filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &RomBrowserDialog::onStatusFilterChanged);
    connect(m_tableView->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &RomBrowserDialog::onGameSelectionChanged);
    connect(m_tableView, &QTableView::doubleClicked, this, &RomBrowserDialog::onGameDoubleClicked);
    connect(m_rescanBtn, &QPushButton::clicked, this, &RomBrowserDialog::onRescanClicked);
    connect(m_okPlayBtn, &QPushButton::clicked, this, &RomBrowserDialog::onOkPlayClicked);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    retranslateUi();
}

void RomBrowserDialog::retranslateUi() {
    setWindowTitle(I18n::romBrowserTitle());
    m_searchLabel->setText(I18n::searchLabel());
    m_searchEdit->setPlaceholderText(I18n::searchPlaceholder());
    m_filterLabel->setText(I18n::filterLabel());

    int currentFilterIdx = m_filterCombo->currentIndex();
    m_filterCombo->blockSignals(true);
    m_filterCombo->clear();
    m_filterCombo->addItem(I18n::filterOption(0), GameFilterProxyModel::FilterAll);
    m_filterCombo->addItem(I18n::filterOption(1), GameFilterProxyModel::FilterAvailable);
    m_filterCombo->addItem(I18n::filterOption(2), GameFilterProxyModel::FilterOkOnly);
    m_filterCombo->addItem(I18n::filterOption(3), GameFilterProxyModel::FilterMismatched);
    m_filterCombo->addItem(I18n::filterOption(4), GameFilterProxyModel::FilterMissingOnly);
    if (currentFilterIdx >= 0 && currentFilterIdx < m_filterCombo->count()) {
        m_filterCombo->setCurrentIndex(currentFilterIdx);
    }
    m_filterCombo->blockSignals(false);

    m_rescanBtn->setText(I18n::rescanButton());
    m_romListLabel->setText(I18n::individualRomVerification());

    QStringList treeHeaders;
    for (int i = 0; i < 7; ++i) {
        treeHeaders << I18n::romTreeHeader(i);
    }
    m_romTreeWidget->setHeaderLabels(treeHeaders);

    m_okPlayBtn->setText(I18n::playButton());
    m_cancelBtn->setText(I18n::cancelButton());

    m_tableModel->refreshTranslations();

    QModelIndex current = m_tableView->currentIndex();
    if (current.isValid()) {
        QModelIndex srcIdx = m_proxyModel->mapToSource(current);
        const GameEntry &game = m_tableModel->getGame(srcIdx.row());
        updateDetailPanel(game);
    } else {
        m_detailTitle->setText(I18n::selectGamePrompt());
    }

    updateStatusBarStats();
}

void RomBrowserDialog::ensureLoaded() {
    if (m_isLoaded) return;

    QString cacheFile = GameCache::defaultCachePath();
    std::vector<GameEntry> games;

    if (GameCache::exists(cacheFile) && GameCache::load(cacheFile, games) && !games.empty()) {
        m_tableModel->setGames(games);
        m_tableView->sortByColumn(GameTableModel::ColDescription, Qt::AscendingOrder);
        if (m_proxyModel->rowCount() > 0) {
            m_tableView->selectRow(0);
        }
        updateStatusBarStats();
        m_isLoaded = true;
    } else {
        // キャッシュが存在しない場合は m1.xml をパースして自動スキャンを開始
        loadXml();
        m_isLoaded = true;
        onRescanClicked();
    }
}

void RomBrowserDialog::loadXml() {
    std::string xmlPath = "m1.xml";
    if (!QFile::exists(QString::fromStdString(xmlPath))) {
        QMessageBox::critical(this, I18n::msgXmlNotFoundTitle(), I18n::msgXmlNotFoundText());
        return;
    }

    std::vector<GameEntry> games;
    std::string err;
    if (!M1XmlParser::parse(xmlPath, games, err)) {
        QMessageBox::critical(this, I18n::msgXmlParseErrorTitle(), I18n::msgXmlParseErrorText(QString::fromStdString(err)));
        return;
    }

    m_tableModel->setGames(games);
    m_tableView->sortByColumn(GameTableModel::ColDescription, Qt::AscendingOrder);
    if (m_proxyModel->rowCount() > 0) {
        m_tableView->selectRow(0);
    }
    updateStatusBarStats();
}

void RomBrowserDialog::onSearchTextChanged(const QString &text) {
    m_proxyModel->setSearchText(text);
}

void RomBrowserDialog::onStatusFilterChanged(int index) {
    auto filter = static_cast<GameFilterProxyModel::StatusFilter>(m_filterCombo->itemData(index).toInt());
    m_proxyModel->setStatusFilter(filter);
}

void RomBrowserDialog::onGameSelectionChanged(const QModelIndex &current, const QModelIndex &previous) {
    Q_UNUSED(previous);
    if (!current.isValid()) return;

    QModelIndex srcIdx = m_proxyModel->mapToSource(current);
    const GameEntry &game = m_tableModel->getGame(srcIdx.row());
    updateDetailPanel(game);
}

void RomBrowserDialog::onGameDoubleClicked(const QModelIndex &index) {
    Q_UNUSED(index);
    onOkPlayClicked();
}

void RomBrowserDialog::onOkPlayClicked() {
    QModelIndex current = m_tableView->currentIndex();
    if (!current.isValid()) {
        QMessageBox::information(this, I18n::msgSelectGameTitle(), I18n::msgSelectGameText());
        return;
    }

    QModelIndex srcIdx = m_proxyModel->mapToSource(current);
    m_selectedGame = m_tableModel->getGame(srcIdx.row());
    accept();
}

void RomBrowserDialog::onRescanClicked() {
    if (m_verifier->isRunning()) return;

    m_rescanBtn->setEnabled(false);
    m_progressBar->setValue(0);
    m_progressBar->setMaximum(static_cast<int>(m_tableModel->games().size()));
    m_progressBar->setVisible(true);

    // スキャン中の過負荷・再ソートを防ぐため動的ソートを一時無効化
    m_proxyModel->setDynamicSortFilter(false);

    m_verifier->startVerification(m_tableModel->games(), m_romPaths);
}

void RomBrowserDialog::onBatchVerified(const std::vector<VerifiedItem> &items) {
    m_tableModel->updateBatch(items);

    // 選択中の行が含まれていれば詳細パネルも更新
    QModelIndex current = m_tableView->currentIndex();
    if (current.isValid()) {
        QModelIndex srcIdx = m_proxyModel->mapToSource(current);
        for (const auto &it : items) {
            if (it.index == srcIdx.row()) {
                updateDetailPanel(it.game);
                break;
            }
        }
    }
}

void RomBrowserDialog::onVerificationProgress(int current, int total) {
    Q_UNUSED(total);
    m_progressBar->setValue(current);
}

void RomBrowserDialog::onVerificationFinished() {
    m_progressBar->setVisible(false);
    m_rescanBtn->setEnabled(true);

    // ソート・フィルタを再有効化して最新状態を反映
    m_proxyModel->setDynamicSortFilter(true);
    m_proxyModel->invalidate();

    int sortCol = m_tableView->horizontalHeader()->sortIndicatorSection();
    Qt::SortOrder sortOrder = m_tableView->horizontalHeader()->sortIndicatorOrder();
    m_tableView->sortByColumn(sortCol, sortOrder);

    // 未選択なら先頭行を選択
    if (!m_tableView->currentIndex().isValid() && m_proxyModel->rowCount() > 0) {
        m_tableView->selectRow(0);
    }

    updateStatusBarStats();

    // スキャン結果をキャッシュファイルに保存！
    GameCache::save(GameCache::defaultCachePath(), m_tableModel->games());
}

void RomBrowserDialog::updateDetailPanel(const GameEntry &game) {
    m_detailTitle->setText(QString::fromStdString(game.description));

    // ステータスバッジ
    QString badgeStyle = "padding: 4px 8px; border-radius: 4px; font-weight: bold; ";
    switch (game.status) {
        case GameStatus::Ok:
            badgeStyle += "background-color: #c8e6c9; color: #2e7d32;";
            break;
        case GameStatus::Missing:
            badgeStyle += "background-color: #eeeeee; color: #616161;";
            break;
        case GameStatus::Incomplete:
            badgeStyle += "background-color: #e1bee7; color: #6a1b9a;";
            break;
        case GameStatus::CrcMismatch:
            badgeStyle += "background-color: #fff9c4; color: #f57f17;";
            break;
        case GameStatus::Sha1Mismatch:
            badgeStyle += "background-color: #ffe0b2; color: #e65100;";
            break;
        case GameStatus::BothMismatch:
            badgeStyle += "background-color: #ffcdd2; color: #c62828;";
            break;
        default:
            badgeStyle += "background-color: #e0e0e0; color: #333333;";
            break;
    }
    m_detailStatusBadge->setStyleSheet(badgeStyle);
    m_detailStatusBadge->setText(I18n::gameStatusToString(game.status));

    // 基本情報テキスト
    QString infoText;
    infoText += QString("<b>%1</b> %2<br>").arg(I18n::detailRomName(), QString::fromStdString(game.name));
    if (!game.parentZip.empty()) {
        infoText += QString("<b>%1</b> %2<br>").arg(I18n::detailParent(), QString::fromStdString(game.parentZip));
    }
    infoText += QString("<b>%1</b> %2 / <b>%3</b> %4<br>")
                    .arg(I18n::detailYear(), QString::fromStdString(game.year),
                         I18n::detailManufacturer(), QString::fromStdString(game.manufacturer));
    infoText += QString("<b>%1</b> %2<br>").arg(I18n::detailBoard(), QString::fromStdString(game.board));
    infoText += I18n::detailTrackRange(game.minCmd, game.maxCmd, game.defaultCmd);
    if (!game.zipPath.empty()) {
        infoText += QString("<b>%1</b> <small>%2</small>").arg(I18n::detailPath(), QString::fromStdString(game.zipPath));
    }
    m_detailInfo->setText(infoText);

    // 個別 ROM リスト
    m_romTreeWidget->clear();
    for (const auto &rom : game.roms) {
        QTreeWidgetItem *item = new QTreeWidgetItem(m_romTreeWidget);
        item->setText(0, QString::fromStdString(rom.name));

        QString statusStr = I18n::romCheckStateToString(rom.checkState);
        item->setText(1, statusStr);

        // 色付け
        switch (rom.checkState) {
            case RomCheckState::Ok:
                item->setForeground(1, QColor(46, 125, 50));
                break;
            case RomCheckState::Missing:
                item->setForeground(1, QColor(198, 40, 40));
                break;
            case RomCheckState::CrcMismatch:
                item->setForeground(1, QColor(245, 127, 23));
                break;
            case RomCheckState::Sha1Mismatch:
                item->setForeground(1, QColor(230, 81, 0));
                break;
            case RomCheckState::BothMismatch:
                item->setForeground(1, QColor(198, 40, 40));
                break;
            default:
                break;
        }

        // 期待 CRC vs 実際 CRC
        item->setText(2, QString("%1").arg(rom.crc, 8, 16, QChar('0')));
        if (rom.found) {
            item->setText(3, QString("%1").arg(rom.actualCrc, 8, 16, QChar('0')));
            if (rom.crc != 0 && rom.actualCrc != rom.crc) {
                item->setForeground(3, QColor(198, 40, 40));
            }
        } else {
            item->setText(3, "-");
        }

        // 期待 SHA1 vs 実際 SHA1
        item->setText(4, QString::fromStdString(rom.sha1).left(10) + "...");
        item->setToolTip(4, QString::fromStdString(rom.sha1));

        if (rom.found && !rom.actualSha1.empty()) {
            item->setText(5, QString::fromStdString(rom.actualSha1).left(10) + "...");
            item->setToolTip(5, QString::fromStdString(rom.actualSha1));
            if (!rom.sha1.empty() && rom.actualSha1 != rom.sha1) {
                item->setForeground(5, QColor(198, 40, 40));
            }
        } else {
            item->setText(5, "-");
        }

        item->setText(6, QString::number(rom.size) + " B");
    }
}

void RomBrowserDialog::updateStatusBarStats() {
    int total = static_cast<int>(m_tableModel->games().size());
    int ok = 0;
    int missing = 0;
    int crcMismatch = 0;
    int sha1Mismatch = 0;
    int bothMismatch = 0;
    int incomplete = 0;

    for (const auto &g : m_tableModel->games()) {
        switch (g.status) {
            case GameStatus::Ok: ok++; break;
            case GameStatus::Missing: missing++; break;
            case GameStatus::CrcMismatch: crcMismatch++; break;
            case GameStatus::Sha1Mismatch: sha1Mismatch++; break;
            case GameStatus::BothMismatch: bothMismatch++; break;
            case GameStatus::Incomplete: incomplete++; break;
            default: break;
        }
    }

    int available = total - missing;
    m_statsLabel->setText(I18n::statsString(total, available, ok, crcMismatch, sha1Mismatch, bothMismatch, incomplete, missing));
}
