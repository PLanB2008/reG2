#include "welcome.hpp"
#include "Draw.hpp"
#include "Frontend.hpp"
#include "Function.hpp"
#include "SoundObject.hpp"
#include "gbh_graphics.hpp"
#include "menu.hpp"
#include "registry.hpp"
#include "settings.hpp"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#if defined(GTA2_PORT_SDL)
#include <SDL3/SDL.h>
#endif

// No std::wstring: with -fshort-wchar its traits would call the C library's 4-byte wide functions.

EXTERN_GLOBAL(SoundObject, snd1);

#if defined(GTA2_PORT_SDL)
int port_SetFrameWidth(bool wide);
bool port_TakeMouse(float* pX, float* pY, int* pLeft, int* pRight, int* pWheel);
void port_DrawPicture(s32 slot, s32 width, s32 height, const u32* pRgba, const f32* pXyuv, const u32* pColours, s32 count);
void port_FreePictures();
#endif

namespace PortWelcome
{
namespace
{
using PortSettings::Translate;

enum State
{
    kNotChecked,
    kShown,
    kDone,
};
State sState;
PortSettings::Style sSelected;

#if defined(GTA2_PORT_SDL)
// Screenshots of each style, cropped to what the widest screens show of them and scaled to 540 lines, and the
// logo.
const unsigned char kClassicPng[] = {
#embed "../resources/welcome_classic.png"
};
const unsigned char kEnhancedPng[] = {
#embed "../resources/welcome_enhanced.png"
};
const unsigned char kLogoPng[] = {
#embed "../resources/reg2_256.png"
};

struct Picture
{
    std::vector<u32> mPixels; // bytes r, g, b, a
    s32 mWidth;
    s32 mHeight;
    // Where the player is, from 0 to 1 across.
    f32 mPlayerU;
};
enum Slot
{
    kClassicSlot,
    kEnhancedSlot,
    kLogoSlot,
    kNumSlots,
};
Picture sPictures[kNumSlots];

bool Load(const unsigned char* pPng, size_t size, f32 playerU, Picture* pPicture)
{
    SDL_Surface* pLoaded = SDL_LoadPNG_IO(SDL_IOFromConstMem(pPng, size), true);
    SDL_Surface* pSurface = pLoaded ? SDL_ConvertSurface(pLoaded, SDL_PIXELFORMAT_RGBA32) : NULL;
    SDL_DestroySurface(pLoaded);
    if (!pSurface)
    {
        return false;
    }
    pPicture->mWidth = pSurface->w;
    pPicture->mHeight = pSurface->h;
    pPicture->mPlayerU = playerU;
    pPicture->mPixels.resize(static_cast<size_t>(pSurface->w) * pSurface->h);
    for (s32 y = 0; y < pSurface->h; y++)
    {
        memcpy(&pPicture->mPixels[static_cast<size_t>(y) * pSurface->w], static_cast<const u8*>(pSurface->pixels) + y * pSurface->pitch,
               static_cast<size_t>(pSurface->w) * 4);
    }
    SDL_DestroySurface(pSurface);
    return true;
}

// The logo as a badge: its corners rounded off.
void RoundCorners(Picture* pPicture)
{
    const f32 radius = pPicture->mWidth * 0.2f;
    for (s32 y = 0; y < pPicture->mHeight; y++)
    {
        for (s32 x = 0; x < pPicture->mWidth; x++)
        {
            const f32 dx = fmaxf(fmaxf(radius - (x + 0.5f), (x + 0.5f) - (pPicture->mWidth - radius)), 0.0f);
            const f32 dy = fmaxf(fmaxf(radius - (y + 0.5f), (y + 0.5f) - (pPicture->mHeight - radius)), 0.0f);
            const f32 coverage = fminf(fmaxf(radius - sqrtf(dx * dx + dy * dy) + 0.5f, 0.0f), 1.0f);
            u32& pixel = pPicture->mPixels[static_cast<size_t>(y) * pPicture->mWidth + x];
            pixel = (pixel & 0xFFFFFFu) | (static_cast<u32>(coverage * 255.0f) << 24);
        }
    }
}

// The layout is that of a 16:9 screen 480 lines high, narrower on narrower screens.
const f32 kHeight = 480;
const f32 kDesignWidth = 480.0f * 16 / 9;
f32 sWidth = 640;

f32 Scale()
{
    return fminf(sWidth / kDesignWidth, 1.0f);
}

// A design x on the screen.
f32 X(f32 x)
{
    return sWidth / 2 + (x - kDesignWidth / 2) * Scale();
}

// The frontend fonts, at 4/3 of their size on a 16:9 screen.
f32 FontScale()
{
    return Scale() * 4 / 3;
}

u32 Rgba(u32 grey, u32 alpha)
{
    return grey | (grey << 8) | (grey << 16) | (alpha << 24);
}

// A quad from its corners (x, y), in turn, with a colour each.
void Quad(s32 slot, const f32* pCorners, const u32* pColours, f32 left = 0, f32 top = 0, f32 width = 1, f32 height = 1)
{
    static const s32 kOrder[6] = {0, 1, 2, 0, 2, 3};
    f32 xyuv[24];
    u32 colours[6];
    for (s32 i = 0; i < 6; i++)
    {
        const f32 x = pCorners[kOrder[i] * 2];
        const f32 y = pCorners[kOrder[i] * 2 + 1];
        xyuv[i * 4] = x;
        xyuv[i * 4 + 1] = y;
        xyuv[i * 4 + 2] = (x - left) / width;
        xyuv[i * 4 + 3] = (y - top) / height;
        colours[i] = pColours[kOrder[i]];
    }
    const Picture* pPicture = slot >= 0 ? &sPictures[slot] : NULL;
    port_DrawPicture(slot, pPicture ? pPicture->mWidth : 0, pPicture ? pPicture->mHeight : 0, pPicture ? pPicture->mPixels.data() : NULL,
                     xyuv, colours, 6);
}

void Rect(f32 left, f32 top, f32 right, f32 bottom, u32 colour)
{
    const f32 corners[8] = {left, top, right, top, right, bottom, left, bottom};
    const u32 colours[4] = {colour, colour, colour, colour};
    Quad(-1, corners, colours);
}

// A screenshot covering the screen from left to right, cropped not stretched, its player near playerX.
struct Placement
{
    f32 mLeft;
    f32 mTop;
    f32 mWidth;
    f32 mHeight;
};
Placement Place(Slot slot, f32 playerX, f32 left, f32 right)
{
    const Picture& picture = sPictures[slot];
    const f32 scale = fmaxf(kHeight / picture.mHeight, (right - left) / picture.mWidth);
    Placement place = {playerX - picture.mPlayerU * picture.mWidth * scale, 0, picture.mWidth * scale, picture.mHeight * scale};
    place.mLeft = fmaxf(fminf(place.mLeft, left), right - place.mWidth);
    place.mTop = (kHeight - place.mHeight) / 2;
    return place;
}

f32 SmoothStep(f32 t)
{
    return t * t * (3 - 2 * t);
}

// CLASSIC on the left, ENHANCED on the right, the unselected one darker, apart along a slanted line with a
// soft edge; darker at the top and bottom, under the texts.
void DrawBackground()
{
    const f32 slant = 85 * Scale();
    const f32 fade = 52 * Scale();
    const f32 top = sWidth / 2 + slant;
    const f32 bottom = sWidth / 2 - slant;
    const u32 classic = Rgba(sSelected == PortSettings::kClassicStyle ? 255 : 158, 255);
    const u32 enhanced = Rgba(sSelected == PortSettings::kEnhancedStyle ? 255 : 158, 255);

    const Placement right = Place(kEnhancedSlot, X(512), bottom - fade / 2, sWidth);
    const f32 rightCorners[8] = {bottom - fade / 2, 0, sWidth, 0, sWidth, kHeight, bottom - fade / 2, kHeight};
    const u32 rightColours[4] = {enhanced, enhanced, enhanced, enhanced};
    Quad(kEnhancedSlot, rightCorners, rightColours, right.mLeft, right.mTop, right.mWidth, right.mHeight);

    const Placement left = Place(kClassicSlot, X(230), 0, top + fade / 2);
    const f32 leftCorners[8] = {0, 0, top - fade / 2, 0, bottom - fade / 2, kHeight, 0, kHeight};
    const u32 leftColours[4] = {classic, classic, classic, classic};
    Quad(kClassicSlot, leftCorners, leftColours, left.mLeft, left.mTop, left.mWidth, left.mHeight);
    const s32 kStrips = 4;
    for (s32 i = 0; i < kStrips; i++)
    {
        const f32 a = fade * (static_cast<f32>(i) / kStrips - 0.5f);
        const f32 b = fade * (static_cast<f32>(i + 1) / kStrips - 0.5f);
        const u32 alphaA = static_cast<u32>(255 * (1 - SmoothStep(static_cast<f32>(i) / kStrips)));
        const u32 alphaB = static_cast<u32>(255 * (1 - SmoothStep(static_cast<f32>(i + 1) / kStrips)));
        const f32 corners[8] = {top + a, 0, top + b, 0, bottom + b, kHeight, bottom + a, kHeight};
        const u32 colours[4] = {(classic & 0xFFFFFFu) | (alphaA << 24), (classic & 0xFFFFFFu) | (alphaB << 24),
                                (classic & 0xFFFFFFu) | (alphaB << 24), (classic & 0xFFFFFFu) | (alphaA << 24)};
        Quad(kClassicSlot, corners, colours, left.mLeft, left.mTop, left.mWidth, left.mHeight);
    }

    const s32 kShades = 6;
    for (s32 i = 0; i < kShades; i++)
    {
        const f32 a = static_cast<f32>(i) / kShades;
        const f32 b = static_cast<f32>(i + 1) / kShades;
        const f32 topCorners[8] = {0, a * 0.42f * kHeight, sWidth, a * 0.42f * kHeight, sWidth, b * 0.42f * kHeight, 0, b * 0.42f * kHeight};
        const u32 topA = Rgba(0, static_cast<u32>(209 * (1 - SmoothStep(a))));
        const u32 topB = Rgba(0, static_cast<u32>(209 * (1 - SmoothStep(b))));
        const u32 topColours[4] = {topA, topA, topB, topB};
        Quad(-1, topCorners, topColours);
        const f32 y0 = (0.58f + a * 0.42f) * kHeight;
        const f32 y1 = (0.58f + b * 0.42f) * kHeight;
        const f32 bottomCorners[8] = {0, y0, sWidth, y0, sWidth, y1, 0, y1};
        const u32 bottomA = Rgba(0, static_cast<u32>(217 * SmoothStep(a)));
        const u32 bottomB = Rgba(0, static_cast<u32>(217 * SmoothStep(b)));
        const u32 bottomColours[4] = {bottomA, bottomA, bottomB, bottomB};
        Quad(-1, bottomCorners, bottomColours);
    }
}

// The logo on a light rim, over a shadow, size wide with its top at y.
void DrawLogo(f32 y, f32 size)
{
    const f32 x = sWidth / 2 - size / 2;
    const f32 shadow[8] = {x - 1, y + 3, x + size + 1, y + 3, x + size + 1, y + size + 5, x - 1, y + size + 5};
    const u32 shadowColours[4] = {Rgba(0, 150), Rgba(0, 150), Rgba(0, 150), Rgba(0, 150)};
    Quad(kLogoSlot, shadow, shadowColours, shadow[0], shadow[1], size + 2, size + 2);
    const f32 rim[8] = {x - 0.75f, y - 0.75f, x + size + 0.75f, y - 0.75f, x + size + 0.75f, y + size + 0.75f, x - 0.75f, y + size + 0.75f};
    const u32 rimColours[4] = {Rgba(255, 90), Rgba(255, 90), Rgba(255, 90), Rgba(255, 90)};
    Quad(kLogoSlot, rim, rimColours, rim[0], rim[1], size + 1.5f, size + 1.5f);
    const f32 logo[8] = {x, y, x + size, y, x + size, y + size, x, y + size};
    const u32 logoColours[4] = {Rgba(255, 255), Rgba(255, 255), Rgba(255, 255), Rgba(255, 255)};
    Quad(kLogoSlot, logo, logoColours, x, y, size, size);
}
#endif

// The texts' widths at a scale of 1.
f32 Width(const wchar_t* pText, s16 font)
{
    wchar_t text[96];
    wcsncpy(text, pText, 95);
    text[95] = 0;
    return static_cast<f32>(Frontend::GetMaxTextWidth(text, font));
}

Fix16 Fx(f32 v)
{
    return Fix16(static_cast<s32>(v * 16384.0f), 0);
}

// At x, its left (align -1), its centre (0) or its right (1); scale lowered to fit that width. Its width.
f32 Text(const wchar_t* pText, f32 x, f32 y, s16 font, f32 scale, s32 align, f32 maxWidth = 10000)
{
    const f32 width = Width(pText, font);
    scale = width * scale > maxWidth ? maxWidth / width : scale;
    DrawStringDefaultPalette(pText, Fx(x - (align + 1) * width * scale / 2), Fx(y), font, Fx(scale));
    return width * scale;
}

#if defined(GTA2_PORT_SDL)
void DrawTexts()
{
    const f32 scale = FontScale();
    const f32 box = fminf(sWidth, kDesignWidth);
    Text(Translate(L"CHOOSE YOUR STYLE"), sWidth / 2, 87, gFrontendTitleFont, scale, 0, box * 0.9f);

    // Each style's name, underlined when selected, then what it is; each within its half.
    const f32 half = box * 0.45f;
    const PortSettings::Style styles[2] = {PortSettings::kClassicStyle, PortSettings::kEnhancedStyle};
    const wchar_t* const kLines[2][3] = {{L"THE GAME AS IT WAS IN 1999", NULL, NULL},
                                         {L"REFINED LIGHTS AND SHADOWS", L"DAY AND NIGHT CYCLE", L"TWIN STICK CONTROLS"}};
    for (s32 i = 0; i < 2; i++)
    {
        const bool selected = sSelected == styles[i];
        const f32 x = i ? X(kDesignWidth - 43) : X(43);
        const s32 align = i ? 1 : -1;
        const f32 width = Text(Translate(i ? L"ENHANCED" : L"CLASSIC"), x, 311, selected ? gFrontendSelectedFont : gFrontendNormalFont,
                               scale, align, half);
        if (selected)
        {
            const f32 left = i ? x - width : x;
            Rect(left, 311 + 26 * Scale(), left + width, 311 + 27.5f * Scale(), 0xFF1414E6u);
        }
        for (s32 line = 0; line < 3 && kLines[i][line]; line++)
        {
            Text(Translate(kLines[i][line]), x, 337 + line * 15 * Scale(), gFrontendFont7, scale, align, half);
        }
    }

    // ALWAYS INCLUDED, then what both have, apart by red diamonds; smaller if the row is too wide.
    const wchar_t* const kShared[4] = {L"ALWAYS INCLUDED", L"WIDESCREEN", L"HD RESOLUTIONS", L"SMOOTH 60+ FPS"};
    f32 gap = 28 * Scale();
    f32 total = gap * 3;
    for (s32 i = 0; i < 4; i++)
    {
        total += Width(Translate(kShared[i]), i ? gFrontendFont7 : gFrontendFont6) * scale;
    }
    const f32 fit = fminf(box * 0.96f / total, 1.0f);
    gap *= fit;
    f32 x = sWidth / 2 - total * fit / 2;
    for (s32 i = 0; i < 4; i++)
    {
        x += Text(Translate(kShared[i]), x, 403, i ? gFrontendFont7 : gFrontendFont6, scale * fit, -1);
        if (i && i < 3)
        {
            const f32 r = 3.2f * Scale();
            const f32 cx = x + gap / 2;
            const f32 cy = 403 + 10 * Scale();
            const f32 diamond[8] = {cx, cy - r, cx + r, cy, cx, cy + r, cx - r, cy};
            const u32 red = 0xFF1919E1u;
            const u32 colours[4] = {red, red, red, red};
            Quad(-1, diamond, colours);
        }
        x += gap;
    }

    Text(Translate(L"YOU CAN CHANGE THIS AND MUCH MORE IN OPTIONS"), sWidth / 2, 429, gFrontendFont7, scale, 0, box * 0.96f);
    wchar_t keys[64];
    swprintf(keys, L"%ls %ls", PortMenu::PadMode() ? L"A" : Translate(L"ENTER"), Translate(L"SELECT"));
    Text(keys, sWidth / 2, 449, gFrontendFont7, scale, 0);
}

// Which half of the screen the mouse is over, along the slanted line.
PortSettings::Style StyleAt(f32 x, f32 y)
{
    const f32 slant = 85 * Scale();
    return x < sWidth / 2 + slant - 2 * slant * y / kHeight ? PortSettings::kClassicStyle : PortSettings::kEnhancedStyle;
}

// A player profile was chosen before: an install from before the screen existed.
bool PlayedBefore()
{
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\DMA Design Ltd\\GTA2\\Player", 0, KEY_ALL_ACCESS, &hKey) != ERROR_SUCCESS)
    {
        return false;
    }
    const bool found = RegQueryValueExA(hKey, "plyrslot", NULL, NULL, NULL, NULL) == ERROR_SUCCESS;
    RegCloseKey(hKey);
    return found;
}

bool ShouldShow()
{
    const char* pAgain = SDL_getenv("GTA2_WELCOME");
    return (pAgain && atoi(pAgain)) || !gRegistry.Get_Screen_Setting("port_style_chosen", 0);
}

void Close(bool apply)
{
    if (apply)
    {
        PortSettings::ApplyStyle(sSelected);
    }
    gRegistry.Set_Screen_Setting("port_style_chosen", 1);
    sState = kDone;
    port_SetFrameWidth(false);
    port_FreePictures();
    for (Picture& picture : sPictures)
    {
        picture = Picture();
    }
    snd1.mObjectType = apply ? PortMenu::kEntered : PortMenu::kWentBack;
}
#endif
} // namespace

bool Update(Frontend* pFrontend)
{
#if defined(GTA2_PORT_SDL)
    if (sState == kNotChecked)
    {
        sState = kDone;
        // The players sit at the middle of each screenshot's height, 2/3 and 1/3 across.
        if (ShouldShow() && Load(kClassicPng, sizeof(kClassicPng), 2.0f / 3, &sPictures[kClassicSlot]) &&
            Load(kEnhancedPng, sizeof(kEnhancedPng), 1.0f / 3, &sPictures[kEnhancedSlot]) &&
            Load(kLogoPng, sizeof(kLogoPng), 0.5f, &sPictures[kLogoSlot]))
        {
            RoundCorners(&sPictures[kLogoSlot]);
            sSelected = PlayedBefore() ? PortSettings::CurrentStyle() : PortSettings::kEnhancedStyle;
            sState = kShown;
        }
    }
    if (sState != kShown)
    {
        return false;
    }
    sWidth = static_cast<f32>(port_SetFrameWidth(true));
    float x;
    float y;
    int left;
    int right;
    int wheel;
    // The window's first pointer event only places the mouse, where the player may not have put it.
    static bool sMouseKnown;
    static f32 sMouseX;
    static f32 sMouseY;
    bool pointed = false;
    if (port_TakeMouse(&x, &y, &left, &right, &wheel))
    {
        pointed = sMouseKnown && (fabsf(x - sMouseX) > 2 || fabsf(y - sMouseY) > 2);
        sMouseKnown = true;
        sMouseX = x;
        sMouseY = y;
    }
    const PortSettings::Style before = sSelected;
    if (pFrontend->mLeftPressed || pFrontend->mRightPressed)
    {
        sSelected = pFrontend->mLeftPressed ? PortSettings::kClassicStyle : PortSettings::kEnhancedStyle;
    }
    else if (pointed || left)
    {
        sSelected = StyleAt(x, y);
    }
    if (sSelected != before)
    {
        snd1.mObjectType = sSelected == PortSettings::kClassicStyle ? PortMenu::kLowered : PortMenu::kRaised;
    }
    if (pFrontend->mReturnPressed || left)
    {
        Close(true);
    }
    else if (pFrontend->mEscapePressed)
    {
        Close(false);
    }
    return true;
#else
    (void)pFrontend;
    return false;
#endif
}

bool Draw()
{
#if defined(GTA2_PORT_SDL)
    if (sState != kShown)
    {
        return false;
    }
    pgbh_SetWindow(0, 0, sWidth, kHeight);
    DrawBackground();
    DrawLogo(15, 67 * Scale());
    DrawTexts();
    return true;
#else
    return false;
#endif
}
} // namespace PortWelcome
