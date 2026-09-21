#include "game_table_model.h"
#include "i18n.h"
#include <QColor>
#include <QFont>

GameTableModel::GameTableModel(QObject *parent) : QAbstractTableModel(parent) {}

int GameTableModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(m_games.size());
}

int GameTableModel::columnCount(const QModelIndex &parent) const {
    if (parent.isValid()) return 0;
    return ColCount;
}

QVariant GameTableModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_games.size())) {
        return QVariant();
    }

    const auto &game = m_games[index.row()];
    int col = index.column();

    if (role == Qt::DisplayRole) {
        switch (col) {
            case ColStatus:
                return I18n::gameStatusToBadge(game.status);
            case ColDescription:
                return QString::fromStdString(game.description);
            case ColYear:
                return QString::fromStdString(game.year);
            case ColManufacturer:
                return QString::fromStdString(game.manufacturer);
            case ColRomName:
                return QString::fromStdString(game.name);
            case ColBoard:
                return QString::fromStdString(game.board);
            case ColRomCount: {
                int found = 0;
                for (const auto &r : game.roms) {
                    if (r.found) found++;
                }
                return QString("%1 / %2").arg(found).arg(game.roms.size());
            }
        }
    } else if (role == Qt::ForegroundRole) {
        if (col == ColStatus) {
            switch (game.status) {
                case GameStatus::Ok:
                    return QColor(46, 125, 50); // 緑
                case GameStatus::Missing:
                    return QColor(120, 120, 120); // グレー
                case GameStatus::Incomplete:
                    return QColor(123, 31, 162); // 紫
                case GameStatus::CrcMismatch:
                    return QColor(245, 127, 23); // 黄
                case GameStatus::Sha1Mismatch:
                    return QColor(230, 81, 0);  // 橙
                case GameStatus::BothMismatch:
                    return QColor(198, 40, 40);  // 赤
                case GameStatus::Checking:
                    return QColor(21, 101, 192); // 青
                case GameStatus::Unchecked:
                    return QColor(158, 158, 158);
            }
        }
    } else if (role == Qt::FontRole) {
        if (col == ColStatus || col == ColDescription) {
            QFont font;
            font.setBold(true);
            return font;
        }
    } else if (role == Qt::UserRole) {
        // ソート用の内部値
        switch (col) {
            case ColStatus:
                return static_cast<int>(game.status);
            case ColDescription:
                return QString::fromStdString(game.description);
            case ColYear:
                return QString::fromStdString(game.year);
            case ColManufacturer:
                return QString::fromStdString(game.manufacturer);
            case ColRomName:
                return QString::fromStdString(game.name);
            case ColBoard:
                return QString::fromStdString(game.board);
            case ColRomCount:
                return static_cast<int>(game.roms.size());
        }
    }

    return QVariant();
}

QVariant GameTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        return I18n::tableHeader(section);
    }
    return QVariant();
}

void GameTableModel::refreshTranslations() {
    emit headerDataChanged(Qt::Horizontal, 0, ColCount - 1);
    if (!m_games.empty()) {
        emit dataChanged(createIndex(0, ColStatus), createIndex(static_cast<int>(m_games.size()) - 1, ColStatus));
    }
}

void GameTableModel::setGames(const std::vector<GameEntry> &games) {
    beginResetModel();
    m_games = games;
    endResetModel();
}

void GameTableModel::updateGame(int index, const GameEntry &game) {
    if (index >= 0 && index < static_cast<int>(m_games.size())) {
        m_games[index] = game;
        QModelIndex topLeft = createIndex(index, 0);
        QModelIndex bottomRight = createIndex(index, ColCount - 1);
        emit dataChanged(topLeft, bottomRight);
    }
}

const GameEntry &GameTableModel::getGame(int index) const {
    return m_games.at(index);
}

void GameTableModel::updateBatch(const std::vector<VerifiedItem> &items) {
    if (items.empty()) return;
    int minRow = static_cast<int>(m_games.size());
    int maxRow = 0;
    for (const auto &it : items) {
        if (it.index >= 0 && it.index < static_cast<int>(m_games.size())) {
            m_games[it.index] = it.game;
            if (it.index < minRow) minRow = it.index;
            if (it.index > maxRow) maxRow = it.index;
        }
    }
    if (minRow <= maxRow) {
        emit dataChanged(createIndex(minRow, 0), createIndex(maxRow, ColCount - 1));
    }
}
