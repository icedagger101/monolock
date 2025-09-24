#pragma once
#include "AppState.h"
#include "Authenticator.h"
#include "Config.h"
#include "Renderer.h"
#include "ScreenManager.h"
#include <X11/Xlib.h>
#include <csignal>

class LockerApp {
public:
    LockerApp();
    void run();

private:
    static LockerApp* instance;
    static void handleSignal(int sig);

    void updateCapsLockState();
    void handleEvent(XEvent& ev);
    void handleKeyPress(XKeyEvent& kev);

    Config config;
    ScreenManager screenManager;
    Renderer renderer;
    Authenticator authenticator;
    AppState state;
};
