#include "i18n.h"

namespace I18n {

static QString s_language = "en";

void setLanguage(const QString &lang) {
    if (lang == "jp" || lang == "ja") {
        s_language = "jp";
    } else {
        s_language = "en";
    }
}

QString language() {
    return s_language;
}

bool isJapanese() {
    return s_language == "jp";
}

QString gameStatusToString(GameStatus status) {
    if (isJapanese()) {
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
    } else {
        switch (status) {
            case GameStatus::Unchecked:     return "Unchecked";
            case GameStatus::Checking:      return "Checking...";
            case GameStatus::Ok:             return "Exact Match (OK)";
            case GameStatus::Missing:        return "Missing ROM";
            case GameStatus::Incomplete:     return "Incomplete ROMs";
            case GameStatus::CrcMismatch:    return "CRC Mismatch";
            case GameStatus::Sha1Mismatch:   return "SHA1 Mismatch";
            case GameStatus::BothMismatch:   return "Both CRC/SHA1 Mismatch";
        }
        return "Unknown";
    }
}

QString gameStatusToBadge(GameStatus status) {
    if (isJapanese()) {
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
    } else {
        switch (status) {
            case GameStatus::Unchecked:     return "⚪ Unchecked";
            case GameStatus::Checking:      return "⏳ Checking";
            case GameStatus::Ok:             return "🟢 OK";
            case GameStatus::Missing:        return "⚪ Missing";
            case GameStatus::Incomplete:     return "🟣 Incomplete";
            case GameStatus::CrcMismatch:    return "🟡 CRC Mismatch";
            case GameStatus::Sha1Mismatch:   return "🟠 SHA1 Mismatch";
            case GameStatus::BothMismatch:   return "🔴 Both Mismatch";
        }
        return "❓ Unknown";
    }
}

QString romCheckStateToString(RomCheckState state) {
    if (isJapanese()) {
        switch (state) {
            case RomCheckState::Unchecked:    return "未検証";
            case RomCheckState::Ok:           return "OK";
            case RomCheckState::Missing:      return "ファイル欠落";
            case RomCheckState::CrcMismatch:  return "CRC不一致";
            case RomCheckState::Sha1Mismatch: return "SHA1不一致";
            case RomCheckState::BothMismatch: return "CRC/SHA1両方不一致";
        }
        return "不明";
    } else {
        switch (state) {
            case RomCheckState::Unchecked:    return "Unchecked";
            case RomCheckState::Ok:           return "OK";
            case RomCheckState::Missing:      return "Missing";
            case RomCheckState::CrcMismatch:  return "CRC Mismatch";
            case RomCheckState::Sha1Mismatch: return "SHA1 Mismatch";
            case RomCheckState::BothMismatch: return "CRC/SHA1 Mismatch";
        }
        return "Unknown";
    }
}

QString tableHeader(int column) {
    if (isJapanese()) {
        switch (column) {
            case 0: return "状態";
            case 1: return "ゲーム名";
            case 2: return "年";
            case 3: return "メーカ";
            case 4: return "ROM名";
            case 5: return "基板";
            case 6: return "ROM数";
        }
    } else {
        switch (column) {
            case 0: return "Status";
            case 1: return "Game Title";
            case 2: return "Year";
            case 3: return "Manufacturer";
            case 4: return "ROM Name";
            case 5: return "Board";
            case 6: return "ROMs";
        }
    }
    return QString();
}

QString romBrowserTitle() {
    return "M1 ROM Browser";
}

QString searchLabel() {
    return isJapanese() ? "検索:" : "Search:";
}

QString searchPlaceholder() {
    return isJapanese() ? "ゲーム名、ROM名、メーカ、基板..." : "Game title, ROM name, manufacturer, board...";
}

QString filterLabel() {
    return isJapanese() ? "表示:" : "Filter:";
}

QString filterOption(int filterIndex) {
    if (isJapanese()) {
        switch (filterIndex) {
            case 0: return "すべてのゲーム";
            case 1: return "所持ゲーム (完全一致 + 不一致)";
            case 2: return "完全一致 (OK) のみ";
            case 3: return "不一致 / エラーのみ";
            case 4: return "未所持のみ";
        }
    } else {
        switch (filterIndex) {
            case 0: return "All Games";
            case 1: return "Available Games (OK + Mismatched)";
            case 2: return "Exact Match (OK) Only";
            case 3: return "Mismatched / Errors Only";
            case 4: return "Missing Only";
        }
    }
    return QString();
}

QString rescanButton() {
    return isJapanese() ? "🔄 ROM再スキャン" : "🔄 Rescan ROMs";
}

QString selectGamePrompt() {
    return isJapanese() ? "ゲームを選択してください" : "Please select a game";
}

QString individualRomVerification() {
    return isJapanese() ? "個別 ROM 検証結果 (CRC / SHA1 照合):" : "Individual ROM Verification (CRC / SHA1 Check):";
}

QString romTreeHeader(int column) {
    if (isJapanese()) {
        switch (column) {
            case 0: return "ROMファイル";
            case 1: return "状態";
            case 2: return "期待 CRC";
            case 3: return "実測 CRC";
            case 4: return "期待 SHA1";
            case 5: return "実測 SHA1";
            case 6: return "サイズ";
        }
    } else {
        switch (column) {
            case 0: return "ROM File";
            case 1: return "Status";
            case 2: return "Expected CRC";
            case 3: return "Actual CRC";
            case 4: return "Expected SHA1";
            case 5: return "Actual SHA1";
            case 6: return "Size";
        }
    }
    return QString();
}

QString playButton() {
    return isJapanese() ? "▶ 再生" : "▶ Play";
}

QString cancelButton() {
    return isJapanese() ? "キャンセル" : "Cancel";
}

QString statsString(int total, int available, int ok, int crcMismatch, int sha1Mismatch, int bothMismatch, int incomplete, int missing) {
    if (isJapanese()) {
        return QString("総数: %1 | 所持: %2 (🟢完全一致: %3 | 🟡CRC不一致: %4 | 🟠SHA1不一致: %5 | 🔴両方不一致: %6 | 🟣一部不足: %7) | 未所持: %8")
            .arg(total).arg(available).arg(ok).arg(crcMismatch).arg(sha1Mismatch).arg(bothMismatch).arg(incomplete).arg(missing);
    } else {
        return QString("Total: %1 | Available: %2 (🟢Exact: %3 | 🟡CRC Mismatch: %4 | 🟠SHA1 Mismatch: %5 | 🔴Both Mismatch: %6 | 🟣Incomplete: %7) | Missing: %8")
            .arg(total).arg(available).arg(ok).arg(crcMismatch).arg(sha1Mismatch).arg(bothMismatch).arg(incomplete).arg(missing);
    }
}

QString msgSelectGameTitle() {
    return isJapanese() ? "案内" : "Notice";
}

QString msgSelectGameText() {
    return isJapanese() ? "リストからゲームを選択してください。" : "Please select a game from the list.";
}

QString msgXmlNotFoundTitle() {
    return isJapanese() ? "エラー" : "Error";
}

QString msgXmlNotFoundText() {
    return isJapanese() ? "m1.xml が見つかりません。M1 の実行ディレクトリで起動してください。" : "m1.xml not found. Please run in M1 executable directory.";
}

QString msgXmlParseErrorTitle() {
    return isJapanese() ? "エラー" : "Error";
}

QString msgXmlParseErrorText(const QString &err) {
    return isJapanese() ? QString("m1.xml のパースに失敗しました:\n%1").arg(err) : QString("Failed to parse m1.xml:\n%1").arg(err);
}

QString detailRomName() {
    return isJapanese() ? "ROM名:" : "ROM Name:";
}

QString detailParent() {
    return isJapanese() ? "親セット:" : "Parent:";
}

QString detailYear() {
    return isJapanese() ? "発売年:" : "Year:";
}

QString detailManufacturer() {
    return isJapanese() ? "メーカ:" : "Manufacturer:";
}

QString detailBoard() {
    return isJapanese() ? "基板:" : "Board:";
}

QString detailTrackRange(int minCmd, int maxCmd, int defaultCmd) {
    if (isJapanese()) {
        return QString("<b>曲番号範囲:</b> %1 〜 %2 (デフォルト: %3)<br>").arg(minCmd).arg(maxCmd).arg(defaultCmd);
    } else {
        return QString("<b>Track Range:</b> %1 - %2 (Default: %3)<br>").arg(minCmd).arg(maxCmd).arg(defaultCmd);
    }
}

QString detailPath() {
    return isJapanese() ? "パス:" : "Path:";
}

QString normTooltip() {
    return isJapanese() ? "音量ノーマライズ (ON/OFF)" : "Sound Normalization (ON/OFF)";
}

QString nextTooltip() {
    return isJapanese() ? "自動曲送り (Auto Moving: 曲時間経過後に次の曲へ)" : "Auto Track Change (Auto Moving: Advances to next track after track duration)";
}

QString prevBtnTooltip() {
    return isJapanese() ? "前の曲 (- キー)" : "Previous Track (- key)";
}

QString playBtnTooltip() {
    return isJapanese() ? "再生" : "Play";
}

QString pauseBtnTooltip() {
    return isJapanese() ? "一時停止 (Space キー)" : "Pause (Space key)";
}

QString stopBtnTooltip() {
    return isJapanese() ? "停止 (Esc キー)" : "Stop (Esc key)";
}

QString nextBtnTooltip() {
    return isJapanese() ? "次の曲 (+ キー)" : "Next Track (+ key)";
}

QString selectGameBtnTooltip() {
    return isJapanese() ? "ゲーム一覧 (ROM Browser) を開く" : "Open Game List (ROM Browser)";
}

QString restartBtnTooltip() {
    return isJapanese() ? "現在の曲を先頭から再生 (0 キー)" : "Restart Track (0 key)";
}

QString settingsBtnTooltip() {
    return isJapanese() ? "設定" : "Settings";
}

QString toggleLogBtnTooltip() {
    return isJapanese() ? "ログ表示切替" : "Toggle Log";
}

QString reloadListBtn() {
    return "🔄 Reload";
}

QString reloadListTooltip() {
    return isJapanese() ? "トラックリストを再読み込み" : "Reload track list";
}

QString m1ErrorTitle() {
    return isJapanese() ? "M1 エラー" : "M1 Error";
}

QString settingsTitle() {
    return isJapanese() ? "M1 ランチャー設定" : "M1 Launcher Settings";
}

QString langLabel() {
    return isJapanese() ? "言語 / Language:" : "Language / 言語:";
}

QString romDirLabel() {
    return isJapanese() ? "ROM ディレクトリ:" : "ROM Directory:";
}

QString browseBtn() {
    return isJapanese() ? "参照..." : "Browse...";
}

QString m1BinLabel() {
    return isJapanese() ? "M1 実行ファイル:" : "M1 Executable:";
}

QString audioOutputLabel() {
    return isJapanese() ? "オーディオ出力 (-s):" : "Audio Output (-s):";
}

QString driverPulseAudio() {
    return isJapanese() ? "3: PulseAudio (推奨・標準)" : "3: PulseAudio (Recommended / Default)";
}

QString sampleRateLabel() {
    return isJapanese() ? "サンプルレート (-r):" : "Sample Rate (-r):";
}

QString sampleRateDefault() {
    return isJapanese() ? "44100 Hz (標準)" : "44100 Hz (Default)";
}

QString execModeLabel() {
    return isJapanese() ? "実行モード:" : "Execution Mode:";
}

QString terminalCheckText() {
    return isJapanese() ? "M1 を外部ターミナルウィンドウで起動する" : "Launch M1 in external terminal window";
}

QString terminalCheckTooltip() {
    return isJapanese()
        ? "チェックすると、別ウィンドウのコンソール（x-terminal-emulator等）で M1 を開き、キーボードで直接操作できます。"
        : "If checked, M1 opens in a separate console window (e.g. x-terminal-emulator) for direct keyboard control.";
}

QString browseRomDialogTitle() {
    return isJapanese() ? "ROM ディレクトリを選択" : "Select ROM Directory";
}

QString browseM1BinDialogTitle() {
    return isJapanese() ? "M1 実行ファイルを選択" : "Select M1 Executable";
}

} // namespace I18n
