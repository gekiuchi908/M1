#pragma once

#include "models.h"
#include <QObject>
#include <QString>
#include <vector>
#include <string>

struct VerifiedItem {
    int index;
    GameEntry game;
};

class RomVerifier : public QObject {
    Q_OBJECT

public:
    explicit RomVerifier(QObject *parent = nullptr);
    ~RomVerifier();

    // 1つのゲームを同期検証
    static void verifyGame(GameEntry &game, const std::vector<std::string> &romPaths);

    // 複数ゲームを非同期で検証開始
    void startVerification(std::vector<GameEntry> games, const std::vector<std::string> &romPaths);
    void cancel();
    bool isRunning() const { return m_running; }

signals:
    // バッチ通知で UI の負荷を激減させる
    void batchVerified(const std::vector<VerifiedItem> &items);
    void finished();
    void progress(int current, int total);

private:
    bool m_running = false;
    bool m_cancelled = false;
};

Q_DECLARE_METATYPE(VerifiedItem)
Q_DECLARE_METATYPE(std::vector<VerifiedItem>)
