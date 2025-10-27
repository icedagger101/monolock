#include "ScreenManager.h"
#include <unistd.h>

ScreenManager::ScreenManager()
{
    dpy = XOpenDisplay(nullptr);
    if (!dpy) {
        throw std::runtime_error("Cannot open X display.");
    }

    Window root = DefaultRootWindow(dpy);
    XSetWindowAttributes attrs{};
    attrs.override_redirect = True;
    attrs.background_pixel = BlackPixel(dpy, DefaultScreen(dpy));

    if (XineramaIsActive(dpy)) {
        int heads = 0;
        XineramaScreenInfo* si = XineramaQueryScreens(dpy, &heads);
        if (si && heads > 0) {
            screens.assign(si, si + heads);
            XFree(si);
        }
    }

    if (screens.empty()) {
        XineramaScreenInfo s{};
        s.screen_number = 0;
        s.x_org = 0;
        s.y_org = 0;
        s.width = DisplayWidth(dpy, DefaultScreen(dpy));
        s.height = DisplayHeight(dpy, DefaultScreen(dpy));
        screens.push_back(s);
    }

    for (const auto& screenInfo : screens) {
        Window win = XCreateWindow(dpy, root, screenInfo.x_org, screenInfo.y_org, screenInfo.width, screenInfo.height, 0,
            DefaultDepth(dpy, DefaultScreen(dpy)), CopyFromParent, DefaultVisual(dpy, DefaultScreen(dpy)),
            CWOverrideRedirect | CWBackPixel, &attrs);
        XSelectInput(dpy, win, ExposureMask | KeyPressMask);
        XMapRaised(dpy, win);
        windows.push_back(win);
    }

    if (!windows.empty()) {
        activeWin = windows.front();
        activeScreenIdx = 0;
    }
}

ScreenManager::~ScreenManager()
{
    if (!dpy) {
        return;
    }
    ungrabInput();
    for (Window w : windows) {
        XDestroyWindow(dpy, w);
    }
    XCloseDisplay(dpy);
}

void ScreenManager::grabInput()
{
    for (int i = 0; i < 5; ++i) {
        if (XGrabKeyboard(dpy, DefaultRootWindow(dpy), True, GrabModeAsync, GrabModeAsync, CurrentTime) == GrabSuccess) {
            break;
        }
        usleep(200000);  // **FIX: Increased from 100000 (0.1s) to 200000 (0.2s)**
    }
    XGrabPointer(dpy, DefaultRootWindow(dpy), True, ButtonPressMask | PointerMotionMask, GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
}

void ScreenManager::ungrabInput()
{
    if (!dpy) {
        return;
    }
    XUngrabPointer(dpy, CurrentTime);
    XUngrabKeyboard(dpy, CurrentTime);
}

const XineramaScreenInfo& ScreenManager::getActiveScreenInfo() const
{
    return screens.at(static_cast<size_t>(activeScreenIdx));
}

int ScreenManager::getScreenIndexForCoordinates(int x, int y) const
{
    for (size_t i = 0; i < screens.size(); ++i) {
        const auto& s = screens[i];
        if (x >= s.x_org && x < s.x_org + s.width && y >= s.y_org && y < s.y_org + s.height) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void ScreenManager::forceSetActiveWindow(int screenIndex)
{
    if (screenIndex < 0 || static_cast<size_t>(screenIndex) >= windows.size()) {
        return;
    }
    activeScreenIdx = screenIndex;
    activeWin = windows[static_cast<size_t>(screenIndex)];
}
