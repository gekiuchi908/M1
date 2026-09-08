#include "xml_parser.h"
#include <QFile>
#include <QXmlStreamReader>
#include <QString>

bool M1XmlParser::parse(const std::string &xmlFilePath, std::vector<GameEntry> &games, std::string &errorString) {
    QFile file(QString::fromStdString(xmlFilePath));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        errorString = "Failed to open XML file: " + xmlFilePath;
        return false;
    }

    games.clear();
    QXmlStreamReader xml(&file);

    GameEntry currentGame;
    std::string currentRegion;
    bool inGame = false;

    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();

        if (token == QXmlStreamReader::StartElement) {
            QString name = xml.name().toString();
            if (name == QStringLiteral("game")) {
                inGame = true;
                currentGame = GameEntry();
                QXmlStreamAttributes attrs = xml.attributes();
                if (attrs.hasAttribute("name")) {
                    currentGame.name = attrs.value("name").toString().toStdString();
                }
                if (attrs.hasAttribute("board")) {
                    currentGame.board = attrs.value("board").toString().toStdString();
                }
                if (attrs.hasAttribute("romof")) {
                    currentGame.parentZip = attrs.value("romof").toString().toStdString();
                }
            } else if (inGame) {
                if (name == QStringLiteral("description")) {
                    currentGame.description = xml.readElementText().trimmed().toStdString();
                } else if (name == QStringLiteral("year")) {
                    currentGame.year = xml.readElementText().trimmed().toStdString();
                } else if (name == QStringLiteral("manufacturer")) {
                    currentGame.manufacturer = xml.readElementText().trimmed().toStdString();
                } else if (name == QStringLiteral("m1data")) {
                    QXmlStreamAttributes attrs = xml.attributes();
                    if (attrs.hasAttribute("default")) {
                        currentGame.defaultCmd = attrs.value("default").toInt();
                    }
                    if (attrs.hasAttribute("stop")) {
                        currentGame.stopCmd = attrs.value("stop").toInt();
                    }
                    if (attrs.hasAttribute("min")) {
                        currentGame.minCmd = attrs.value("min").toInt();
                    }
                    if (attrs.hasAttribute("max")) {
                        currentGame.maxCmd = attrs.value("max").toInt();
                    }
                } else if (name == QStringLiteral("region")) {
                    QXmlStreamAttributes attrs = xml.attributes();
                    if (attrs.hasAttribute("type")) {
                        currentRegion = attrs.value("type").toString().toStdString();
                    }
                } else if (name == QStringLiteral("rom") || name == QStringLiteral("disk")) {
                    QXmlStreamAttributes attrs = xml.attributes();
                    RomInfo rom;
                    rom.region = currentRegion;
                    if (attrs.hasAttribute("name")) {
                        rom.name = attrs.value("name").toString().toStdString();
                    }
                    if (attrs.hasAttribute("size")) {
                        bool ok = false;
                        rom.size = attrs.value("size").toUInt(&ok, 0);
                    }
                    if (attrs.hasAttribute("crc")) {
                        bool ok = false;
                        rom.crc = attrs.value("crc").toUInt(&ok, 16);
                    }
                    if (attrs.hasAttribute("sha1")) {
                        rom.sha1 = attrs.value("sha1").toString().trimmed().toLower().toStdString();
                    }
                    if (attrs.hasAttribute("md5")) {
                        rom.md5 = attrs.value("md5").toString().trimmed().toLower().toStdString();
                    }
                    if (attrs.hasAttribute("offset")) {
                        bool ok = false;
                        rom.offset = attrs.value("offset").toUInt(&ok, 16);
                    }
                    if (!rom.name.empty()) {
                        currentGame.roms.push_back(rom);
                    }
                }
            }
        } else if (token == QXmlStreamReader::EndElement) {
            QString name = xml.name().toString();
            if (name == QStringLiteral("game")) {
                if (!currentGame.name.empty()) {
                    games.push_back(currentGame);
                }
                inGame = false;
            } else if (name == QStringLiteral("region")) {
                currentRegion.clear();
            }
        }
    }

    if (xml.hasError()) {
        errorString = xml.errorString().toStdString();
        return false;
    }

    return true;
}
