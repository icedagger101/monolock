#pragma once
#include <string>

class Authenticator {
public:
    Authenticator();
    bool checkPassword(const std::string& password);

private:
    std::string username;
};

