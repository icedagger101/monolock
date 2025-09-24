#pragma once
#include <X11/Xlib.h>
#include <X11/extensions/Xinerama.h>
#include <stdexcept>
#include <vector>

class ScreenManager {
public:
    ScreenManager();
    ~ScreenManager();

    ScreenManager(const ScreenManager&) = delete;
    ScreenManager& operator=(const ScreenManager&) = delete;

    void grabInput();
    void ungrabInput();

    Display* getDisplay() const { return dpy; }
    Window getActiveWindow() const { return activeWin; }
    int getActiveScreenIndex() const { return activeScreenIdx; }
    const XineramaScreenInfo& getActiveScreenInfo() const;

    const std::vector<Window>& getAllWindows() const { return windows; }
    const std::vector<XineramaScreenInfo>& getAllScreens() const { return screens; }

    int getScreenIndexForCoordinates(int x, int y) const;
    void forceSetActiveWindow(int screenIndex);

private:
    Display* dpy = nullptr;
    std::vector<Window> windows;
    std::vector<XineramaScreenInfo> screens;
    Window activeWin = 0;
    int activeScreenIdx = 0;
};
