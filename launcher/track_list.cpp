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

            static QRegularExpression rxDefault("^\\$default=(\\$?[0-9a-fA-F]+)");
            static QRegularExpression rxFixedVol("^\\$fixed_volume=(\\d+)");
            static QRegularExpression rxTrack("^([#$])([0-9a-fA-F]+)\\s+(.*)$");
            static QRegularExpression rxTime("time=\"([^\"]*)\"");
            static QRegularExpression rxV1Time("^(\\d+:\\d+(?:\\.\\d+)?)$");
            static QRegularExpression rxFadeout("fadeout=\"([^\"]*)\"");
            static QRegularExpression rxFadeLen("fadeoutlength=\"([^\"]*)\"");

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
                    QString val = matchDef.captured(1);
                    if (val.startsWith('$')) {
                        data.defaultSong = val.mid(1).toInt(nullptr, 16);
                    } else {
                        data.defaultSong = val.toInt();
                    }
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
                    bool ok = false;
                    int songNum = -1;
                    if (matchTrk.captured(1) == "#") {
                        songNum = matchTrk.captured(2).toInt(&ok, 10);
                    } else {
                        songNum = matchTrk.captured(2).toInt(&ok, 16);
                    }
                    if (!ok) continue;

                    TrackItem item;
                    item.songNum = songNum;

                    QString rest = matchTrk.captured(3).trimmed();
                    int tagStart = rest.indexOf('<');
                    int tagEnd = rest.lastIndexOf('>');

                    QString title;
                    QString rawTime;

                    if (tagStart != -1 && tagEnd > tagStart) {
                        title = rest.left(tagStart).trimmed();
                        QString tagContent = rest.mid(tagStart + 1, tagEnd - tagStart - 1).trimmed();

                        auto matchTime = rxTime.match(tagContent);
                        if (matchTime.hasMatch()) {
                            rawTime = matchTime.captured(1).trimmed();
                        } else {
                            auto matchV1 = rxV1Time.match(tagContent);
                            if (matchV1.hasMatch()) {
                                rawTime = matchV1.captured(1).trimmed();
                            }
                        }

                        auto matchFade = rxFadeout.match(tagContent);
                        if (matchFade.hasMatch()) {
                            QString fo = matchFade.captured(1).trimmed().toLower();
                            item.fadeout = (fo == "yes" || fo == "1" || fo == "true");
                        }

                        auto matchFadeLen = rxFadeLen.match(tagContent);
                        if (matchFadeLen.hasMatch()) {
                            item.fadeoutLength = matchFadeLen.captured(1).trimmed();
                        }
                    } else {
                        title = rest;
                    }

                    title.replace("&lt;", "<").replace("&gt;", ">").replace("&amp;", "&");
                    item.title = title;

                    if (!rawTime.isEmpty()) {
                        if (!rawTime.contains(':')) {
                            // 秒数指定 ("20", "180", "5.67" 等) を mm:ss 形式に正規化
                            bool ok = false;
                            double totalSec = rawTime.toDouble(&ok);
                            if (ok) {
                                int min = (int)totalSec / 60;
                                double sec = totalSec - (min * 60);
                                if (sec == (int)sec) {
                                    rawTime = QString("%1:%2").arg(min).arg((int)sec, 2, 10, QChar('0'));
                                } else {
                                    rawTime = QString("%1:%2").arg(min).arg(sec, 5, 'f', 2, QChar('0'));
                                }
                            }
                        } else {
                            if (rawTime.startsWith("00:")) {
                                rawTime = rawTime.mid(1); // "0:48"
                            } else if (rawTime.startsWith("0") && rawTime.length() > 3 && rawTime[2] == ':') {
                                rawTime = rawTime.mid(1);
                            }
                        }
                    }
                    item.time = rawTime;
                    data.items.push_back(item);
                } else if (line.startsWith('$')) {
                    // $name, $version, $songmax, $main 等のディレクティブ行をスキップ
                    continue;
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
