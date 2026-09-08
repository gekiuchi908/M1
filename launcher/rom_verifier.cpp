#include "rom_verifier.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QCryptographicHash>
#include <QThread>
#include <algorithm>
#include <vector>

extern "C" {
#include "unzip.h"
}

struct ZipEntryDetail {
    std::string name;
    uint32_t crc = 0;
    uint32_t size = 0;
};

// ZIP 内のエントリ一覧を収集
static bool scanZipArchive(const std::string &zipPath, std::vector<ZipEntryDetail> &entries) {
    entries.clear();
    unzFile zip = unzOpen(zipPath.c_str());
    if (!zip) {
        return false;
    }

    if (unzGoToFirstFile(zip) != UNZ_OK) {
        unzClose(zip);
        return false;
    }

    do {
        unz_file_info info;
        char filename[1024];
        if (unzGetCurrentFileInfo(zip, &info, filename, sizeof(filename), nullptr, 0, nullptr, 0) == UNZ_OK) {
            ZipEntryDetail entry;
            entry.name = filename;
            entry.crc = static_cast<uint32_t>(info.crc);
            entry.size = static_cast<uint32_t>(info.uncompressed_size);
            entries.push_back(entry);
        }
    } while (unzGoToNextFile(zip) == UNZ_OK);

    unzClose(zip);
    return true;
}

// 指定した ZIP 内のファイルの SHA1 を計算
static bool computeSha1InZip(const std::string &zipPath, const std::string &filenameInZip, std::string &outSha1) {
    outSha1.clear();
    unzFile zip = unzOpen(zipPath.c_str());
    if (!zip) {
        return false;
    }

    if (unzLocateFile(zip, filenameInZip.c_str(), 0) != UNZ_OK) {
        unzClose(zip);
        return false;
    }

    if (unzOpenCurrentFile(zip) != UNZ_OK) {
        unzClose(zip);
        return false;
    }

    QCryptographicHash hash(QCryptographicHash::Sha1);
    char buffer[65536];
    int bytesRead = 0;
    while ((bytesRead = unzReadCurrentFile(zip, buffer, sizeof(buffer))) > 0) {
        hash.addData(QByteArrayView(buffer, bytesRead));
    }

    unzCloseCurrentFile(zip);
    unzClose(zip);

    outSha1 = hash.result().toHex().toLower().toStdString();
    return true;
}

static std::string toLowerString(const std::string &s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c) { return std::tolower(c); });
    return r;
}

// ROM パス一覧から ZIP を探す
static std::string findZip(const std::string &romName, const std::vector<std::string> &romPaths) {
    for (const auto &path : romPaths) {
        QString fullPath = QString::fromStdString(path) + "/" + QString::fromStdString(romName) + ".zip";
        if (QFile::exists(fullPath)) {
            return fullPath.toStdString();
        }
    }
    return "";
}

RomVerifier::RomVerifier(QObject *parent) : QObject(parent) {
    qRegisterMetaType<VerifiedItem>("VerifiedItem");
    qRegisterMetaType<std::vector<VerifiedItem>>("std::vector<VerifiedItem>");
}

RomVerifier::~RomVerifier() {
    cancel();
}

void RomVerifier::cancel() {
    m_cancelled = true;
}

void RomVerifier::verifyGame(GameEntry &game, const std::vector<std::string> &romPaths) {
    game.zipPath = findZip(game.name, romPaths);

    if (game.zipPath.empty()) {
        game.status = GameStatus::Missing;
        for (auto &r : game.roms) {
            r.found = false;
            r.checkState = RomCheckState::Missing;
        }
        return;
    }

    // ZIP をスキャン
    std::vector<ZipEntryDetail> mainEntries;
    if (!scanZipArchive(game.zipPath, mainEntries)) {
        game.status = GameStatus::Missing;
        return;
    }

    // 親 ZIP のスキャン（必要に応じて）
    std::string parentZipPath;
    std::vector<ZipEntryDetail> parentEntries;
    bool hasParent = false;
    if (!game.parentZip.empty() && game.parentZip != game.name) {
        parentZipPath = findZip(game.parentZip, romPaths);
        if (!parentZipPath.empty()) {
            hasParent = scanZipArchive(parentZipPath, parentEntries);
        }
    }

    // 各 ROM の検証
    for (auto &rom : game.roms) {
        rom.found = false;
        rom.matchedFileName.clear();
        rom.actualCrc = 0;
        rom.actualSha1.clear();
        rom.actualSize = 0;
        rom.checkState = RomCheckState::Unchecked;

        std::string targetZip = game.zipPath;
        const ZipEntryDetail *matched = nullptr;

        std::string lowerRomName = toLowerString(rom.name);

        // 1. メイン ZIP 内で名前照合
        for (const auto &e : mainEntries) {
            if (toLowerString(e.name) == lowerRomName) {
                matched = &e;
                targetZip = game.zipPath;
                break;
            }
        }

        // 2. メイン ZIP 内で CRC 照合 (ファイル名が異なるリビジョン対策)
        if (!matched && rom.crc != 0) {
            for (const auto &e : mainEntries) {
                if (e.crc == rom.crc) {
                    matched = &e;
                    targetZip = game.zipPath;
                    break;
                }
            }
        }

        // 3. 親 ZIP 内を照合
        if (!matched && hasParent) {
            for (const auto &e : parentEntries) {
                if (toLowerString(e.name) == lowerRomName) {
                    matched = &e;
                    targetZip = parentZipPath;
                    break;
                }
            }
            if (!matched && rom.crc != 0) {
                for (const auto &e : parentEntries) {
                    if (e.crc == rom.crc) {
                        matched = &e;
                        targetZip = parentZipPath;
                        break;
                    }
                }
            }
        }

        if (!matched) {
            rom.found = false;
            rom.checkState = RomCheckState::Missing;
            continue;
        }

        rom.found = true;
        rom.matchedFileName = matched->name;
        rom.actualCrc = matched->crc;
        rom.actualSize = matched->size;

        bool crcMatch = (rom.crc == 0 || rom.actualCrc == rom.crc);

        // SHA1 の計算
        bool sha1Match = true;
        if (!rom.sha1.empty()) {
            std::string actualSha1;
            if (computeSha1InZip(targetZip, matched->name, actualSha1)) {
                rom.actualSha1 = actualSha1;
                sha1Match = (toLowerString(actualSha1) == toLowerString(rom.sha1));
            } else {
                sha1Match = false;
            }
        }

        // 個別ステータス判定
        if (crcMatch && sha1Match) {
            rom.checkState = RomCheckState::Ok;
        } else if (!crcMatch && sha1Match) {
            rom.checkState = RomCheckState::CrcMismatch;
        } else if (crcMatch && !sha1Match) {
            rom.checkState = RomCheckState::Sha1Mismatch;
        } else {
            rom.checkState = RomCheckState::BothMismatch;
        }
    }

    // ゲーム全体のステータス集計
    bool hasMissing = false;
    bool hasBoth = false;
    bool hasCrc = false;
    bool hasSha1 = false;

    for (const auto &rom : game.roms) {
        if (rom.checkState == RomCheckState::Missing) {
            hasMissing = true;
        } else if (rom.checkState == RomCheckState::BothMismatch) {
            hasBoth = true;
        } else if (rom.checkState == RomCheckState::CrcMismatch) {
            hasCrc = true;
        } else if (rom.checkState == RomCheckState::Sha1Mismatch) {
            hasSha1 = true;
        }
    }

    if (game.roms.empty()) {
        game.status = GameStatus::Ok;
    } else if (hasMissing) {
        game.status = GameStatus::Incomplete;
    } else if (hasBoth) {
        game.status = GameStatus::BothMismatch;
    } else if (hasCrc) {
        game.status = GameStatus::CrcMismatch;
    } else if (hasSha1) {
        game.status = GameStatus::Sha1Mismatch;
    } else {
        game.status = GameStatus::Ok;
    }
}

void RomVerifier::startVerification(std::vector<GameEntry> games, const std::vector<std::string> &romPaths) {
    m_running = true;
    m_cancelled = false;

    // ワーカースレッドを生成してバックグラウンドで処理
    QThread *thread = QThread::create([this, games, romPaths]() mutable {
        int total = static_cast<int>(games.size());
        std::vector<VerifiedItem> batch;
        batch.reserve(50);

        for (int i = 0; i < total; ++i) {
            if (m_cancelled) break;

            verifyGame(games[i], romPaths);
            batch.push_back({i, games[i]});

            // 40件ごと、または最後の1件でまとめてメインスレッドへ通知
            if (batch.size() >= 40 || i == total - 1) {
                emit batchVerified(batch);
                emit progress(i + 1, total);
                batch.clear();
                // UI スレッドがイベントを処理できるように微小スリープ
                QThread::msleep(1);
            }
        }

        m_running = false;
        emit finished();
    });

    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}
