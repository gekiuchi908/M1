#pragma once

#include <string>
#include <vector>
#include <cstdint>

// ROM 単体の検証状態
enum class RomCheckState {
    Unchecked,      // 未検証
    Ok,             // 完全一致 (CRC & SHA1 一致)
    Missing,        // ZIP 内にファイルが存在しない
    CrcMismatch,    // CRC 不一致 (SHA1 は一致)
    Sha1Mismatch,   // SHA1 不一致 (CRC は一致)
    BothMismatch,   // CRC と SHA1 の両方が不一致
};

// 1つの ROM ファイルの情報 (m1.xml 内の <rom>)
struct RomInfo {
    std::string name;       // 期待ファイル名 (例: "41_09.rom")
    uint32_t size = 0;      // 期待サイズ (バイト)
    uint32_t crc = 0;       // 期待 CRC32
    std::string sha1;       // 期待 SHA1 (40文字 hex)
    std::string md5;        // 期待 MD5
    std::string region;     // リージョン (cpu1, samp1 等)
    uint32_t offset = 0;

    // 実測値
    bool found = false;
    std::string matchedFileName; // ZIP 内で見つかった実際のファイル名
    uint32_t actualCrc = 0;
    std::string actualSha1;
    uint32_t actualSize = 0;
    RomCheckState checkState = RomCheckState::Unchecked;
};

// ゲーム全体の総合ステータス
enum class GameStatus {
    Unchecked,      // 未検証
    Checking,       // 検証中
    Ok,             // 完全一致 (全ROM存在、CRC & SHA1 一致)
    Missing,        // ROM ZIP 自体が存在しない
    Incomplete,     // 一部の ROM ファイルが ZIP 内に存在しない
    CrcMismatch,    // CRC 不一致が存在 (SHA1 は一致)
    Sha1Mismatch,   // SHA1 不一致が存在 (CRC は一致)
    BothMismatch,   // CRC と SHA1 の両方が不一致の ROM が存在
};

// 1つのゲームの情報 (m1.xml 内の <game>)
struct GameEntry {
    std::string name;           // ROM名 / ZIP名 (例: "1941")
    std::string description;    // ゲーム名 (例: "1941 - Counter Attack (World)")
    std::string year;           // 年代 (例: "1990")
    std::string manufacturer;   // メーカ (例: "Capcom")
    std::string board;          // 基板名 (例: "CPS-1")
    std::string parentZip;      // 親セット名 (romof)
    int defaultCmd = 0;
    int stopCmd = 0;
    int minCmd = 0;
    int maxCmd = 0;

    std::vector<RomInfo> roms;  // 必要な ROM 一覧
    GameStatus status = GameStatus::Unchecked;
    std::string zipPath;        // 見つかった ZIP の実パス
};

inline const char *gameStatusToString(GameStatus status) {
    switch (status) {
        case GameStatus::Unchecked:     return "未検証";
        case GameStatus::Checking:      return "検証中...";
        case GameStatus::Ok:             return "完全一致 (OK)";
        case GameStatus::Missing:        return "ROMなし";
        case GameStatus::Incomplete:     return "一部ROM不足";
        case GameStatus::CrcMismatch:    return "CRC不一致";
        case GameStatus::Sha1Mismatch:   return "SHA1不一致";
        case GameStatus::BothMismatch:   return "CRC/SHA1両方不一致";
    }
    return "不明";
}

inline const char *gameStatusToBadge(GameStatus status) {
    switch (status) {
        case GameStatus::Unchecked:     return "⚪ 未検証";
        case GameStatus::Checking:      return "⏳ 検証中";
        case GameStatus::Ok:             return "🟢 OK";
        case GameStatus::Missing:        return "⚪ なし";
        case GameStatus::Incomplete:     return "🟣 一部不足";
        case GameStatus::CrcMismatch:    return "🟡 CRC不一致";
        case GameStatus::Sha1Mismatch:   return "🟠 SHA1不一致";
        case GameStatus::BothMismatch:   return "🔴 両方不一致";
    }
    return "❓ 不明";
}

inline const char *romCheckStateToString(RomCheckState state) {
    switch (state) {
        case RomCheckState::Unchecked:    return "未検証";
        case RomCheckState::Ok:           return "OK";
        case RomCheckState::Missing:      return "ファイル欠落";
        case RomCheckState::CrcMismatch:  return "CRC不一致";
        case RomCheckState::Sha1Mismatch: return "SHA1不一致";
        case RomCheckState::BothMismatch: return "CRC/SHA1両方不一致";
    }
    return "不明";
}

#include <QMetaType>
Q_DECLARE_METATYPE(GameEntry)
