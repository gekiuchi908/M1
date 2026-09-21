#pragma once

#include "models.h"
#include "rom_verifier.h"
#include <QAbstractTableModel>
#include <vector>

class GameTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column {
        ColStatus = 0,
        ColDescription,
        ColYear,
        ColManufacturer,
        ColRomName,
        ColBoard,
        ColRomCount,
        ColCount
    };

    explicit GameTableModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void setGames(const std::vector<GameEntry> &games);
    void updateGame(int index, const GameEntry &game);
    void updateBatch(const std::vector<VerifiedItem> &items);
    const GameEntry &getGame(int index) const;
    const std::vector<GameEntry> &games() const { return m_games; }
    void refreshTranslations();

private:
    std::vector<GameEntry> m_games;
};
