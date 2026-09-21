#include "track_list.h"
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QRegularExpression>
#include <QCoreApplication>

TrackListData TrackListParser::load(const QString &romName, int minCmd, int maxCmd, int defaultCmd, const QString &language) {
    TrackListData data;
    data.romName = romName;
    data.defaultSong = defaultCmd;

    // 探索するファイルパスの候補 (言語設定に応じて優先順位を切り替え)
    QStringList candidates;
    if (language == "jp") {
        candidates = {
            QString("lists/jp/%1.lst").arg(romName),
            QCoreApplication::applicationDirPath() + QString("/lists/jp/%1.lst").arg(romName),
            QString("lists/en/%1.lst").arg(romName),
            QCoreApplication::applicationDirPath() + QString("/lists/en/%1.lst").arg(romName),
            QString("lists/%1.lst").arg(romName),
            QCoreApplication::applicationDirPath() + QString("/lists/%1.lst").arg(romName)
        };
    } else {
        candidates = {
            QString("lists/en/%1.lst").arg(romName),
            QCoreApplication::applicationDirPath() + QString("/lists/en/%1.lst").arg(romName),
            QString("lists/jp/%1.lst").arg(romName),
            QCoreApplication::applicationDirPath() + QString("/lists/jp/%1.lst").arg(romName),
            QString("lists/%1.lst").arg(romName),
            QCoreApplication::applicationDirPath() + QString("/lists/%1.lst").arg(romName)
        };
    }

    QString targetPath;
    for (const QString &path : candidates) {
        if (QFile::exists(path)) {
            targetPath = path;
            break;
        }
    }

    if (!targetPath.isEmpty()) {
        QFile file(targetPath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            in.setEncoding(QStringConverter::Utf8);

            static QRegularExpression rxDefault("^\\$default=(\\d+)");
            static QRegularExpression rxFixedVol("^\\$fixed_volume=(\\d+)");
            static QRegularExpression rxTrack("^#(\\d+)\\s*(.*?)(?:\\s*<time=\"([^\"]*)\">)?$");

            while (!in.atEnd()) {
                QString line = in.readLine().trimmed();
                if (line.isEmpty() || line.startsWith("//")) {
                    continue;
                }

                if (line.startsWith("$end", Qt::CaseInsensitive)) {
                    break;
                }

                auto matchDef = rxDefault.match(line);
                if (matchDef.hasMatch()) {
                    data.defaultSong = matchDef.captured(1).toInt();
                    continue;
                }

                auto matchVol = rxFixedVol.match(line);
                if (matchVol.hasMatch()) {
                    data.fixedVolume = matchVol.captured(1).toInt();
                    continue;
                }

                if (line.startsWith("---")) {
                    TrackItem sep;
                    sep.isSeparator = true;
                    sep.title = "------------------------------------------------------";
                    data.items.push_back(sep);
                    continue;
                }

                auto matchTrk = rxTrack.match(line);
                if (matchTrk.hasMatch()) {
                    TrackItem item;
                    item.songNum = matchTrk.captured(1).toInt();
                    QString title = matchTrk.captured(2).trimmed();
                    title.replace("&lt;", "<").replace("&gt;", ">").replace("&amp;", "&");
                    item.title = title;
                    QString rawTime = matchTrk.captured(3).trimmed();
                    if (rawTime.startsWith("00:")) {
                        rawTime = rawTime.mid(1); // "0:48"
                    } else if (rawTime.startsWith("0") && rawTime.length() > 3 && rawTime[2] == ':') {
                        rawTime = rawTime.mid(1);
                    }
                    item.time = rawTime;
                    data.items.push_back(item);
                }
            }
            return data;
        }
    }

    // .lst が見つからない場合は minCmd 〜 maxCmd から曲番号リストを生成
    if (minCmd > 0 && maxCmd >= minCmd) {
        for (int cmd = minCmd; cmd <= maxCmd; ++cmd) {
            TrackItem item;
            item.songNum = cmd;
            item.title = QString("Track #%1").arg(cmd);
            data.items.push_back(item);
        }
    }

    return data;
}
