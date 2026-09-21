#pragma once

#include "models.h"
#include <QString>
#include <vector>

class GameCache {
public:
    // キャッシュファイルのデフォルトパスを取得
    static QString defaultCachePath();

    // キャッシュファイルが存在するか判定
    static bool exists(const QString &cachePath = defaultCachePath());

    // キャッシュファイルから読み込み
    static bool load(const QString &cachePath, std::vector<GameEntry> &outGames);

    // キャッシュファイルへ保存
    static bool save(const QString &cachePath, const std::vector<GameEntry> &games);
};
