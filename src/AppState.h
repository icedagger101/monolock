#pragma once
#include <string>

struct AppState {
    std::string password{};
    bool authFailed = false;
    bool isUnlocking = false;
    bool capsLockOn = false;
};
