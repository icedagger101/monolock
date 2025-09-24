#include "Renderer.h"
#include <fontconfig/fontconfig.h>
#include <iostream>
#include <memory>
#include <stdexcept>

Renderer::Renderer(Display* dpy, Window win, const Config& cfg)
    : display(dpy)
    , currentWindow(win)
{
    screenNum = DefaultScreen(dpy);
    visual = DefaultVisual(dpy, screenNum);
    colormap = DefaultColormap(dpy, screenNum);

    loadResources(cfg);

    xftDraw = XftDrawCreate(dpy, currentWindow, visual, colormap);
    if (!xftDraw) {
        throw std::runtime_error("Failed to create XftDraw.");
    }
}

Renderer::~Renderer()
{
    if (xftFont)
        XftFontClose(display, xftFont);
    if (xftDraw)
        XftDrawDestroy(xftDraw);
}

void Renderer::loadResources(const Config& cfg)
{
    loadColors(cfg);
    loadFont(cfg);
    asciiArt = cfg.getAsciiArt();
    passwordChar = cfg.getChar("password_char", '*');
}

void Renderer::loadFont(const Config& cfg)
{
    std::string fontSpec = cfg.getString("font", "monospace:size=14");

    auto patternDeleter = [](FcPattern* p) { FcPatternDestroy(p); };
    std::unique_ptr<FcPattern, decltype(patternDeleter)> pattern(FcNameParse(reinterpret_cast<const FcChar8*>(fontSpec.c_str())), patternDeleter);

    if (!pattern) {
        throw std::runtime_error("Failed to parse font spec: " + fontSpec);
    }

    FcConfigSubstitute(nullptr, pattern.get(), FcMatchPattern);
    XftDefaultSubstitute(display, screenNum, pattern.get());

    FcResult result;
    FcPattern* match = XftFontMatch(display, screenNum, pattern.get(), &result);
    if (!match) {
        throw std::runtime_error("Failed to find a matching font for: " + fontSpec);
    }

    xftFont = XftFontOpenPattern(display, match);
    if (!xftFont) {
        FcPatternDestroy(match);
        throw std::runtime_error("Xft could not open the matched font.");
    }
}

void Renderer::loadColors(const Config& cfg)
{
    XRenderColor rc;

    parseHexColor(cfg.getString("background_color", "#000000"), rc);
    XftColorAllocValue(display, visual, colormap, &rc, &backgroundColor);

    parseHexColor(cfg.getString("text_color", "#FFFFFF"), rc);
    XftColorAllocValue(display, visual, colormap, &rc, &textColor);

    parseHexColor(cfg.getString("box_color", "#000000"), rc);
    XftColorAllocValue(display, visual, colormap, &rc, &boxColor);

    parseHexColor(cfg.getString("error_color", "#FF0000"), rc);
    XftColorAllocValue(display, visual, colormap, &rc, &errorColor);

    std::string startHex = cfg.getString("ascii_color_start", "");
    std::string endHex = cfg.getString("ascii_color_end", "");
    size_t numLines = cfg.getAsciiArt().size();

    if (!startHex.empty() && !endHex.empty() && numLines > 0) {
        XRenderColor startC, endC;
        if (parseHexColor(startHex, startC) && parseHexColor(endHex, endC)) {
            for (size_t i = 0; i < numLines; ++i) {
                float ratio = (numLines == 1) ? 0.0f : static_cast<float>(i) / (numLines - 1);
                XRenderColor stepC;
                stepC.red = startC.red + (endC.red - startC.red) * ratio;
                stepC.green = startC.green + (endC.green - startC.green) * ratio;
                stepC.blue = startC.blue + (endC.blue - startC.blue) * ratio;
                stepC.alpha = 0xffff;

                XftColor tempXftColor;
                XftColorAllocValue(display, visual, colormap, &stepC, &tempXftColor);
                asciiGradient.push_back(tempXftColor);
            }
        }
    } else {
        parseHexColor(cfg.getString("ascii_color", "#FFFFFF"), rc);
        XftColorAllocValue(display, visual, colormap, &rc, &asciiColor);
    }
}

bool Renderer::parseHexColor(const std::string& hex, XRenderColor& color)
{
    if (hex.size() != 7 || hex[0] != '#')
        return false;
    unsigned int r, g, b;
    if (sscanf(hex.c_str() + 1, "%02x%02x%02x", &r, &g, &b) != 3)
        return false;
    color.red = r * 257;
    color.green = g * 257;
    color.blue = b * 257;
    color.alpha = 0xffff;
    return true;
}

void Renderer::setActiveWindow(Window win)
{
    if (win == currentWindow)
        return;
    currentWindow = win;
    if (xftDraw) {
        XftDrawDestroy(xftDraw);
    }
    xftDraw = XftDrawCreate(display, currentWindow, visual, colormap);
    if (!xftDraw) {
        throw std::runtime_error("Failed to recreate XftDraw.");
    }
}

void Renderer::draw(const AppState& state, const XineramaScreenInfo& screen)
{
    XftDrawRect(xftDraw, &backgroundColor, 0, 0, screen.width, screen.height);
    drawAsciiArt(screen);
    drawInputBox(state, screen);
    XFlush(display);
}

void Renderer::drawBackgroundOnly(Window win, const XineramaScreenInfo& screen)
{
    setActiveWindow(win);
    XftDrawRect(xftDraw, &backgroundColor, 0, 0, screen.width, screen.height);
    XFlush(display);
}

void Renderer::drawAsciiArt(const XineramaScreenInfo& screen)
{
    int fontHeight = xftFont->ascent + xftFont->descent;
    int blockHeight = asciiArt.size() * fontHeight;
    int startY = (screen.height / 2) - blockHeight / 2;
    bool useGradient = !asciiGradient.empty();

    for (size_t i = 0; i < asciiArt.size(); i++) {
        XGlyphInfo extents;
        XftTextExtentsUtf8(display, xftFont, (FcChar8*)asciiArt[i].c_str(), asciiArt[i].size(), &extents);
        int x = (screen.width - extents.width) / 2;
        int y = startY + i * fontHeight + xftFont->ascent;
        XftColor* color = useGradient ? &asciiGradient[i] : &asciiColor;
        XftDrawStringUtf8(xftDraw, color, xftFont, x, y, (FcChar8*)asciiArt[i].c_str(), asciiArt[i].size());
    }
}

void Renderer::drawInputBox(const AppState& state, const XineramaScreenInfo& screen)
{
    const int boxW = 300, boxH = 30;
    const int border = 2, padding = 5;
    int fontHeight = xftFont->ascent + xftFont->descent;
    int artHeight = asciiArt.size() * fontHeight;
    int artCenterY = screen.height / 2;
    int artBottom = artCenterY + artHeight / 2;
    int boxY = artBottom + 40;

    XRectangle boxRect = { static_cast<short>((screen.width - boxW) / 2), static_cast<short>(boxY),
        static_cast<unsigned short>(boxW), static_cast<unsigned short>(boxH) };

    XftColor* borderColor = state.authFailed ? &errorColor : &textColor;
    XftDrawRect(xftDraw, borderColor, boxRect.x - border, boxRect.y - border, boxRect.width + 2 * border, boxRect.height + 2 * border);
    XftDrawRect(xftDraw, &boxColor, boxRect.x, boxRect.y, boxRect.width, boxRect.height);

    std::string textToDraw;
    bool showCursor = false;
    if (state.isUnlocking)
        textToDraw = "Unlocking...";
    else if (state.authFailed)
        textToDraw = "Wrong!";
    else if (state.password.empty())
        textToDraw = "Enter password";
    else {
        textToDraw = std::string(state.password.size(), passwordChar);
        showCursor = true;
    }

    XGlyphInfo ext;
    XftTextExtentsUtf8(display, xftFont, (FcChar8*)textToDraw.c_str(), textToDraw.size(), &ext);

    int drawableWidth = boxRect.width - 2 * padding;
    int textX = (ext.width < drawableWidth) ? (boxRect.x + (boxRect.width - ext.width) / 2) : (boxRect.x + boxRect.width - padding - ext.width);
    int textY = boxRect.y + (boxRect.height / 2) + (xftFont->ascent - xftFont->descent) / 2;

    XRectangle clip = { static_cast<short>(boxRect.x + padding), boxRect.y, static_cast<unsigned short>(drawableWidth), boxRect.height };
    XftDrawSetClipRectangles(xftDraw, 0, 0, &clip, 1);
    XftDrawStringUtf8(xftDraw, &textColor, xftFont, textX, textY, (FcChar8*)textToDraw.c_str(), textToDraw.size());

    if (showCursor) {
        int cursorX = textX + ext.width;
        if (cursorX > boxRect.x + drawableWidth - 2) {
            cursorX = boxRect.x + drawableWidth - 2;
        }
        int cursorY = boxRect.y + (boxRect.height - fontHeight) / 2;
        XftDrawRect(xftDraw, &textColor, cursorX, cursorY, 8, fontHeight);
    }

    XftDrawSetClip(xftDraw, None);

    if (state.capsLockOn) {
        drawCapsLockIndicator(screen, boxRect);
    }
}

void Renderer::drawCapsLockIndicator(const XineramaScreenInfo& screen, const XRectangle& boxRect)
{
    std::string capsMsg = "CAPS LOCK ON";
    XGlyphInfo capsExt;
    XftTextExtentsUtf8(display, xftFont, (FcChar8*)capsMsg.c_str(), capsMsg.size(), &capsExt);
    int capsX = boxRect.x + (boxRect.width - capsExt.width) / 2;
    int capsY = boxRect.y + boxRect.height + xftFont->ascent + 5;
    XftDrawStringUtf8(xftDraw, &textColor, xftFont, capsX, capsY, (FcChar8*)capsMsg.c_str(), capsMsg.size());
}
