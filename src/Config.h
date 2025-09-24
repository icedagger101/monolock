#pragma once
#include <map>
#include <string>
#include <vector>

class Config {
public:
    Config();

    void load();

    std::string getString(const std::string& key, const std::string& defaultValue) const;
    char getChar(const std::string& key, char defaultValue) const;
    std::vector<std::string> getAsciiArt() const;

private:
    std::map<std::string, std::string> settings;
    static std::vector<std::string> readAsciiFile(const std::string& path);
};
