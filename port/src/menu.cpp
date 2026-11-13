#include "menu.hpp"
#include "Draw.hpp"
#include "Frontend.hpp"
#include "Function.hpp"
#include "Globals.hpp"
#include "InputRecorder_67F8B0.hpp"
#include "KeyboardLayout.hpp"
#include "input.hpp"
#include "registry.hpp"
#include "settings.hpp"
#include "TextTable.hpp"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <vector>

// No std::wstring: with -fshort-wchar its traits would call the C library's 4-byte wide functions.

bool port_GameRunning();
#if defined(GTA2_PORT_SDL)
bool port_PadMode();
int port_GetPadBinding(int control);
void port_SetPadBinding(int control, int code);
int port_DefaultPadBinding(int control);
int port_GetAltBinding(int control);
void port_SetAltBinding(int control, int code);
int port_DefaultAltBinding(int control);
int port_KeyLetter(int dik);
void port_CaptureStart();
int port_CapturedKey();
int port_CapturedPad();
bool port_MouseMode();
bool port_TakeMouse(float* pX, float* pY, int* pLeft, int* pRight, int* pWheel);
bool port_MouseLeftDown();
void port_DeferRegistrySaves(bool defer);
#endif

namespace PortMenu
{
namespace
{
using PortSettings::Translate;

enum Page
{
    kMainPage,
    kGamePage,
    kGraphicsPage,
    kDayNightPage,
    kDisplayPage,
    kSoundPage,
    kControlsPage,
    kStatsPage,
    kSavesPage,
    kNumPages,
};

enum Kind
{
    kGoTo,
    kSetting,
    kBinding,
    kDefaultKeys,
    kBack,
    kResume,
    kQuit,
    kSave,
    kRetry,
};

struct Item
{
    Kind mKind;
    s32 mArg; // page, setting or control
    bool mGap; // set apart from the item above
};

const s32 kMaxItems = 16;
const wchar_t* const kTitles[kNumPages] =
    {L"OPTIONS", L"GAME", L"GRAPHICS", L"DAY AND NIGHT", L"DISPLAY", L"SOUND", L"CONTROLS", L"STATISTICS", L"RESUME SAVED STATUS"};

const s32 kNumControls = 12;
const s32 kDefaultControls[kNumControls] = {DIK_UP, DIK_DOWN, DIK_LEFT, DIK_RIGHT, DIK_LCONTROL, DIK_RETURN,
                                            DIK_SPACE, DIK_Z, DIK_X, DIK_TAB, DIK_LMENU, DIK_RSHIFT};
const wchar_t* const kControlNames[kNumControls] = {L"FORWARD", L"BACKWARD", L"LEFT", L"RIGHT", L"ATTACK",
                                                    L"ENTER OR EXIT", L"JUMP OR BRAKE", L"PREVIOUS WEAPON",
                                                    L"NEXT WEAPON", L"SPECIAL 1", L"SPECIAL 2", L"SPECIAL 3"};
// Pad binding codes (compat/src/pad.cpp): buttons 0 to 11, then the stick and d-pad directions.
const wchar_t* const kPadButtonNames[12] = {L"A", L"B", L"X", L"Y", L"LB", L"RB", L"BACK", L"START", L"L3", L"R3", L"LT", L"RT"};
const s32 kPadB = 1;
const s32 kPadBack = 6;
const s32 kPadLeft = 224;
const s32 kPadRight = 225;
const s32 kPadUp = 226;
const s32 kPadDown = 227;
// Second key codes (compat/src/port_platform.hpp): a DIK code, a mouse button, a wheel notch.
const s32 kMouseCode = 0x100;
const s32 kWheelUpCode = 0x110;

// A binding's columns, KEYBOARD alone without SDL.
enum Column
{
    kKeyColumn,
    kAltColumn,
    kPadColumn,
    kNumColumns,
};
#if defined(GTA2_PORT_SDL)
const s32 kColumns = kNumColumns;
#else
const s32 kColumns = 1;
#endif
const wchar_t* const kColumnNames[kNumColumns] = {L"KEYBOARD", L"ALTERNATE", L"GAMEPAD"};
// A save's two columns, each with two rows: city and money, date and jobs.
const s32 kSaveColumns = 2;
const s32 kMaxTexts = 4;
const s32 kTextLength = 40;

// The frontend's user sprites: a disc behind a pad button's name, arrows.
const s16 kDiscSprite = 2;
const s16 kLeftArrowSprite = 37;
const s16 kArrowSprite = 38;

Page sPage;
s32 sSelected[kNumPages];
PortSaves::Entry sSaves[PortSaves::kMaxSaves];
s32 sNumSaves;
// The retry checkpoint the pause menu offers.
PortSaves::Checkpoint sRetry;
s32 sRetryMoney;
long long sRetryTime;
s32 sStatRows;
s32 sColumn; // a Column
s32 sCapturing = -1; // control waiting for a key or pad input
bool sDragging; // a slider's knob, with the left button held
#if !defined(GTA2_PORT_SDL)
char_type sPrevKeys[256];
#endif

struct KeyName
{
    wchar_t mName[24];
};
std::vector<KeyName> sKeyNames;

// A page of settings, then BACK.
s32 SettingsPage(Item* pItems, const PortSettings::Id* pIds, s32 count)
{
    s32 n = 0;
    for (; n < count; n++)
    {
        pItems[n] = Item{kSetting, pIds[n], false};
    }
    pItems[n++] = Item{kBack, 0, true};
    return n;
}

s32 Items(Page page, Item* pItems)
{
    const bool inGame = port_GameRunning();
    s32 n = 0;
    switch (page)
    {
        case kMainPage:
            if (inGame && sRetry != PortSaves::kNoCheckpoint)
            {
                pItems[n++] = Item{kRetry, 0, false};
            }
            if (inGame)
            {
                pItems[n++] = Item{kResume, 0, false};
            }
            if (inGame && sStatRows)
            {
                pItems[n++] = Item{kGoTo, kStatsPage, false};
            }
            for (s32 page = kGamePage; page <= kControlsPage; page++)
            {
                pItems[n++] = Item{kGoTo, page, inGame && page == kGamePage};
            }
            pItems[n++] = Item{inGame ? kQuit : kBack, 0, true};
            break;
        case kGamePage:
            return SettingsPage(pItems, PortSettings::kGameSettings, PortSettings::kNumGameSettings);
        case kGraphicsPage:
            return SettingsPage(pItems, PortSettings::kGraphicsSettings, PortSettings::kNumGraphicsSettings);
        case kDayNightPage:
            return SettingsPage(pItems, PortSettings::kDayNightSettings, PortSettings::kNumDayNightSettings);
        case kDisplayPage:
            return SettingsPage(pItems, PortSettings::kDisplaySettings, PortSettings::kNumDisplaySettings);
        case kSoundPage:
            return SettingsPage(pItems, PortSettings::kSoundSettings, PortSettings::kNumSoundSettings);
        case kStatsPage:
            pItems[n++] = Item{kBack, 0, false};
            break;
        case kSavesPage:
            for (s32 i = 0; i < sNumSaves; i++)
            {
                pItems[n++] = Item{kSave, i, false};
            }
            pItems[n++] = Item{kBack, 0, true};
            break;
        default:
            for (s32 i = 0; i < kNumControls; i++)
            {
                pItems[n++] = Item{kBinding, i, false};
            }
#if defined(GTA2_PORT_SDL)
            pItems[n++] = Item{kSetting, PortSettings::kControls, true};
            pItems[n++] = Item{kSetting, PortSettings::kAimCamera, false};
#endif
            // Under the TWIN STICK settings, or set apart from the bindings.
            const bool gap = n == kNumControls;
            pItems[n++] = Item{kDefaultKeys, 0, gap};
            pItems[n++] = Item{kBack, 0, false};
            break;
    }
    return n;
}

const wchar_t* EnglishName(const Item& item)
{
    switch (item.mKind)
    {
        case kGoTo:
            return kTitles[item.mArg];
        case kSetting:
            return PortSettings::Name(static_cast<PortSettings::Id>(item.mArg));
        case kBinding:
            return kControlNames[item.mArg];
        case kDefaultKeys:
            return L"DEFAULT KEYS";
        case kResume:
            return L"RESUME";
        case kQuit:
            return L"QUIT GAME";
        case kSave:
            switch (sSaves[item.mArg].mKind)
            {
                case PortSaves::kChurch:
                    return L"CHURCH";
                case PortSaves::kAutosave:
                    return L"AUTOSAVE";
                default:
                    return sSaves[item.mArg].mCheckpoint == PortSaves::kBeforeFrenzy ? L"BEFORE LAST FRENZY" : L"BEFORE LAST JOB";
            }
        case kRetry:
            return sRetry == PortSaves::kBeforeFrenzy ? L"RETRY FRENZY" : L"RETRY JOB";
        default:
            return L"BACK";
    }
}

const wchar_t* ItemName(const Item& item)
{
    return Translate(EnglishName(item));
}

// The key names GTA2 Manager showed: data\keyboard\<lang>_kb.cfg, one name per DirectInput code.
void LoadKeyNames()
{
    static const char* const kFiles[] = {"eng", "fre", "ger", "ita", "spa", "por", "eng"};
    const s32 layout = KeyboardLayout::GetLayout();
    char path[64];
    sprintf(path, "data\\keyboard\\%s_kb.cfg", kFiles[layout >= 0 && layout < 7 ? layout : 0]);
    sKeyNames.clear();
    FILE* f = fopen(path, "rb");
    if (!f)
    {
        return;
    }
    KeyName line = {};
    size_t len = 0;
    for (int c; (c = fgetc(f)) != EOF;)
    {
        if (c == '\n')
        {
            sKeyNames.push_back(line);
            line = KeyName();
            len = 0;
        }
        else if (c != '\r' && c != '`' && len + 1 < 24)
        {
            line.mName[len++] = static_cast<wchar_t>(static_cast<u8>(c));
        }
    }
    fclose(f);
}

void KeyName(s32 dik, wchar_t* pOut)
{
    dik &= 0xFF;
#if defined(GTA2_PORT_SDL)
    // A letter as the keyboard's layout has it, whatever the language.
    if (const int letter = port_KeyLetter(dik))
    {
        pOut[0] = static_cast<wchar_t>(letter);
        pOut[1] = 0;
        return;
    }
#endif
    if (dik < static_cast<s32>(sKeyNames.size()) && sKeyNames[dik].mName[0])
    {
        wcscpy(pOut, sKeyNames[dik].mName);
    }
    else
    {
        swprintf(pOut, L"KEY %d", dik);
    }
}

void PadName(s32 code, wchar_t* pOut)
{
    static const wchar_t* const kDirections[] = {L"LEFT", L"RIGHT", L"UP", L"DOWN"};
    if (code >= kPadLeft && code <= kPadDown)
    {
        wcscpy(pOut, Translate(kDirections[code - kPadLeft]));
    }
    else
    {
        wcscpy(pOut, code >= 0 && code < 12 ? kPadButtonNames[code] : L"-");
    }
}

// The second key column's names, a key's or the mouse's; "-" for none.
void AltName(s32 code, wchar_t* pOut)
{
    if (code >= kWheelUpCode)
    {
        swprintf(pOut, L"%ls %ls", Translate(L"WHEEL"), code == kWheelUpCode ? L"+" : L"-");
    }
    else if (code > kMouseCode)
    {
        // SDL's buttons 1 to 5 are left, middle, right, back and forward: mouse 1, 3, 2, 4, 5.
        const s32 button = code - kMouseCode;
        swprintf(pOut, L"%ls %d", Translate(L"MOUSE"), button == 2 ? 3 : (button == 3 ? 2 : button));
    }
    else if (code > 0)
    {
        KeyName(code, pOut);
    }
    else
    {
        wcscpy(pOut, L"-");
    }
}

s32 GetKey(s32 control)
{
    return gRegistry.Get_Control_Setting(control, kDefaultControls[control]) & 0xFF;
}

void SetKey(s32 control, s32 key)
{
    HKEY hKey;
    if (RegCreateKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\DMA Design Ltd\\GTA2\\Control", 0, NULL, 0, KEY_ALL_ACCESS, NULL,
                        &hKey, NULL) != ERROR_SUCCESS)
    {
        return;
    }
    char name[4];
    sprintf(name, "%d", control);
    const DWORD data = static_cast<DWORD>(key);
    RegSetValueExA(hKey, name, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&data), sizeof(data));
    RegCloseKey(hKey);
}

s32 GetBinding(s32 column, s32 control)
{
#if defined(GTA2_PORT_SDL)
    if (column != kKeyColumn)
    {
        return column == kAltColumn ? port_GetAltBinding(control) : port_GetPadBinding(control);
    }
#endif
    return GetKey(control);
}

void SetBinding(s32 column, s32 control, s32 binding)
{
#if defined(GTA2_PORT_SDL)
    if (column != kKeyColumn)
    {
        column == kAltColumn ? port_SetAltBinding(control, binding) : port_SetPadBinding(control, binding);
        return;
    }
#endif
    SetKey(control, binding);
}

// The game reads the keyboard's bindings when it starts, the pad's at each press.
void ReloadKeys()
{
    if (gInputDevices)
    {
        gInputDevices->get_registry_controls();
    }
}

// A key or pad input already used by another binding moves to the one being changed, keys between both
// keyboard columns too, but a KEYBOARD binding never gets none or a mouse input.
void Bind(s32 column, s32 control, s32 binding)
{
    const s32 old = GetBinding(column, control);
    for (s32 c = 0; c < kColumns; c++)
    {
        for (s32 i = 0; i < kNumControls; i++)
        {
            const bool sameDevice = (c == kPadColumn) == (column == kPadColumn);
            if (sameDevice && (c != column || i != control) && GetBinding(c, i) == binding &&
                (c != kKeyColumn || (old > 0 && old < kMouseCode)))
            {
                SetBinding(c, i, old);
            }
        }
    }
    SetBinding(column, control, binding);
    ReloadKeys();
}

void DefaultKeys()
{
    for (s32 i = 0; i < kNumControls; i++)
    {
        SetKey(i, kDefaultControls[i]);
#if defined(GTA2_PORT_SDL)
        port_SetAltBinding(i, port_DefaultAltBinding(i));
        port_SetPadBinding(i, port_DefaultPadBinding(i));
#endif
    }
    ReloadKeys();
}

#if !defined(GTA2_PORT_SDL)
// The DirectX build has no SDL layer: the first key pressed in the frontend's key state.
s32 FrontendKeyPressed()
{
    s32 result = -1;
    for (s32 i = 0; i < 256 && result < 0; i++)
    {
        if ((gFrontend->mKeys[i] & 0x80) && !(sPrevKeys[i] & 0x80))
        {
            result = i;
        }
    }
    memcpy(sPrevKeys, gFrontend->mKeys, sizeof(sPrevKeys));
    return result;
}
#endif

void GoTo(Page page)
{
    if (page == kControlsPage)
    {
        sColumn = PadMode() ? kPadColumn : kKeyColumn;
    }
    sPage = page;
    sSelected[page] = 0;
}

Result Back()
{
    if (sPage == kMainPage || sPage == kSavesPage)
    {
        return kClosed;
    }
    sPage = kMainPage;
    return kWentBack;
}

// Drawing: the layout's coordinates to the screen's.
f32 sScale;
f32 sLeft;
f32 sTop;

Fix16 Fx(f32 v)
{
    return Fix16(static_cast<s32>(v * 16384.0f), 0);
}

// colour: 0xRRGGBB.
void Rect(f32 left, f32 top, f32 right, f32 bottom, s32 colour)
{
    Vert verts[4] = {};
    verts[0].x = verts[3].x = sLeft + left * sScale;
    verts[1].x = verts[2].x = sLeft + right * sScale;
    verts[0].y = verts[1].y = sTop + top * sScale;
    verts[2].y = verts[3].y = sTop + bottom * sScale;
    pgbh_DrawFlatRect(verts, colour);
}

// The text colours (white, the red remap); dim, darker.
s32 TextColour(bool selected, bool dim)
{
    return dim ? (selected ? 0x5C1212 : 0x707070) : (selected ? 0xB72323 : 0xFFFFFF);
}

// The pages' text: the frontend's plain white score font, smaller than its menus'; a selected line in its
// red-rimmed one, reddened by a remap. Neither has a '-', drawn as a bar.
s16 MenuFont()
{
    return gFrontendScoreFont;
}

const u16 kRedRemap = 4;
const f32 kDashWidth = 7;
const f32 kDashY = 8;

// Draws the text if asked; its width. Dim: half transparent, grey on the panel or the frontend's black.
f32 TextAt(const wchar_t* pText, f32 x, f32 y, s16 font, bool selected, bool dim, bool draw)
{
    const bool bars = GetCharWidthFromFontType(font, L'-') < 3;
    font = selected && font == MenuFont() ? gFrontendPlayMenuScoreFont : font;
    f32 width = 0;
    while (*pText)
    {
        wchar_t run[64];
        size_t n = 0;
        for (; pText[n] && n < 63 && !(bars && pText[n] == L'-'); n++)
        {
            run[n] = pText[n];
        }
        run[n] = 0;
        pText += n;
        if (n && draw)
        {
            DrawString(run, Fx(sLeft + (x + width) * sScale), Fx(sTop + y * sScale), font, Fx(sScale),
                       selected ? palette_types_enum::font_remaps_8 : palette_types_enum::sprites_2, selected ? kRedRemap : 0,
                       dim ? 1 : 0, dim ? 12 : 0);
        }
        width += n ? Frontend::GetMaxTextWidth(run, font) : 0;
        if (bars && *pText == L'-')
        {
            if (draw)
            {
                Rect(x + width + 1, y + kDashY, x + width + kDashWidth - 1, y + kDashY + 2, TextColour(selected, dim));
            }
            width += kDashWidth;
            pText++;
        }
    }
    return width;
}

f32 Width(const wchar_t* pText, s16 font)
{
    return TextAt(pText, 0, 0, font, false, false, false);
}

void Text(const wchar_t* pText, f32 x, f32 y, s16 font, bool selected = false, bool dim = false)
{
    TextAt(pText, x, y, font, selected, dim, true);
}

void Sprite(s16 sprite, f32 x, f32 y)
{
    DrawFigure(sprite_types_enum::user_6, sprite, Fx(sLeft + x * sScale), Fx(sTop + y * sScale), Ang16(0), Fx(sScale),
               palette_types_enum::sprites_2, 0, 0, 0, false);
}

f32 Max(f32 a, f32 b)
{
    return a > b ? a : b;
}

// The numeric settings' sliders: a track, then the value's text.
const f32 kSliderWidth = 100;
const f32 kSliderGap = 14;

struct Slider
{
    s32 mMin;
    s32 mMax;
    s32 mValue;
};

bool GetSlider(const Item& item, Slider* pSlider)
{
    return item.mKind == kSetting &&
        PortSettings::Range(static_cast<PortSettings::Id>(item.mArg), &pSlider->mMin, &pSlider->mMax, &pSlider->mValue);
}

// From the value column to the value's text.
f32 TextOffset(const Item& item)
{
    Slider slider;
    return GetSlider(item, &slider) ? kSliderWidth + kSliderGap : 0;
}

// The text's colours, a darker grey for the empty part of the track.
void DrawSlider(const Slider& slider, f32 x, f32 y, bool selected, bool dim)
{
    const s32 colour = TextColour(selected, dim);
    const f32 middle = y + 7;
    const f32 knob = x + kSliderWidth * (slider.mValue - slider.mMin) / (slider.mMax - slider.mMin);
    Rect(x, middle - 1.5f, kSliderWidth + x, middle + 1.5f, 0x606060);
    Rect(x, middle - 1.5f, knob, middle + 1.5f, colour);
    Rect(knob - 3, middle - 7, knob + 3, middle + 7, colour);
}

bool IsEnabled(const Item& item)
{
    return item.mKind != kSetting || PortSettings::Enabled(static_cast<PortSettings::Id>(item.mArg));
}

// Under the page, for a greyed setting: the one it needs.
void DrawRequirement(const Item& item, f32 x, f32 y)
{
    const PortSettings::Id requirement = PortSettings::Requirement(static_cast<PortSettings::Id>(item.mArg));
    wchar_t text[64];
    const wchar_t* pName = requirement == PortSettings::kLighting ? L"DAY-NIGHT CYCLE" : PortSettings::Name(requirement);
    swprintf(text, L"%ls %ls", Translate(L"NEEDS"), Translate(pName));
    Text(text, x, y, MenuFont());
}

// The value under the mouse on a slider's track, or nearest to it.
Result SetSliderAt(const Item& item, f32 x, f32 trackX)
{
    Slider slider;
    if (!GetSlider(item, &slider))
    {
        return kNothing;
    }
    const f32 steps = (x - trackX) * (slider.mMax - slider.mMin) / kSliderWidth;
    s32 value = slider.mMin + static_cast<s32>(steps < 0 ? steps - 0.5f : steps + 0.5f);
    value = value < slider.mMin ? slider.mMin : (value > slider.mMax ? slider.mMax : value);
    if (value == slider.mValue)
    {
        return kNothing;
    }
    PortSettings::Change(static_cast<PortSettings::Id>(item.mArg), value - slider.mValue);
    return value < slider.mValue ? kLowered : kRaised;
}

// $184,000 in English; 184.000 $, or 184 000 $ in French, in the others.
void MoneyText(s32 money, wchar_t* pOut)
{
    char digits[16];
    snprintf(digits, sizeof(digits), "%d", money < 0 ? -money : money);
    const char lang = gText_0x14 ? gText_0x14->mLangCode : 'e';
    const wchar_t separator = lang == 'e' ? L',' : (lang == 'f' ? L' ' : L'.');
    wchar_t* p = pOut;
    if (money < 0)
    {
        *p++ = L'-';
    }
    if (lang == 'e')
    {
        *p++ = L'$';
    }
    const size_t len = strlen(digits);
    for (size_t i = 0; i < len; i++)
    {
        if (i && (len - i) % 3 == 0)
        {
            *p++ = separator;
        }
        *p++ = static_cast<wchar_t>(digits[i]);
    }
    if (lang != 'e')
    {
        *p++ = L' ';
        *p++ = L'$';
    }
    *p = 0;
}

time_t Midnight(tm day)
{
    day.tm_hour = day.tm_min = day.tm_sec = 0;
    day.tm_isdst = -1;
    return mktime(&day);
}

// TODAY 21:40, YESTERDAY 18:05, 3 DAYS AGO.
void DateText(long long fileTime, wchar_t* pOut)
{
    const time_t t = static_cast<time_t>(fileTime);
    const time_t now = time(NULL);
    const tm* pLocal = fileTime >= 0 ? localtime(&t) : NULL;
    if (!pLocal)
    {
        pOut[0] = 0;
        return;
    }
    const tm local = *pLocal;
    const tm today = *localtime(&now);
    // Half a day more: a day of a daylight saving change is 23 or 25 hours long.
    const long long days = (static_cast<long long>(Midnight(today)) - Midnight(local) + 43200) / 86400;
    if (days >= 2)
    {
        swprintf(pOut, Translate(L"%d DAYS AGO"), static_cast<int>(days));
    }
    else
    {
        swprintf(pOut, L"%ls %d:%02d", Translate(days == 1 ? L"YESTERDAY" : L"TODAY"), local.tm_hour, local.tm_min);
    }
}

// JUST NOW, 8 MIN AGO, then as DateText.
void AgeText(long long fileTime, wchar_t* pOut)
{
    const long long age = static_cast<long long>(time(NULL)) - fileTime;
    if (fileTime >= 0 && age < 60)
    {
        wcscpy(pOut, Translate(L"JUST NOW"));
    }
    else if (fileTime >= 0 && age < 3600)
    {
        swprintf(pOut, Translate(L"%d MIN AGO"), static_cast<int>(age / 60));
    }
    else
    {
        DateText(fileTime, pOut);
    }
}

// A setting's value, a binding's inputs by column, a save's details by column then on the second row, or
// the checkpoint's money and age; empty for other items.
void ItemValues(const Item& item, wchar_t (*pTexts)[kTextLength])
{
    for (s32 c = 0; c < kMaxTexts; c++)
    {
        pTexts[c][0] = 0;
    }
    if (item.mKind == kRetry)
    {
        wchar_t age[kTextLength];
        MoneyText(sRetryMoney, pTexts[0]);
        AgeText(sRetryTime, age);
        wcscat(pTexts[0], L", ");
        wcscat(pTexts[0], age);
    }
    else if (item.mKind == kSave)
    {
        static const wchar_t* const kCities[PortSaves::kNumCities] = {L"DOWNTOWN", L"RESIDENTIAL", L"INDUSTRIAL"};
        const PortSaves::Entry& save = sSaves[item.mArg];
        wcscpy(pTexts[0], Translate(kCities[save.mMainStage < PortSaves::kNumCities ? save.mMainStage : 0]));
        DateText(save.mTime, pTexts[1]);
        MoneyText(save.mMoney, pTexts[2]);
        if (save.mJobsPassed >= 0)
        {
            swprintf(pTexts[3], Translate(L"%d/%d JOBS"), save.mJobsPassed, save.mJobsTotal);
        }
    }
    else if (item.mKind == kSetting)
    {
        PortSettings::Value(static_cast<PortSettings::Id>(item.mArg), pTexts[0]);
    }
    else if (item.mKind == kBinding)
    {
        KeyName(GetKey(item.mArg), pTexts[kKeyColumn]);
        for (s32 c = kAltColumn; c < kColumns; c++)
        {
            const s32 binding = GetBinding(c, item.mArg);
            c == kAltColumn ? AltName(binding, pTexts[c]) : PadName(binding, pTexts[c]);
        }
        if (sCapturing == item.mArg)
        {
            wcscpy(pTexts[sColumn], Translate(L"PRESS"));
        }
    }
}

// Where Draw puts the page in the 640x480 layout.
struct Layout
{
    Item mItems[kMaxItems];
    f32 mY[kMaxItems]; // each line's top
    s32 mCount;
    bool mControls;
    s32 mColumns;
    f32 mX;
    f32 mColumnX[kNumColumns]; // the first, a setting's value
    f32 mLineStep;
    f32 mPanelLeft;
    f32 mPanelRight;
};

// The first line's top, under the frontend's logo.
const f32 kListTop = 160;
// The statistics' rows, from one's top to the next.
const f32 kStatStep = 24;

// From a line's top to the next, or to one set apart; a save's two rows, and the space after them.
const f32 kLineStep = 16;
const f32 kGapStep = 24;
const f32 kSaveSpace = 6;

f32 Height(const Item& item, f32 lineStep)
{
    return item.mKind == kSave ? lineStep * 2 + kSaveSpace : lineStep;
}

// From a setting's value to the centre of the arrows the mouse changes it with.
const f32 kArrowGap = 14;

// Dim, on two lines of about the same length if it is wider than the page.
void DrawHelp(const wchar_t* pText, f32 x, f32 y, f32 width)
{
    const s16 font = MenuFont();
    wchar_t first[64];
    wcsncpy(first, pText, 63);
    first[63] = 0;
    const wchar_t* pSecond = L"";
    s32 split = -1;
    f32 best = 0;
    const bool wide = Width(pText, font) > width;
    for (s32 i = 0; first[i] && wide; i++)
    {
        if (first[i] == L' ')
        {
            first[i] = 0;
            const f32 longest = Max(Width(first, font), Width(first + i + 1, font));
            first[i] = L' ';
            if (split < 0 || longest < best)
            {
                best = longest;
                split = i;
            }
        }
    }
    if (split >= 0)
    {
        first[split] = 0;
        pSecond = first + split + 1;
    }
    Text(first, x, y, font, false, true);
    Text(pSecond, x, y + kLineStep, font, false, true);
}

void GetLayout(Layout* pLayout)
{
    if (sKeyNames.empty())
    {
        LoadKeyNames();
    }
    Item* pItems = pLayout->mItems;
    const s32 count = Items(sPage, pItems);
    const bool controls = sPage == kControlsPage;
    const bool saves = sPage == kSavesPage;
    const s32 columns = controls ? kColumns : (saves ? kSaveColumns : 1);

    // The first column at its place, or further right for longer names, each a gap after the longest
    // text of the one before.
    const s16 normal = MenuFont();
    f32 nameWidth = 0;
    f32 widths[kNumColumns] = {};
    for (s32 c = 0; c < columns && controls; c++)
    {
        widths[c] = Width(Translate(kColumnNames[c]), normal);
    }
    wchar_t texts[kMaxTexts][kTextLength];
    for (s32 i = 0; i < count; i++)
    {
        if (pItems[i].mKind == kSetting || pItems[i].mKind == kBinding || pItems[i].mKind == kSave || pItems[i].mKind == kRetry)
        {
            ItemValues(pItems[i], texts);
            nameWidth = Max(nameWidth, Width(ItemName(pItems[i]), normal));
            for (s32 c = 0; c < columns; c++)
            {
                widths[c] = Max(widths[c], (c ? 0 : TextOffset(pItems[i])) + Width(texts[c], normal));
                widths[c] = saves ? Max(widths[c], Width(texts[c + kSaveColumns], normal)) : widths[c];
            }
        }
    }
    const f32 x = controls ? 80.0f : (saves ? 60.0f : 140.0f);
    f32 columnX[kNumColumns];
    f32 right;
    // Narrower gaps for a page wider than the screen even moved left.
    for (f32 gap = controls ? 16.0f : 24.0f; gap >= 8; gap -= 4)
    {
        columnX[0] = Max(controls ? 260.0f : (saves ? 0.0f : 360.0f), x + nameWidth + gap);
        for (s32 c = 1; c < columns; c++)
        {
            columnX[c] = columnX[c - 1] + widths[c - 1] + gap;
        }
        right = columnX[columns - 1] + widths[columns - 1];
        if (right - (x - 20) <= 630)
        {
            break;
        }
    }
    // Inside the frontend's screen: the page moves left, down to x 20.
    f32 shift = right - 630;
    shift = shift < 0 ? 0 : (shift > x - 20 ? x - 20 : shift);

    pLayout->mCount = count;
    pLayout->mControls = controls;
    pLayout->mColumns = columns;
    pLayout->mX = x - shift;
    for (s32 c = 0; c < columns; c++)
    {
        pLayout->mColumnX[c] = columnX[c] - shift;
    }
    pLayout->mPanelLeft = pLayout->mX < 60 ? pLayout->mX - 20 : 40;
    pLayout->mPanelRight = Max(600, right - shift + (controls || saves ? 20 : kArrowGap + 16));
    // The controls page closer, its last line above the frontend's left logo.
    pLayout->mLineStep = controls ? kLineStep - 1 : kLineStep;
    // BACK under the statistics.
    f32 y = kListTop + (sPage == kStatsPage ? sStatRows * kStatStep + kGapStep - kLineStep : 0);
    for (s32 i = 0; i < count; i++)
    {
        if (i)
        {
            y += (pItems[i].mGap ? (controls ? kGapStep - 4 : kGapStep) : pLayout->mLineStep) +
                Height(pItems[i - 1], pLayout->mLineStep) - pLayout->mLineStep;
        }
        pLayout->mY[i] = y;
    }
}

// The line under that layout position, -1 if none.
s32 LineAt(const Layout& layout, f32 x, f32 y)
{
    if (x < layout.mPanelLeft || x >= layout.mPanelRight)
    {
        return -1;
    }
    for (s32 i = 0; i < layout.mCount; i++)
    {
        if (y >= layout.mY[i] - 3 && y < layout.mY[i] + Height(layout.mItems[i], layout.mLineStep) - 3)
        {
            return i;
        }
    }
    return -1;
}

bool MouseMode()
{
#if defined(GTA2_PORT_SDL)
    return port_MouseMode();
#else
    return false;
#endif
}

// The keys at the bottom, or the pad's buttons in the frontend's red discs once the pad was used last.
void DrawHints()
{
    struct Hint
    {
        const wchar_t* mKey;
        const wchar_t* mButton;
        const wchar_t* mLabel;
    };
    const Hint kNavigate[] = {{L"ENTER", L"A", L"SELECT"}, {L"ESC", L"B", L"BACK"}};
    const bool pad = PadMode();
    // Capture: B cancels on the keyboard columns, Back on the pad column (B can be bound there); Delete
    // clears a second key or a pad input.
    const bool padColumn = sColumn == kPadColumn;
    const Hint kCapture[] = {{pad && padColumn ? L"BACK" : L"ESC", pad && !padColumn ? L"B" : NULL, L"CANCEL"},
                             {L"DEL", NULL, L"CLEAR"}};
    const bool capturing = sCapturing >= 0;
    const Hint* pHints = capturing ? kCapture : kNavigate;
    const s32 count = capturing && sColumn == kKeyColumn ? 1 : 2;
    const s16 font = MenuFont();
    const s16 buttonFont = gFrontendHeadingFont;
    // Between the frontend's two logos (x 70 to 550): longer translations start further left, then
    // come closer together.
    const f32 kLeft = 70;
    const f32 kRight = 550;
    const f32 kDisc = 34;
    f32 width = 0;
    for (s32 i = 0; i < count; i++)
    {
        const bool button = pad && pHints[i].mButton;
        width += (button ? kDisc : Width(Translate(pHints[i].mKey), font) + 10) + Width(Translate(pHints[i].mLabel), font);
    }
    f32 gap = 40;
    f32 x = kRight - width - gap * (count - 1);
    x = x > 120 ? 120 : (x < kLeft ? kLeft : x);
    if (count > 1 && x + width + gap > kRight)
    {
        gap = kRight - x - width > 10 ? kRight - x - width : 10;
    }

    const f32 y = 440;
    for (s32 i = 0; i < count; i++)
    {
        if (pad && pHints[i].mButton)
        {
            Sprite(kDiscSprite, x + 10, y + 12);
            Text(pHints[i].mButton, x + 10 - Width(pHints[i].mButton, buttonFont) / 2, y, buttonFont);
            x += kDisc;
        }
        else
        {
            const wchar_t* pKey = Translate(pHints[i].mKey);
            Text(pKey, x, y, font);
            x += Width(pKey, font) + 10;
        }
        const wchar_t* pLabel = Translate(pHints[i].mLabel);
        Text(pLabel, x, y, font);
        x += Width(pLabel, font) + gap;
    }
}

// Drops the mouse's moves and clicks so far.
void DiscardMouse()
{
#if defined(GTA2_PORT_SDL)
    float x;
    float y;
    int left;
    int right;
    int wheel;
    port_TakeMouse(&x, &y, &left, &right, &wheel);
#endif
}
} // namespace

bool PadMode()
{
#if defined(GTA2_PORT_SDL)
    return port_PadMode();
#else
    return false;
#endif
}

void Open()
{
    sPage = kMainPage;
    sSelected[kMainPage] = 0;
    sRetry = PortSaves::kNoCheckpoint;
    sStatRows = 0;
    sCapturing = -1;
    sDragging = false;
    DiscardMouse();
#if defined(GTA2_PORT_SDL)
    port_DeferRegistrySaves(true);
#endif
}

void OfferRetry(PortSaves::Checkpoint kind, s32 money, long long time, bool selected)
{
    sRetry = kind;
    sRetryMoney = money;
    sRetryTime = time;
    sSelected[kMainPage] = kind != PortSaves::kNoCheckpoint && !selected ? 1 : 0;
}

void OfferStatistics(s32 rows)
{
    sStatRows = rows;
}

bool OnStatistics(f32* pTop, f32* pStep)
{
    *pTop = kListTop;
    *pStep = kStatStep;
    return sPage == kStatsPage;
}

void OpenSaves(const PortSaves::Entry* pEntries, s32 count)
{
    Open();
    sNumSaves = count < PortSaves::kMaxSaves ? count : PortSaves::kMaxSaves;
    memcpy(sSaves, pEntries, sNumSaves * sizeof(sSaves[0]));
    sPage = kSavesPage;
    sSelected[kSavesPage] = 0;
}

bool OnSaves()
{
    return sPage == kSavesPage;
}

const PortSaves::Entry& SelectedSave()
{
    return sSaves[sSelected[kSavesPage] < sNumSaves ? sSelected[kSavesPage] : 0];
}

void Close()
{
#if defined(GTA2_PORT_SDL)
    port_DeferRegistrySaves(false);
#endif
}

Result HandleInput(Input input)
{
    Item items[kMaxItems];
    const s32 count = Items(sPage, items);
    s32& selected = sSelected[sPage];
    const Item& item = items[selected];
    switch (input)
    {
        case kUp:
            selected = (selected + count - 1) % count;
            return kMovedUp;
        case kDown:
            selected = (selected + 1) % count;
            return kMovedDown;
        case kLeft:
        case kRight:
            if (!IsEnabled(item))
            {
                return kNothing;
            }
            if (item.mKind == kSetting)
            {
                PortSettings::Change(static_cast<PortSettings::Id>(item.mArg), input == kLeft ? -1 : 1);
                return input == kLeft ? kLowered : kRaised;
            }
#if defined(GTA2_PORT_SDL)
            if (item.mKind == kBinding)
            {
                const s32 column = sColumn + (input == kLeft ? -1 : 1);
                if (column < 0 || column >= kColumns)
                {
                    return kNothing;
                }
                sColumn = column;
                return input == kLeft ? kLowered : kRaised;
            }
#endif
            return kNothing;
        case kSelect:
            switch (item.mKind)
            {
                case kGoTo:
                    GoTo(static_cast<Page>(item.mArg));
                    return kEntered;
                case kSetting:
                    if (!IsEnabled(item))
                    {
                        return kNothing;
                    }
                    PortSettings::Change(static_cast<PortSettings::Id>(item.mArg), 1);
                    return kRaised;
                case kBinding:
#if defined(GTA2_PORT_SDL)
                    port_CaptureStart();
#else
                    if (!gFrontend)
                    {
                        return kNothing;
                    }
                    memcpy(sPrevKeys, gFrontend->mKeys, sizeof(sPrevKeys));
#endif
                    sCapturing = item.mArg;
                    return kEntered;
                case kDefaultKeys:
                    DefaultKeys();
                    return kEntered;
                case kBack:
                    return Back();
                case kResume:
                    return kClosed;
                case kSave:
                    return kLoadSave;
                case kRetry:
                    return kRetryCheckpoint;
                default:
                    return kQuitGame;
            }
        default:
            return Back();
    }
}

bool IsCapturing()
{
    return sCapturing >= 0;
}

Result UpdateCapture()
{
    if (sCapturing < 0)
    {
        return kNothing;
    }
    // Escape cancels; B cancels on the keyboard columns, Back on the pad column.
#if defined(GTA2_PORT_SDL)
    const s32 key = port_CapturedKey();
    const s32 pad = port_CapturedPad();
#else
    const s32 key = FrontendKeyPressed();
    const s32 pad = -1;
#endif
    const bool padColumn = sColumn == kPadColumn;
    if (key == DIK_ESCAPE || (!padColumn && pad == kPadB) || (padColumn && pad == kPadBack))
    {
        sCapturing = -1;
        return kWentBack;
    }
    if (sColumn != kKeyColumn && (key == DIK_DELETE || key == DIK_BACK))
    {
        SetBinding(sColumn, sCapturing, padColumn ? -1 : 0);
        sCapturing = -1;
        return kLowered;
    }
    const s32 binding = padColumn ? pad : key;
    if (binding < 0 || (!padColumn && binding == 0) || (sColumn == kKeyColumn && binding >= kMouseCode))
    {
        return kNothing;
    }
    Bind(sColumn, sCapturing, binding);
    sCapturing = -1;
    // A click bound stays out of the menu.
    DiscardMouse();
    return kEntered;
}

Result HandleMouse(f32 scale, f32 left, f32 top)
{
#if defined(GTA2_PORT_SDL)
    float frameX;
    float frameY;
    int leftClicks;
    int rightClicks;
    int wheel;
    const bool moved = port_TakeMouse(&frameX, &frameY, &leftClicks, &rightClicks, &wheel);
    if (sCapturing >= 0)
    {
        // The right button cancels, but binds as a second key.
        if (rightClicks && sColumn != kAltColumn)
        {
            sCapturing = -1;
            return kWentBack;
        }
        return kNothing;
    }
    if (rightClicks)
    {
        sDragging = false;
        return Back();
    }
    const f32 x = (frameX - left) / scale;
    const f32 y = (frameY - top) / scale;
    Layout layout;
    GetLayout(&layout);
    s32& selected = sSelected[sPage];
    sDragging = sDragging && port_MouseLeftDown();
    if (sDragging)
    {
        return SetSliderAt(layout.mItems[selected], x, layout.mColumnX[0]);
    }
    const s32 line = LineAt(layout, x, y);
    if (line < 0)
    {
        return kNothing;
    }
    const Item& item = layout.mItems[line];
    Result result = kNothing;
    if (moved || leftClicks || wheel)
    {
        if (line != selected)
        {
            result = line < selected ? kMovedUp : kMovedDown;
            selected = line;
        }
        // Over a binding's input, that column.
        for (s32 c = 0; c < layout.mColumns && item.mKind == kBinding; c++)
        {
            sColumn = x >= layout.mColumnX[c] - kArrowGap - 6 ? c : sColumn;
        }
    }
    if (!IsEnabled(item))
    {
        return result;
    }
    Slider slider;
    if (leftClicks && GetSlider(item, &slider) && x >= layout.mColumnX[0] - 6 && x < layout.mColumnX[0] + kSliderWidth + 6)
    {
        sDragging = true;
        const Result changed = SetSliderAt(item, x, layout.mColumnX[0]);
        return changed != kNothing ? changed : result;
    }
    if (item.mKind == kSetting && (wheel || leftClicks))
    {
        // The left arrow lowers, the rest of the line raises.
        const bool lower = wheel ? wheel < 0 : x >= layout.mColumnX[0] - kArrowGap * 2 && x < layout.mColumnX[0] - 2;
        PortSettings::Change(static_cast<PortSettings::Id>(item.mArg), lower ? -1 : 1);
        return lower ? kLowered : kRaised;
    }
    if (leftClicks)
    {
        return HandleInput(kSelect);
    }
    return result;
#else
    return kNothing;
#endif
}

const wchar_t* Title()
{
    return Translate(kTitles[sPage]);
}

void Draw(f32 scale, f32 left, f32 top, bool panel)
{
    sScale = scale;
    sLeft = left;
    sTop = top;
    Layout layout;
    GetLayout(&layout);
    const s16 normal = MenuFont();
    if (panel)
    {
        Rect(layout.mPanelLeft, 130, layout.mPanelRight, 470, 0);
    }
    for (s32 c = 0; c < layout.mColumns && layout.mControls; c++)
    {
        Text(Translate(kColumnNames[c]), layout.mColumnX[c], 140, normal);
    }
    wchar_t texts[kMaxTexts][kTextLength];
    for (s32 i = 0; i < layout.mCount; i++)
    {
        const Item& item = layout.mItems[i];
        const f32 y = layout.mY[i];
        const bool selected = i == sSelected[sPage];
        const bool dim = !IsEnabled(item);
        ItemValues(item, texts);
        Text(ItemName(item), layout.mX, y, normal, selected, dim);
        Slider slider;
        if (GetSlider(item, &slider))
        {
            DrawSlider(slider, layout.mColumnX[0], y, selected, dim);
        }
        const f32 valueX = layout.mColumnX[0] + TextOffset(item);
        Text(texts[0], valueX, y, normal, selected, dim);
        const f32 underPage = layout.mY[layout.mCount - 1] + kGapStep + 4;
        if (selected && dim)
        {
            DrawRequirement(item, layout.mX, underPage);
        }
        const wchar_t* pHelp = item.mKind == kSetting ? PortSettings::Help(static_cast<PortSettings::Id>(item.mArg)) : NULL;
        if (selected && !dim && pHelp)
        {
            DrawHelp(Translate(pHelp), layout.mX, underPage, layout.mPanelRight - 10 - layout.mX);
        }
        for (s32 c = 1; c < layout.mColumns && (item.mKind == kBinding || item.mKind == kSave); c++)
        {
            Text(texts[c], layout.mColumnX[c], y, normal, selected);
        }
        for (s32 c = 0; c < kSaveColumns && item.mKind == kSave; c++)
        {
            Text(texts[c + kSaveColumns], layout.mColumnX[c], y + layout.mLineStep, normal, selected, !selected);
        }
        if (selected && item.mKind == kBinding && layout.mColumns > 1)
        {
            Sprite(kArrowSprite, layout.mColumnX[sColumn] - 12, y + 6);
        }
        if (selected && item.mKind == kSetting && !dim && MouseMode())
        {
            Sprite(kLeftArrowSprite, layout.mColumnX[0] - kArrowGap, y + 6);
            Sprite(kArrowSprite, valueX + Width(texts[0], normal) + kArrowGap, y + 6);
        }
    }
    DrawHints();
}
} // namespace PortMenu
