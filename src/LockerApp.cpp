#include "LockerApp.h"

#include <X11/keysym.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>

LockerApp* LockerApp::instance = nullptr;

LockerApp::LockerApp()
    : config(),
      screenManager(),
      renderer(screenManager.getDisplay(), screenManager.getActiveWindow(), config),
      authenticator() {
    instance = this;
    signal(SIGINT, LockerApp::handleSignal);
    signal(SIGTERM, LockerApp::handleSignal);

    myPid = getpid();
    Display* dpy = screenManager.getDisplay();
    root_window = DefaultRootWindow(dpy);

    int dummy;
    if (DPMSQueryExtension(dpy, &dummy, &dummy)) {
        dpms_atom = XInternAtom(dpy, "_DPMS", False);
        if (dpms_atom != None) {
            unsigned long event_mask = PropertyChangeMask;
            XSelectInput(dpy, root_window, event_mask);
        }
    }

    // Intern atoms for singleton
    activeAtom = XInternAtom(dpy, "_MONOLOCK_ACTIVE", False);
    unlockAtom = XInternAtom(dpy, "_MONOLOCK_UNLOCK", False);

    setupSingleton();  // Check and set if clear

    // Add PropertyNotify to event mask for unlock signal
    if (unlockAtom != None) {
        unsigned long event_mask = PropertyChangeMask;
        XSelectInput(dpy, root_window, event_mask);
    }

    std::atexit(&LockerApp::atexit_cleanup);
}

void LockerApp::atexit_cleanup() {
    if (instance) {
        instance->cleanupSingleton();
    }
}

void LockerApp::setupSingleton() {
    if (activeAtom == None) return;

    Display* dpy = screenManager.getDisplay();
    Atom type;
    int format;
    unsigned long nitems, bytes_after;
    unsigned char* prop_data = nullptr;
    pid_t existingPid = 0;

    Status status = XGetWindowProperty(dpy, root_window, activeAtom, 0, 1, False,
                                       XA_CARDINAL, &type, &format, &nitems, &bytes_after, &prop_data);

    if (status == Success && prop_data != nullptr) {
        if (type != None && format == 32 && nitems == 1) {
            existingPid = *reinterpret_cast<pid_t*>(prop_data);

            if (existingPid > 0 && kill(existingPid, 0) == 0) {
                std::cerr << "monolock already running (PID: " << existingPid << "). Exiting." << std::endl;
                XFree(prop_data);
                std::exit(1);
            }
        }
        XFree(prop_data);
    }
    // No active or invalid: Set our PID
    XChangeProperty(dpy, root_window, activeAtom, XA_CARDINAL, 32, PropModeReplace,
                    reinterpret_cast<unsigned char*>(&myPid), 1);
    XFlush(dpy);
}

void LockerApp::cleanupSingleton() {
    if (activeAtom != None) {
        Display* dpy = screenManager.getDisplay();
        // Delete property
        XDeleteProperty(dpy, root_window, activeAtom);
        XFlush(dpy);
    }
}

void LockerApp::handleUnlockSignal(XEvent& ev) {
    if (ev.type == PropertyNotify && ev.xproperty.atom == unlockAtom &&
        ev.xproperty.window == root_window) {
        std::cerr << "Unlock signal received. Exiting." << std::endl;
        cleanupSingleton();
        std::exit(0);
    }
}

void LockerApp::handleSignal(int) {
    if (instance) {
        instance->cleanupSingleton();
    }
    std::exit(1);
}

void LockerApp::handleResume() {
    Display* dpy = screenManager.getDisplay();

    // Sync to process any pending events from resume
    XSync(dpy, False);

    // Ungrab input (safe if already ungrabbed)
    screenManager.ungrabInput();

    // **FIX: Delay to allow X server to fully wake up post-suspend**
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Raise all overlay windows to ensure top-most stacking
    const auto& wins = screenManager.getAllWindows();
    for (auto win : wins) {
        XRaiseWindow(dpy, win);
    }

    // Re-grab input
    screenManager.grabInput();

    // **FIX: Sync again after regrab to flush events**
    XSync(dpy, False);

    // Re-query cursor to update active screen
    int rx = 0, ry = 0, wx = 0, wy = 0;
    unsigned int mask = 0;
    Window rret = 0, cret = 0;
    XQueryPointer(dpy, root_window, &rret, &cret, &rx, &ry, &wx, &wy, &mask);
    int new_idx = screenManager.getScreenIndexForCoordinates(rx, ry);
    if (new_idx >= 0) {
        screenManager.forceSetActiveWindow(new_idx);
    }

    // Redraw backgrounds on all screens
    const auto& all_screens = screenManager.getAllScreens();
    for (size_t i = 0; i < wins.size(); ++i) {
        renderer.drawBackgroundOnly(wins[i], all_screens[i]);
    }

    // Full redraw on active screen
    renderer.setActiveWindow(screenManager.getActiveWindow());
    renderer.draw(state, screenManager.getActiveScreenInfo());
}

void LockerApp::run() {   
    Display* dpy = screenManager.getDisplay();

    // Clear pending events
    XEvent dummy_ev;
    XSync(dpy, False);
    while (XPending(dpy)) XNextEvent(dpy, &dummy_ev);

    // Determine which screen the cursor is on at startup
    int rx = 0, ry = 0, wx = 0, wy = 0;
    unsigned int mask = 0;
    Window rret = 0, cret = 0;
    XQueryPointer(dpy, root_window, &rret, &cret, &rx, &ry, &wx, &wy, &mask);

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
        XQueryPointer(dpy, root_window, &rret, &cret, &rx, &ry, &wx, &wy, &mask);

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
    XQueryPointer(screenManager.getDisplay(), root_window,
                  &dummy1, &dummy2, &dx, &dy, &dx2, &dy2, &current_mask);

    bool new_caps_state = (current_mask & LockMask);
    if (new_caps_state != state.capsLockOn) {
        state.capsLockOn = new_caps_state;
        renderer.draw(state, screenManager.getActiveScreenInfo());
    }
}

void LockerApp::handleEvent(XEvent& ev) {
    updateCapsLockState();

    // Handle unlock signal
    handleUnlockSignal(ev);

    // Handle DPMS resume via PropertyNotify
    if (ev.type == PropertyNotify &&
        ev.xproperty.window == root_window &&
        ev.xproperty.atom == dpms_atom &&
        dpms_atom != None) {
        CARD16 state = 0;
        BOOL power_level = 0;
        Status status = DPMSInfo(screenManager.getDisplay(), &state, &power_level);
        if (status == True && state == DPMSModeOn) {
            handleResume();
            return;  // Early return to avoid redundant processing
        }
    }

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

            // Optional: Signal other instances to exit
            if (unlockAtom != None) {
                Display* dpy = screenManager.getDisplay();
                unsigned long unlockVal = 1;
                XChangeProperty(dpy, root_window, unlockAtom, XA_CARDINAL, 32,
                                PropModeReplace, reinterpret_cast<unsigned char*>(&unlockVal), 1);
                XFlush(dpy);
            }

            cleanupSingleton();  // Explicit delete
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
