#include "Config.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

Config::Config()
{
    load();
}

void Config::load()
{
    const char* homeDir = getenv("HOME");
    if (!homeDir) {
        return;
    }

    fs::path configPath = fs::path(homeDir) / ".config" / "monolock" / "config.ini";
    std::ifstream file(configPath);
    if (!file) {
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#' || line[0] == '[') {
            continue;
        }

        size_t eqPos = line.find('=');
        if (eqPos == std::string::npos) {
            continue;
        }

        std::string key = line.substr(0, eqPos);
        std::string val = line.substr(eqPos + 1);

        auto trim = [](std::string& s) {
            s.erase(0, s.find_first_not_of(" \t"));
            s.erase(s.find_last_not_of(" \t") + 1);
        };

        trim(key);
        trim(val);

        settings[key] = val;
    }
}

std::string Config::getString(const std::string& key, const std::string& defaultValue) const
{
    auto it = settings.find(key);
    return (it != settings.end()) ? it->second : defaultValue;
}

char Config::getChar(const std::string& key, char defaultValue) const
{
    auto it = settings.find(key);
    return (it != settings.end() && !it->second.empty()) ? it->second[0] : defaultValue;
}

std::vector<std::string> Config::getAsciiArt() const
{
    std::string asciiFilePath = getString("ascii_file", "");
    std::vector<std::string> asciiLines;

    if (!asciiFilePath.empty()) {
        asciiLines = readAsciiFile(asciiFilePath);
    }

    if (asciiLines.empty()) {
        asciiLines = { "monolock" };
    }
    return asciiLines;
}

std::vector<std::string> Config::readAsciiFile(const std::string& path)
{
    std::vector<std::string> lines;
    std::ifstream file(path);
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    return lines;
}
