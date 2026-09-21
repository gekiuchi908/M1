#pragma once

#include <QString>
#include <vector>

struct TrackItem {
    int songNum = -1;
    QString title;
    QString time;
    bool isSeparator = false;
};

struct TrackListData {
    QString romName;
    int defaultSong = -1;
    int fixedVolume = -1; // -1 の場合は未定義 ("---")
    std::vector<TrackItem> items;
};

class TrackListParser {
public:
    // rom名と minCmd/maxCmd からトラックリストをロード (language: "en" または "jp")
    static TrackListData load(const QString &romName, int minCmd = 1, int maxCmd = 0, int defaultCmd = 1, const QString &language = "en");
};
