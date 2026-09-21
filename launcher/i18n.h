#pragma once

#include <QString>
#include "models.h"

namespace I18n {
    void setLanguage(const QString &lang);
    QString language();
    bool isJapanese();

    // Models / Status strings
    QString gameStatusToString(GameStatus status);
    QString gameStatusToBadge(GameStatus status);
    QString romCheckStateToString(RomCheckState state);

    // GameTableModel
    QString tableHeader(int column);

    // RomBrowserDialog
    QString romBrowserTitle();
    QString searchLabel();
    QString searchPlaceholder();
    QString filterLabel();
    QString filterOption(int filterIndex);
    QString rescanButton();
    QString selectGamePrompt();
    QString individualRomVerification();
    QString romTreeHeader(int column);
    QString playButton();
    QString cancelButton();
    QString statsString(int total, int available, int ok, int crcMismatch, int sha1Mismatch, int bothMismatch, int incomplete, int missing);
    QString msgSelectGameTitle();
    QString msgSelectGameText();
    QString msgXmlNotFoundTitle();
    QString msgXmlNotFoundText();
    QString msgXmlParseErrorTitle();
    QString msgXmlParseErrorText(const QString &err);

    // Details panel in RomBrowser
    QString detailRomName();
    QString detailParent();
    QString detailYear();
    QString detailManufacturer();
    QString detailBoard();
    QString detailTrackRange(int minCmd, int maxCmd, int defaultCmd);
    QString detailPath();

    // MainWindow
    QString normTooltip();
    QString nextTooltip();
    QString prevBtnTooltip();
    QString playBtnTooltip();
    QString pauseBtnTooltip();
    QString stopBtnTooltip();
    QString nextBtnTooltip();
    QString selectGameBtnTooltip();
    QString restartBtnTooltip();
    QString settingsBtnTooltip();
    QString toggleLogBtnTooltip();
    QString reloadListBtn();
    QString reloadListTooltip();
    QString m1ErrorTitle();

    // SettingsDialog
    QString settingsTitle();
    QString langLabel();
    QString romDirLabel();
    QString browseBtn();
    QString m1BinLabel();
    QString audioOutputLabel();
    QString driverPulseAudio();
    QString sampleRateLabel();
    QString sampleRateDefault();
    QString execModeLabel();
    QString terminalCheckText();
    QString terminalCheckTooltip();
    QString browseRomDialogTitle();
    QString browseM1BinDialogTitle();
}
