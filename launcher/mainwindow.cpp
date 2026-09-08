#include "mainwindow.h"
#include "xml_parser.h"
#include "settings_dialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QStatusBar>
#include <QMessageBox>
#include <QSettings>
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
// MainWindow
// ==========================================

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_tableModel(new GameTableModel(this)),
      m_proxyModel(new GameFilterProxyModel(this)),
      m_verifier(new RomVerifier(this)),
      m_m1Process(new M1Process(this))
{
    setWindowTitle("M1 Sound Launcher (Linux Native)");
    resize(1150, 780);

    setupUi();
    loadConfiguration();
    loadXml();

    // 検証バッチシグナル
    connect(m_verifier, &RomVerifier::batchVerified, this, &MainWindow::onBatchVerified);
    connect(m_verifier, &RomVerifier::progress, this, &MainWindow::onVerificationProgress);
    connect(m_verifier, &RomVerifier::finished, this, &MainWindow::onVerificationFinished);

    // M1 プロセスシグナル
    connect(m_m1Process, &M1Process::started, this, &MainWindow::onM1Started);
    connect(m_m1Process, &M1Process::finished, this, &MainWindow::onM1Finished);
    connect(m_m1Process, &M1Process::logOutput, this, &MainWindow::onM1LogOutput);
    connect(m_m1Process, &M1Process::songChanged, this, &MainWindow::onM1SongChanged);
    connect(m_m1Process, &M1Process::errorOccurred, this, &MainWindow::onM1ErrorOccurred);

    // 初期スキャン開始
    onRescanClicked();
}

MainWindow::~MainWindow() {
    saveConfiguration();
}

void MainWindow::setupUi() {
    QWidget *central = new QWidget(this);
    setCentralWidget(central);
    QVBoxLayout *rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(8, 8, 8, 8);
    rootLayout->setSpacing(6);

    // 1. トップツールバー
    QHBoxLayout *topBar = new QHBoxLayout();
    
    QLabel *searchLabel = new QLabel("検索:", this);
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("ゲーム名、ROM名、メーカ、基板...");
    m_searchEdit->setClearButtonEnabled(true);
    topBar->addWidget(searchLabel);
    topBar->addWidget(m_searchEdit, 2);

    QLabel *filterLabel = new QLabel("表示:", this);
    m_filterCombo = new QComboBox(this);
    m_filterCombo->addItem("すべてのゲーム", GameFilterProxyModel::FilterAll);
    m_filterCombo->addItem("所持ゲーム (完全一致 + 不一致)", GameFilterProxyModel::FilterAvailable);
    m_filterCombo->addItem("完全一致 (OK) のみ", GameFilterProxyModel::FilterOkOnly);
    m_filterCombo->addItem("不一致 / エラーのみ", GameFilterProxyModel::FilterMismatched);
    m_filterCombo->addItem("未所持のみ", GameFilterProxyModel::FilterMissingOnly);
    topBar->addWidget(filterLabel);
    topBar->addWidget(m_filterCombo, 1);

    m_rescanBtn = new QPushButton("🔄 ROM再スキャン", this);
    m_settingsBtn = new QPushButton("⚙ 設定", this);
    topBar->addWidget(m_rescanBtn);
    topBar->addWidget(m_settingsBtn);

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

    m_detailTitle = new QLabel("ゲームを選択してください", this);
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

    QLabel *romListLabel = new QLabel("個別 ROM 検証結果 (CRC / SHA1 照合):", this);
    romListLabel->setStyleSheet("font-weight: bold;");
    romLayout->addWidget(romListLabel);

    m_romTreeWidget = new QTreeWidget(this);
    m_romTreeWidget->setHeaderLabels(QStringList() << "ROMファイル" << "状態" << "期待 CRC" << "実測 CRC" << "期待 SHA1" << "実測 SHA1" << "サイズ");
    m_romTreeWidget->setRootIsDecorated(false);
    m_romTreeWidget->setAlternatingRowColors(true);
    m_romTreeWidget->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_romTreeWidget->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    romLayout->addWidget(m_romTreeWidget);

    detailSplitter->addWidget(romPanel);
    detailSplitter->setStretchFactor(0, 1);
    detailSplitter->setStretchFactor(1, 3);

    bottomLayout->addWidget(detailSplitter);

    // 4. 再生コントロールバー (曲送り・一時停止ボタン群)
    QHBoxLayout *ctrlLayout = new QHBoxLayout();
    
    m_playBtn = new QPushButton("▶ 再生", this);
    m_playBtn->setStyleSheet("font-weight: bold; padding: 6px 14px; background-color: #2e7d32; color: white;");

    m_stopBtn = new QPushButton("⏹ 停止", this);
    m_stopBtn->setStyleSheet("font-weight: bold; padding: 6px 14px;");
    m_stopBtn->setEnabled(false);

    m_prevBtn = new QPushButton("⏮ 前の曲 (-)", this);
    m_prevBtn->setToolTip("前の曲へ (- キー)");
    m_prevBtn->setEnabled(false);

    m_pauseBtn = new QPushButton("⏸ 一時停止 (Space)", this);
    m_pauseBtn->setToolTip("一時停止 / 再開 (Space キー)");
    m_pauseBtn->setEnabled(false);

    m_nextBtn = new QPushButton("⏭ 次の曲 (+)", this);
    m_nextBtn->setToolTip("次の曲へ (+ キー)");
    m_nextBtn->setEnabled(false);

    m_restartBtn = new QPushButton("🔄 リスタート (0)", this);
    m_restartBtn->setToolTip("現在の曲を先頭からリスタート (0 キー)");
    m_restartBtn->setEnabled(false);

    m_playStatusLabel = new QLabel("停止中", this);
    m_playStatusLabel->setStyleSheet("color: #555; font-weight: bold; margin-left: 10px;");

    m_toggleLogBtn = new QPushButton("ログ表示 ▾", this);

    ctrlLayout->addWidget(m_playBtn);
    ctrlLayout->addWidget(m_stopBtn);
    ctrlLayout->addWidget(m_prevBtn);
    ctrlLayout->addWidget(m_pauseBtn);
    ctrlLayout->addWidget(m_nextBtn);
    ctrlLayout->addWidget(m_restartBtn);
    ctrlLayout->addWidget(m_playStatusLabel);
    ctrlLayout->addStretch();
    ctrlLayout->addWidget(m_toggleLogBtn);

    bottomLayout->addLayout(ctrlLayout);

    // M1 ログエリア (初期非表示)
    m_logView = new QTextEdit(this);
    m_logView->setReadOnly(true);
    m_logView->setMaximumHeight(100);
    m_logView->setVisible(false);
    bottomLayout->addWidget(m_logView);

    mainSplitter->addWidget(bottomWidget);
    mainSplitter->setStretchFactor(0, 3);
    mainSplitter->setStretchFactor(1, 2);

    rootLayout->addWidget(mainSplitter);

    // 5. ステータスバー
    m_progressBar = new QProgressBar(this);
    m_progressBar->setMaximumWidth(200);
    m_progressBar->setVisible(false);
    statusBar()->addPermanentWidget(m_progressBar);

    m_statsLabel = new QLabel(this);
    statusBar()->addPermanentWidget(m_statsLabel);

    // シグナル接続
    connect(m_searchEdit, &QLineEdit::textChanged, this, &MainWindow::onSearchTextChanged);
    connect(m_filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onStatusFilterChanged);
    connect(m_tableView->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &MainWindow::onGameSelectionChanged);
    connect(m_tableView, &QTableView::doubleClicked, this, &MainWindow::onGameDoubleClicked);
    connect(m_rescanBtn, &QPushButton::clicked, this, &MainWindow::onRescanClicked);
    connect(m_settingsBtn, &QPushButton::clicked, this, &MainWindow::onSettingsClicked);

    // プレイヤーボタン
    connect(m_playBtn, &QPushButton::clicked, this, &MainWindow::onPlayClicked);
    connect(m_stopBtn, &QPushButton::clicked, this, &MainWindow::onStopClicked);
    connect(m_prevBtn, &QPushButton::clicked, this, &MainWindow::onPrevTrackClicked);
    connect(m_nextBtn, &QPushButton::clicked, this, &MainWindow::onNextTrackClicked);
    connect(m_pauseBtn, &QPushButton::clicked, this, &MainWindow::onPauseToggleClicked);
    connect(m_restartBtn, &QPushButton::clicked, this, &MainWindow::onRestartTrackClicked);

    connect(m_toggleLogBtn, &QPushButton::clicked, this, &MainWindow::onToggleLogClicked);
}

static QString getLauncherIniPath() {
    return QCoreApplication::applicationDirPath() + "/m1-launcher.ini";
}

void MainWindow::loadConfiguration() {
    QString configPath = getLauncherIniPath();
    QSettings settings(configPath, QSettings::IniFormat);
    QString defaultRomPath = "./roms";

    // m1.ini があれば初期値として読み込む
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

    QString romDir = settings.value("romPath", defaultRomPath).toString();
    m_m1BinPath = settings.value("m1BinPath", "./m1").toString();
    m_soundDriver = settings.value("soundDriver", 3).toInt(); // デフォルトを PulseAudio (3) に設定
    m_sampleRate = settings.value("sampleRate", 44100).toInt();
    m_useTerminal = settings.value("useTerminal", false).toBool();

    m_romPaths.clear();
    m_romPaths.push_back(romDir.toStdString());
    if (romDir != "./roms") {
        m_romPaths.push_back("./roms");
    }
}

void MainWindow::saveConfiguration() {
    QString configPath = getLauncherIniPath();
    QSettings settings(configPath, QSettings::IniFormat);

    if (!m_romPaths.empty()) {
        settings.setValue("romPath", QString::fromStdString(m_romPaths[0]));
    }
    settings.setValue("m1BinPath", m_m1BinPath);
    settings.setValue("soundDriver", m_soundDriver);
    settings.setValue("sampleRate", m_sampleRate);
    settings.setValue("useTerminal", m_useTerminal);
    settings.sync();
}

void MainWindow::loadXml() {
    std::string xmlPath = "m1.xml";
    if (!QFile::exists(QString::fromStdString(xmlPath))) {
        QMessageBox::critical(this, "エラー", "m1.xml が見つかりません。M1 の実行ディレクトリで起動してください。");
        return;
    }

    std::vector<GameEntry> games;
    std::string err;
    if (!M1XmlParser::parse(xmlPath, games, err)) {
        QMessageBox::critical(this, "エラー", "m1.xml のパースに失敗しました:\n" + QString::fromStdString(err));
        return;
    }

    m_tableModel->setGames(games);
    m_tableView->sortByColumn(GameTableModel::ColDescription, Qt::AscendingOrder);
    if (m_proxyModel->rowCount() > 0) {
        m_tableView->selectRow(0);
    }
    updateStatusBarStats();
}

void MainWindow::onSearchTextChanged(const QString &text) {
    m_proxyModel->setSearchText(text);
}

void MainWindow::onStatusFilterChanged(int index) {
    auto filter = static_cast<GameFilterProxyModel::StatusFilter>(m_filterCombo->itemData(index).toInt());
    m_proxyModel->setStatusFilter(filter);
}

void MainWindow::onGameSelectionChanged(const QModelIndex &current, const QModelIndex &previous) {
    Q_UNUSED(previous);
    if (!current.isValid()) return;

    QModelIndex srcIdx = m_proxyModel->mapToSource(current);
    const GameEntry &game = m_tableModel->getGame(srcIdx.row());
    updateDetailPanel(game);
}

void MainWindow::onGameDoubleClicked(const QModelIndex &index) {
    Q_UNUSED(index);
    onPlayClicked();
}

void MainWindow::onRescanClicked() {
    if (m_verifier->isRunning()) return;

    m_rescanBtn->setEnabled(false);
    m_progressBar->setValue(0);
    m_progressBar->setMaximum(static_cast<int>(m_tableModel->games().size()));
    m_progressBar->setVisible(true);

    // スキャン中の過負荷・再ソートを防ぐため動的ソートを一時無効化
    m_proxyModel->setDynamicSortFilter(false);

    m_verifier->startVerification(m_tableModel->games(), m_romPaths);
}

void MainWindow::onBatchVerified(const std::vector<VerifiedItem> &items) {
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

void MainWindow::onVerificationProgress(int current, int total) {
    Q_UNUSED(total);
    m_progressBar->setValue(current);
}

void MainWindow::onVerificationFinished() {
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
}

void MainWindow::updateDetailPanel(const GameEntry &game) {
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
    m_detailStatusBadge->setText(QString::fromUtf8(gameStatusToString(game.status)));

    // 基本情報テキスト
    QString infoText;
    infoText += QString("<b>ROM名:</b> %1<br>").arg(QString::fromStdString(game.name));
    if (!game.parentZip.empty()) {
        infoText += QString("<b>親セット:</b> %1<br>").arg(QString::fromStdString(game.parentZip));
    }
    infoText += QString("<b>発売年:</b> %1 / <b>メーカ:</b> %2<br>").arg(QString::fromStdString(game.year), QString::fromStdString(game.manufacturer));
    infoText += QString("<b>基板:</b> %1<br>").arg(QString::fromStdString(game.board));
    infoText += QString("<b>曲番号範囲:</b> %1 〜 %2 (デフォルト: %3)<br>")
                    .arg(game.minCmd).arg(game.maxCmd).arg(game.defaultCmd);
    if (!game.zipPath.empty()) {
        infoText += QString("<b>パス:</b> <small>%1</small>").arg(QString::fromStdString(game.zipPath));
    }
    m_detailInfo->setText(infoText);

    // 個別 ROM リスト
    m_romTreeWidget->clear();
    for (const auto &rom : game.roms) {
        QTreeWidgetItem *item = new QTreeWidgetItem(m_romTreeWidget);
        item->setText(0, QString::fromStdString(rom.name));

        QString statusStr = QString::fromUtf8(romCheckStateToString(rom.checkState));
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

void MainWindow::updateStatusBarStats() {
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
    QString stats = QString("総数: %1 | 所持: %2 (🟢完全一致: %3 | 🟡CRC不一致: %4 | 🟠SHA1不一致: %5 | 🔴両方不一致: %6 | 🟣一部不足: %7) | 未所持: %8")
                        .arg(total).arg(available).arg(ok).arg(crcMismatch).arg(sha1Mismatch).arg(bothMismatch).arg(incomplete).arg(missing);
    m_statsLabel->setText(stats);
}

void MainWindow::onPlayClicked() {
    QModelIndex current = m_tableView->currentIndex();
    if (!current.isValid()) {
        QMessageBox::information(this, "案内", "リストからゲームを選択してください。");
        return;
    }

    QModelIndex srcIdx = m_proxyModel->mapToSource(current);
    const GameEntry &game = m_tableModel->getGame(srcIdx.row());

    QStringList extraArgs;
    if (m_soundDriver >= 0) {
        extraArgs << QString("-s%1").arg(m_soundDriver);
    }
    if (m_sampleRate > 0) {
        extraArgs << QString("-r%1").arg(m_sampleRate);
    }

    m_logView->clear();
    m_isPaused = false;
    m_pauseBtn->setText("⏸ 一時停止 (Space)");

    if (m_useTerminal) {
        m_m1Process->playInTerminal(m_m1BinPath, QString::fromStdString(game.name), extraArgs);
    } else {
        m_m1Process->play(m_m1BinPath, QString::fromStdString(game.name), extraArgs);
    }
}

void MainWindow::onStopClicked() {
    m_m1Process->stop();
    onM1Finished(0);
}

void MainWindow::onPrevTrackClicked() {
    m_m1Process->prevTrack();
}

void MainWindow::onNextTrackClicked() {
    m_m1Process->nextTrack();
}

void MainWindow::onPauseToggleClicked() {
    m_m1Process->togglePause();
    m_isPaused = !m_isPaused;
    m_pauseBtn->setText(m_isPaused ? "▶ 再開 (Space)" : "⏸ 一時停止 (Space)");
}

void MainWindow::onRestartTrackClicked() {
    m_m1Process->restartTrack();
}

void MainWindow::keyPressEvent(QKeyEvent *event) {
    // 検索ボックスにフォーカスがある場合はキーボードショートカットを横取りしない
    if (m_searchEdit->hasFocus()) {
        QMainWindow::keyPressEvent(event);
        return;
    }

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

void MainWindow::onSettingsClicked() {
    SettingsDialog dlg(this);
    if (!m_romPaths.empty()) {
        dlg.setRomPath(QString::fromStdString(m_romPaths[0]));
    }
    dlg.setM1BinPath(m_m1BinPath);
    dlg.setSoundDriver(m_soundDriver);
    dlg.setSampleRate(m_sampleRate);
    dlg.setUseTerminal(m_useTerminal);

    if (dlg.exec() == QDialog::Accepted) {
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
        onRescanClicked();
    }
}

void MainWindow::onToggleLogClicked() {
    bool vis = !m_logView->isVisible();
    m_logView->setVisible(vis);
    m_toggleLogBtn->setText(vis ? "ログ非表示 ▴" : "ログ表示 ▾");
}

void MainWindow::onM1Started(const QString &romName) {
    m_playBtn->setEnabled(false);
    m_stopBtn->setEnabled(true);
    m_prevBtn->setEnabled(!m_useTerminal);
    m_nextBtn->setEnabled(!m_useTerminal);
    m_pauseBtn->setEnabled(!m_useTerminal);
    m_restartBtn->setEnabled(!m_useTerminal);

    m_playStatusLabel->setText(QString("▶ 再生中: %1").arg(romName));
    m_playStatusLabel->setStyleSheet("color: #2e7d32; font-weight: bold; margin-left: 10px;");
}

void MainWindow::onM1Finished(int exitCode) {
    Q_UNUSED(exitCode);
    m_playBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
    m_prevBtn->setEnabled(false);
    m_nextBtn->setEnabled(false);
    m_pauseBtn->setEnabled(false);
    m_restartBtn->setEnabled(false);

    m_playStatusLabel->setText("停止中");
    m_playStatusLabel->setStyleSheet("color: #555; font-weight: bold; margin-left: 10px;");
}

void MainWindow::onM1LogOutput(const QString &text) {
    m_logView->append(text);
}

void MainWindow::onM1SongChanged(int songNum, const QString &trackName) {
    QString label = QString("▶ 再生中: %1 | 曲 #%2").arg(m_m1Process->currentRom()).arg(songNum);
    if (!trackName.isEmpty()) {
        label += QString(" (%1)").arg(trackName);
    }
    m_playStatusLabel->setText(label);
}

void MainWindow::onM1ErrorOccurred(const QString &error) {
    QMessageBox::warning(this, "M1 エラー", error);
    onM1Finished(-1);
}
