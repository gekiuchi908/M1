#pragma once

#include "models.h"
#include <vector>
#include <string>

class M1XmlParser {
public:
    static bool parse(const std::string &xmlFilePath, std::vector<GameEntry> &games, std::string &errorString);
};
