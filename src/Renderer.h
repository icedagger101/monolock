#pragma once
#include "AppState.h"
#include "Config.h"
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xinerama.h>
#include <string>
#include <vector>

class Renderer {
public:
    Renderer(Display* dpy, Window win, const Config& cfg);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void setActiveWindow(Window win);
    void draw(const AppState& state, const XineramaScreenInfo& screen);
    void drawBackgroundOnly(Window win, const XineramaScreenInfo& screen);

private:
    void loadResources(const Config& cfg);
    void loadColors(const Config& cfg);
    void loadFont(const Config& cfg);
    bool parseHexColor(const std::string& hex, XRenderColor& color);

    void drawAsciiArt(const XineramaScreenInfo& screen);
    void drawInputBox(const AppState& state, const XineramaScreenInfo& screen);
    void drawCapsLockIndicator(const XineramaScreenInfo& screen, const XRectangle& boxRect);

    Display* display;
    Window currentWindow;
    Visual* visual;
    Colormap colormap;
    int screenNum;

    XftDraw* xftDraw = nullptr;
    XftFont* xftFont = nullptr;

    XftColor textColor{}, boxColor{}, errorColor{}, asciiColor{}, backgroundColor{};
    std::vector<XftColor> asciiGradient;
    std::vector<std::string> asciiArt;
    char passwordChar = '*';
};
