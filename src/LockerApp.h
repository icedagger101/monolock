#pragma once
#include "AppState.h"
#include "Authenticator.h"
#include "Config.h"
#include "Renderer.h"
#include "ScreenManager.h"
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/dpms.h>
#include <csignal>
#include <sys/types.h>
#include <unistd.h>

class LockerApp {
public:
    LockerApp();
    void run();

private:
    static LockerApp* instance;
    static void handleSignal(int sig);
    static void atexit_cleanup();

    void updateCapsLockState();
    void handleEvent(XEvent& ev);
    void handleKeyPress(XKeyEvent& kev);
    void handleResume();
    void setupSingleton();
    void cleanupSingleton();
    void handleUnlockSignal(XEvent& ev);

    Atom dpms_atom = None;
    Atom activeAtom = None;
    Atom unlockAtom = None;
    pid_t myPid;
    Window root_window = None;

    Config config;
    ScreenManager screenManager;
    Renderer renderer;
    Authenticator authenticator;
    AppState state;
};
