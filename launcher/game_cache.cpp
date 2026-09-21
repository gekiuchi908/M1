#include "game_cache.h"
#include <QCoreApplication>
#include <QFile>
#include <QDataStream>
#include <QFileInfo>
#include <QDebug>

static const quint32 CACHE_MAGIC = 0x4D314341; // "M1CA"
static const quint32 CACHE_VERSION = 1;

QString GameCache::defaultCachePath() {
    return QCoreApplication::applicationDirPath() + "/m1-launcher-cache.dat";
}

bool GameCache::exists(const QString &cachePath) {
    return QFile::exists(cachePath);
}

bool GameCache::load(const QString &cachePath, std::vector<GameEntry> &outGames) {
    QFile file(cachePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QDataStream in(&file);
    in.setVersion(QDataStream::Qt_6_0);

    quint32 magic = 0;
    quint32 version = 0;
    in >> magic >> version;

    if (magic != CACHE_MAGIC || version != CACHE_VERSION) {
        return false;
    }

    quint32 count = 0;
    in >> count;

    outGames.clear();
    outGames.reserve(count);

    for (quint32 i = 0; i < count; ++i) {
        GameEntry game;
        QString name, desc, year, mfg, board, parentZip, zipPath;
        qint32 defCmd, stopCmd, minCmd, maxCmd, status;

        in >> name >> desc >> year >> mfg >> board >> parentZip >> zipPath;
        in >> defCmd >> stopCmd >> minCmd >> maxCmd >> status;

        game.name = name.toStdString();
        game.description = desc.toStdString();
        game.year = year.toStdString();
        game.manufacturer = mfg.toStdString();
        game.board = board.toStdString();
        game.parentZip = parentZip.toStdString();
        game.zipPath = zipPath.toStdString();
        game.defaultCmd = defCmd;
        game.stopCmd = stopCmd;
        game.minCmd = minCmd;
        game.maxCmd = maxCmd;
        game.status = static_cast<GameStatus>(status);

        quint32 romCount = 0;
        in >> romCount;
        game.roms.reserve(romCount);

        for (quint32 j = 0; j < romCount; ++j) {
            RomInfo rom;
            QString rName, rSha1, rMd5, rRegion, rMatched, rActualSha1;
            quint32 rSize, rCrc, rOffset, rActualCrc, rActualSize;
            bool rFound;
            qint32 rState;

            in >> rName >> rSize >> rCrc >> rSha1 >> rMd5 >> rRegion >> rOffset;
            in >> rFound >> rMatched >> rActualCrc >> rActualSha1 >> rActualSize >> rState;

            rom.name = rName.toStdString();
            rom.size = rSize;
            rom.crc = rCrc;
            rom.sha1 = rSha1.toStdString();
            rom.md5 = rMd5.toStdString();
            rom.region = rRegion.toStdString();
            rom.offset = rOffset;

            rom.found = rFound;
            rom.matchedFileName = rMatched.toStdString();
            rom.actualCrc = rActualCrc;
            rom.actualSha1 = rActualSha1.toStdString();
            rom.actualSize = rActualSize;
            rom.checkState = static_cast<RomCheckState>(rState);

            game.roms.push_back(rom);
        }

        outGames.push_back(game);
    }

    return (in.status() == QDataStream::Ok);
}

bool GameCache::save(const QString &cachePath, const std::vector<GameEntry> &games) {
    QFile file(cachePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    QDataStream out(&file);
    out.setVersion(QDataStream::Qt_6_0);

    out << CACHE_MAGIC;
    out << CACHE_VERSION;
    out << static_cast<quint32>(games.size());

    for (const auto &game : games) {
        out << QString::fromStdString(game.name);
        out << QString::fromStdString(game.description);
        out << QString::fromStdString(game.year);
        out << QString::fromStdString(game.manufacturer);
        out << QString::fromStdString(game.board);
        out << QString::fromStdString(game.parentZip);
        out << QString::fromStdString(game.zipPath);
        out << static_cast<qint32>(game.defaultCmd);
        out << static_cast<qint32>(game.stopCmd);
        out << static_cast<qint32>(game.minCmd);
        out << static_cast<qint32>(game.maxCmd);
        out << static_cast<qint32>(game.status);

        out << static_cast<quint32>(game.roms.size());
        for (const auto &rom : game.roms) {
            out << QString::fromStdString(rom.name);
            out << static_cast<quint32>(rom.size);
            out << static_cast<quint32>(rom.crc);
            out << QString::fromStdString(rom.sha1);
            out << QString::fromStdString(rom.md5);
            out << QString::fromStdString(rom.region);
            out << static_cast<quint32>(rom.offset);

            out << rom.found;
            out << QString::fromStdString(rom.matchedFileName);
            out << static_cast<quint32>(rom.actualCrc);
            out << QString::fromStdString(rom.actualSha1);
            out << static_cast<quint32>(rom.actualSize);
            out << static_cast<qint32>(rom.checkState);
        }
    }

    return (out.status() == QDataStream::Ok);
}
