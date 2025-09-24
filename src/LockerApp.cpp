#include "LockerApp.h"

#include <X11/keysym.h>
#include <iostream>
#include <thread>
#include <chrono>

LockerApp* LockerApp::instance = nullptr;

LockerApp::LockerApp()
    : config(),
      screenManager(),
      renderer(screenManager.getDisplay(), screenManager.getActiveWindow(), config),
      authenticator() {
    instance = this;
    signal(SIGINT, LockerApp::handleSignal);
    signal(SIGTERM, LockerApp::handleSignal);
}

void LockerApp::handleSignal(int) {
    if (LockerApp::instance) {
        std::exit(1);
    }
}

void LockerApp::run() {   
    Display* dpy = screenManager.getDisplay();

    // Clear pending events
    XEvent dummy_ev;
    XSync(dpy, False);
    while (XPending(dpy)) XNextEvent(dpy, &dummy_ev);

    // Determine which screen the cursor is on at startup
    Window root = DefaultRootWindow(dpy);
    int rx = 0, ry = 0, wx = 0, wy = 0;
    unsigned int mask = 0;
    Window rret = 0, cret = 0;
    XQueryPointer(dpy, root, &rret, &cret, &rx, &ry, &wx, &wy, &mask);

    int final_screen_idx = screenManager.getScreenIndexForCoordinates(rx, ry);
    if (final_screen_idx < 0) final_screen_idx = 0;

    // Activate the screen under the cursor and grab input
    screenManager.forceSetActiveWindow(final_screen_idx);
    screenManager.grabInput();
    renderer.setActiveWindow(screenManager.getActiveWindow());

    // Draw backgrounds on all screens once
    const auto& all_wins = screenManager.getAllWindows();
    const auto& all_screens = screenManager.getAllScreens();
    for (size_t i = 0; i < all_wins.size(); ++i) {
        renderer.drawBackgroundOnly(all_wins[i], all_screens[i]);
    }

    // Initial draw on active screen
    renderer.setActiveWindow(screenManager.getActiveWindow());
    renderer.draw(state, screenManager.getActiveScreenInfo());

    // Track last cursor position to avoid unnecessary switches
    int last_rx = rx;
    int last_ry = ry;

    XEvent ev;
    while (true) {
        // Process all pending X events
        while (XPending(dpy)) {
            XNextEvent(dpy, &ev);
            handleEvent(ev);
        }

        // Poll cursor position on root window
        XQueryPointer(dpy, root, &rret, &cret, &rx, &ry, &wx, &wy, &mask);

        // If cursor moved, check screen membership
        if (rx != last_rx || ry != last_ry) {
            last_rx = rx;
            last_ry = ry;

            int new_idx = screenManager.getScreenIndexForCoordinates(rx, ry);

            if (new_idx != -1 && new_idx != screenManager.getActiveScreenIndex()) {
                std::cerr << "Switching active screen: " << new_idx
                          << " (cursor " << rx << ',' << ry << ")\n";

                int old_idx = screenManager.getActiveScreenIndex();
                Window old_win = screenManager.getActiveWindow();

                // redraw background on old screen
                renderer.drawBackgroundOnly(old_win, all_screens[old_idx]);

                // switch active screen: ungrab -> set -> grab
                screenManager.ungrabInput();
                screenManager.forceSetActiveWindow(new_idx);
                screenManager.grabInput();

                // draw UI on new active screen
                renderer.setActiveWindow(screenManager.getActiveWindow());
                renderer.draw(state, screenManager.getActiveScreenInfo());
            }
        }

        // update caps lock state periodically
        updateCapsLockState();

        // sleep to limit CPU usage (~20 FPS)
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void LockerApp::updateCapsLockState() {
    unsigned int current_mask;
    Window dummy1, dummy2;
    int dx, dy, dx2, dy2;
    XQueryPointer(screenManager.getDisplay(), DefaultRootWindow(screenManager.getDisplay()),
                  &dummy1, &dummy2, &dx, &dy, &dx2, &dy2, &current_mask);

    bool new_caps_state = (current_mask & LockMask);
    if (new_caps_state != state.capsLockOn) {
        state.capsLockOn = new_caps_state;
        renderer.draw(state, screenManager.getActiveScreenInfo());
    }
}

void LockerApp::handleEvent(XEvent& ev) {
    updateCapsLockState();

    switch (ev.type) {
    case Expose:
        if (ev.xexpose.window == screenManager.getActiveWindow()) {
            renderer.draw(state, screenManager.getActiveScreenInfo());
        }
        break;
    case KeyPress:
        handleKeyPress(ev.xkey);
        break;
    default:
        break;
    }
}

void LockerApp::handleKeyPress(XKeyEvent& kev) {
    KeySym ks;
    char buf[32] = {0};
    int len = XLookupString(&kev, buf, static_cast<int>(sizeof(buf)), &ks, nullptr);

    state.authFailed = false;

    if (ks == XK_Return) {
        if (state.password.empty()) return;

        state.isUnlocking = true;
        renderer.draw(state, screenManager.getActiveScreenInfo());

        bool ok = authenticator.checkPassword(state.password);

        state.isUnlocking = false;

        if (ok) {
            std::fill(state.password.begin(), state.password.end(), '\0');
            std::exit(0);
        } else {
            state.authFailed = true;
            std::fill(state.password.begin(), state.password.end(), '\0');
            state.password.clear();
        }
    } else if (ks == XK_BackSpace) {
        if (!state.password.empty()) state.password.pop_back();
    } else if (ks == XK_Escape) {
        state.password.clear();
    } else if (len > 0) {
        for (int i = 0; i < len; ++i) {
            if (buf[i] >= 32 && buf[i] <= 126) state.password.push_back(buf[i]);
        }
    }

    renderer.draw(state, screenManager.getActiveScreenInfo());
}

