#include "options_menu.hpp"
#include "Function.hpp"
#include "GameSession.hpp"
#include "Draw.hpp"
#include "Globals.hpp"
#include "PlyDat.hpp"
#include "SoundObject.hpp"
#include "TextTable.hpp"
#include "menu.hpp"
#include "reg2_version.h"
#include "retry.hpp"
#include "settings.hpp"
#include "welcome.hpp"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#if defined(GTA2_UPDATE_CHECK)
#include "registry.hpp"
#include <SDL3/SDL.h>
#endif

// No std::wstring: with -fshort-wchar its traits would call the C library's 4-byte wide functions.

EXTERN_GLOBAL(SoundObject, snd1);

#if defined(GTA2_PORT_SDL)
bool port_TakeMouse(float* pX, float* pY, int* pLeft, int* pRight, int* pWheel);
#endif

namespace PortOptionsMenu
{
namespace
{
const u16 kMenuPage = 12;
bool sShown;
// The page and option the list of saves goes back to, and the save chosen in it.
u16 sSavesReturnPage;
u16 sSavesReturnOption;
Frontend* sListing;
char_type sResumeFile[64];

// The delete player dialog's type for a new game that replaces an autosave, past the original's 1, and its lines'
// y as HandleDeletePlayerDialog has them.
const u8 kReplaceDialog = 2;
const u16 kSureY = 190;
const u16 kYesY = 210;
const u16 kNoY = 230;
// Before the original pages took the input, put back if the new game is not confirmed; and the boot's screen.
GameSession sSessionBefore;
char_type sScreenBefore;
char_type sBootScreen;

s32 Wrap(s32 i, s32 n)
{
    return (i % n + n) % n;
}

// The frontend fonts have no hyphen and no lower case.
void Widen(const char* pText, wchar_t* pOut)
{
    for (; *pText; pText++)
    {
        *pOut++ = static_cast<wchar_t>(*pText == '-' ? ' ' : toupper(static_cast<unsigned char>(*pText)));
    }
    *pOut = 0;
}

#if defined(GTA2_UPDATE_CHECK)
// x.y.z as one number, 0 if the text does not start with one.
s32 VersionNumber(const char* pText)
{
    s32 x, y, z;
    return sscanf(pText, "%d.%d.%d", &x, &y, &z) == 3 ? x * 1000000 + y * 1000 + z : 0;
}

// The latest release's version number, -1 while it is fetched, and the day it is fetched on.
SDL_AtomicInt sLatest;
s32 sFetchDay = -1;

// With curl, else wget. GTA2_UPDATE_URL replaces GitHub's API in tests.
int SDLCALL FetchLatest(void*)
{
    const char* url = SDL_getenv("GTA2_UPDATE_URL") ? SDL_getenv("GTA2_UPDATE_URL") : "https://api.github.com/repos/reG2-project/reG2/releases/latest";
    const char* commands[][6] = {{"curl", "-fsSL", "--max-time", "10", url, NULL}, {"wget", "-qO-", "--timeout=10", url, NULL}};
    char* pOut = NULL;
    for (s32 i = 0; i < 2 && !pOut; i++)
    {
        const SDL_PropertiesID props = SDL_CreateProperties();
        SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, commands[i]);
        SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP);
#if defined(SDL_PLATFORM_WINDOWS)
        // Else Windows opens a console window for curl.
        SDL_SetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_BACKGROUND_BOOLEAN, true);
#endif
        SDL_Process* pProcess = SDL_CreateProcessWithProperties(props);
        SDL_DestroyProperties(props);
        pOut = pProcess ? static_cast<char*>(SDL_ReadProcess(pProcess, NULL, NULL)) : NULL;
        SDL_DestroyProcess(pProcess);
    }
    const char* pTag = pOut ? SDL_strstr(pOut, "\"tag_name\"") : NULL;
    pTag = pTag ? SDL_strstr(pTag + 10, "\"v") : NULL;
    SDL_SetAtomicInt(&sLatest, pTag ? VersionNumber(pTag + 2) : 0);
    SDL_free(pOut);
    return 0;
}

// At most once a day: the last check's day and result are kept in the registry.
void StartUpdateCheck()
{
    SDL_Time now = 0;
    SDL_GetCurrentTime(&now);
    const s32 today = static_cast<s32>(now / (86400 * SDL_NS_PER_SECOND));
    if (PortSettings::UpdateCheck() && gRegistry.Get_Screen_Setting("update_day", 0) == today)
    {
        SDL_SetAtomicInt(&sLatest, gRegistry.Get_Screen_Setting("update_latest", 0));
    }
    else if (PortSettings::UpdateCheck())
    {
        SDL_SetAtomicInt(&sLatest, -1);
        sFetchDay = today;
        SDL_DetachThread(SDL_CreateThread(FetchLatest, "update check", NULL));
    }
}

// A newer release's version number, else 0.
s32 NewerRelease()
{
    const s32 latest = SDL_GetAtomicInt(&sLatest);
    if (sFetchDay >= 0 && latest >= 0)
    {
        gRegistry.Set_Screen_Setting("update_day", sFetchDay);
        gRegistry.Set_Screen_Setting("update_latest", latest);
        sFetchDay = -1;
    }
    return latest > VersionNumber(GTA2_VERSION) ? latest : 0;
}
#endif

PortMenu::Result HandleInput(Frontend* pFrontend)
{
    if (PortMenu::IsCapturing())
    {
        const PortMenu::Result result = PortMenu::UpdateCapture();
        return result != PortMenu::kNothing ? result : PortMenu::HandleMouse(1.0f, 0.0f, 0.0f);
    }
    const struct
    {
        bool Frontend::*mPressed;
        PortMenu::Input mInput;
    } kInputs[] = {{&Frontend::mReturnPressed, PortMenu::kSelect}, {&Frontend::mEscapePressed, PortMenu::kCancel},
                   {&Frontend::mUpPressed, PortMenu::kUp},          {&Frontend::mDownPressed, PortMenu::kDown},
                   {&Frontend::mLeftPressed, PortMenu::kLeft},      {&Frontend::mRightPressed, PortMenu::kRight}};
    for (const auto& input : kInputs)
    {
        if (pFrontend->*input.mPressed)
        {
            return PortMenu::HandleInput(input.mInput);
        }
    }
    return PortMenu::HandleMouse(1.0f, 0.0f, 0.0f);
}

#if defined(GTA2_PORT_SDL)
bool Inside(f32 x, f32 y, f32 left, f32 top, f32 right, f32 bottom)
{
    return x >= left && x < right && y >= top && y < bottom;
}

// The original pages with the mouse, as their keys: pointing at an option selects it, a click enters
// it, the right button goes back, the wheel and the arrows beside an option change it.
void OriginalPageMouse(Frontend* pFrontend)
{
    float x;
    float y;
    int left;
    int right;
    int wheel;
    const bool moved = port_TakeMouse(&x, &y, &left, &right, &wheel);
    if (right)
    {
        pFrontend->mEscapePressed = true;
        return;
    }
#if defined(GTA2_UPDATE_CHECK)
    if (left && pFrontend->mCurrentPageIdx == MENUPAGE_START_MENU && NewerRelease() && Inside(x, y, 300.0f, 450.0f, 640.0f, 480.0f))
    {
        SDL_OpenURL("https://github.com/reG2-project/reG2/releases/latest");
        return;
    }
#endif
    MenuPage* pPage = &pFrontend->mMenuPagesArray[pFrontend->mCurrentPageIdx];
    for (s32 i = 0; i < pPage->mNumberOfElements && left; i++)
    {
        const MenuElement& element = pPage->mElementsArray[i];
        const u16 shape = element.mGeometricShapeType;
        if (element.mElementType == GEOMETRIC_SHAPE_3 && element.mIsItDisplayed && (shape == 1 || shape == 2) &&
            Inside(x, y, element.mXpos - 12.0f, element.mYpos - 12.0f, element.mXpos + 12.0f, element.mYpos + 12.0f))
        {
            (shape == 1 ? pFrontend->mLeftPressed : pFrontend->mRightPressed) = true;
            return;
        }
    }
    s32 option = -1;
    for (s32 i = 0; i < pPage->mNumberOfOptions && option < 0; i++)
    {
        MenuItem& item = pPage->mOptionsArray[i];
        if (!item.mIsUnlocked)
        {
            continue;
        }
        wchar_t* pText = item.mOptionNameStr;
        if (item.mOptionType == HORIZONTAL_SELECTION_2)
        {
            pFrontend->GetOptionText(pFrontend->mCurrentPageIdx, static_cast<u16>(i), &pText);
        }
        const u16 font = i == pPage->mCurrentOptionIdx ? pFrontend->mSelectedFont
                                                        : (item.mFontType != 0xFFFF ? item.mFontType : pFrontend->mNormalFont);
        const f32 width = static_cast<f32>(Frontend::GetMaxTextWidth(pText, font));
        if (Inside(x, y, item.mXPos - 4.0f, item.mYPos - 2.0f, item.mXPos + width + 4.0f, item.mYPos + 18.0f))
        {
            option = i;
        }
    }
    if (option < 0 || !(moved || left || wheel))
    {
        return;
    }
    if (option != pPage->mCurrentOptionIdx)
    {
        snd1.mObjectType = option < pPage->mCurrentOptionIdx ? PortMenu::kMovedUp : PortMenu::kMovedDown;
        pPage->mCurrentOptionIdx = static_cast<u16>(option);
    }
    if (wheel)
    {
        (wheel < 0 ? pFrontend->mLeftPressed : pFrontend->mRightPressed) = true;
    }
    else if (left)
    {
        pFrontend->mReturnPressed = true;
    }
}
#endif
// gFrontend is not set yet while the frontend sets its pages up.
bool IsListingLevel(const PortSaves::Header& header)
{
    return IsLevel(sListing, header);
}

s32 ListSaves(Frontend* pFrontend, PortSaves::Entry* pEntries)
{
    sListing = pFrontend;
    return PortSaves::List(gGameSession.GetPlySlotIdx(), IsListingLevel, pEntries);
}

void Boot(Frontend* pFrontend, const char_type* pFileName)
{
    snprintf(sResumeFile, sizeof(sResumeFile), "%s", pFileName);
    pFrontend->LoadMapFilenamesFromSvg();
    pFrontend->mMenuScreen = RedBar_16;
    pFrontend->mState = FrontendState::Booting_Map_2;
}
} // namespace

void Setup(Frontend* pFrontend)
{
    MenuPage* pPage = &pFrontend->mMenuPagesArray[kMenuPage];
    // The menu draws its own lines; ChangeMenuPage only needs option 0 to be selectable.
    pPage->mNumberOfOptions = 0;
    pPage->mOptionsArray[0].mIsUnlocked = 1;
    pPage->mCurrentOptionIdx = 0;
    pPage->mDefaultOptionIdx = 0;

    MenuElement& title = pPage->mElementsArray[0];
    pPage->mNumberOfElements = 1;
    title.mElementType = STRING_TEXT_1;
    title.mXpos = 35;
    title.mYpos = 11;
    title.mIsItDisplayed = 1;
    title.mFontType = pFrontend->mTitleFont;
    title.mFontPalette = 5;

    PortSettings::ApplyRenderOptions();
#if defined(GTA2_UPDATE_CHECK)
    StartUpdateCheck();
#endif

    // The start menu's "Options" opened GTA2 Manager.
    pFrontend->mMenuPagesArray[MENUPAGE_START_MENU].mOptionsArray[1].mMenuPageTarget = kMenuPage;
}

bool Update(Frontend* pFrontend)
{
    if (PortWelcome::Update(pFrontend))
    {
        return true;
    }
    if (pFrontend->mCurrentPageIdx != kMenuPage)
    {
        if (sShown)
        {
            PortMenu::Close();
        }
        sShown = false;
#if defined(GTA2_PORT_SDL)
        OriginalPageMouse(pFrontend);
#endif
        sSessionBefore = gGameSession;
        sScreenBefore = pFrontend->mMenuScreen;
        return false;
    }
    if (!sShown)
    {
        PortMenu::Open();
        sShown = true;
    }
    pFrontend->mMenuScreen = RedBar_16;
    PortMenu::Result result = HandleInput(pFrontend);
    // LOAD GAME's cities: also Page Up and Page Down, the pad's LB and RB.
    static bool sPrevPageKeys[2];
    const u8 pageKeys[2] = {DIK_PRIOR, DIK_NEXT};
    for (s32 i = 0; i < 2; i++)
    {
        const bool down = (pFrontend->mKeys[pageKeys[i]] & 0x80) != 0;
        if (down && !sPrevPageKeys[i] && result == PortMenu::kNothing && PortMenu::OnSaves())
        {
            result = PortMenu::HandleInput(i ? PortMenu::kRight : PortMenu::kLeft);
        }
        sPrevPageKeys[i] = down;
    }
    if (result == PortMenu::kLoadSave)
    {
        PortMenu::Close();
        sShown = false;
        Boot(pFrontend, PortMenu::SelectedSave().mFileName);
        snd1.mObjectType = PortMenu::kEntered;
        return true;
    }
    if (result == PortMenu::kClosed && PortMenu::OnSaves())
    {
        pFrontend->ChangeMenuPage(sSavesReturnPage);
        pFrontend->mMenuPagesArray[sSavesReturnPage].mCurrentOptionIdx = sSavesReturnOption;
        snd1.mObjectType = PortMenu::kWentBack;
    }
    else if (result == PortMenu::kClosed)
    {
        pFrontend->ChangeMenuPage(MENUPAGE_START_MENU);
        snd1.mObjectType = PortMenu::kWentBack;
    }
    else if (result != PortMenu::kNothing)
    {
        snd1.mObjectType = result;
    }
    wcsncpy(pFrontend->mMenuPagesArray[kMenuPage].mElementsArray[0].mElementNameStr, PortMenu::Title(), 50);
    return true;
}

void AfterPageInput(Frontend* pFrontend)
{
    if (pFrontend->mState != FrontendState::Booting_Map_2 || gGameSession.GetSaveName()[0] || gGameSession.IsBonusStage() ||
        !PortSettings::Autosave() ||
        !PortSaves::HasAutosave(gGameSession.GetPlySlotIdx(), static_cast<u8>(gGameSession.GetMainStageIdx())))
    {
        return;
    }
    sBootScreen = pFrontend->mMenuScreen;
    pFrontend->mMenuScreen = sScreenBefore;
    pFrontend->mState = FrontendState::Delete_Player_Dialog_4;
    pFrontend->mDialogType = kReplaceDialog;
    pFrontend->mDialogCursorYpos = kSureY;
}

bool UpdateDialog(Frontend* pFrontend)
{
    if (pFrontend->mDialogType != kReplaceDialog)
    {
        return false;
    }
    u16& cursor = pFrontend->mDialogCursorYpos;
#if defined(GTA2_PORT_SDL)
    // YEAH! and NO WAY! with the mouse, as the original pages' options.
    float x;
    float y;
    int left;
    int right;
    int wheel;
    const bool moved = port_TakeMouse(&x, &y, &left, &right, &wheel);
    pFrontend->mEscapePressed = pFrontend->mEscapePressed || right;
    const u16 lines[2] = {kYesY, kNoY};
    const char* const keys[2] = {"yes", "no"};
    for (s32 i = 0; i < 2 && (moved || left); i++)
    {
        const f32 width = static_cast<f32>(Frontend::GetMaxTextWidth(gText_0x14->Find(keys[i]), pFrontend->mSelectedFont));
        if (Inside(x, y, 296.0f, lines[i] - 2.0f, 304.0f + width, lines[i] + 18.0f))
        {
            if (cursor != lines[i])
            {
                snd1.mObjectType = lines[i] < cursor ? PortMenu::kMovedUp : PortMenu::kMovedDown;
                cursor = lines[i];
            }
            pFrontend->mReturnPressed = pFrontend->mReturnPressed || left;
        }
    }
#endif
    if (pFrontend->mReturnPressed && cursor == kYesY)
    {
        pFrontend->mMenuScreen = sBootScreen;
        pFrontend->mState = FrontendState::Booting_Map_2;
        snd1.mObjectType = PortMenu::kEntered;
    }
    else if (pFrontend->mEscapePressed || (pFrontend->mReturnPressed && cursor == kNoY))
    {
        gGameSession = sSessionBefore;
        pFrontend->mMenuScreen = sScreenBefore;
        pFrontend->mState = FrontendState::Menu_Navigation_1;
        snd1.mObjectType = PortMenu::kWentBack;
    }
    else if (pFrontend->mUpPressed)
    {
        cursor = cursor == kNoY ? kYesY : kNoY;
        snd1.mObjectType = PortMenu::kMovedUp;
    }
    else if (pFrontend->mDownPressed)
    {
        cursor = cursor == kYesY ? kNoY : kYesY;
        snd1.mObjectType = PortMenu::kMovedDown;
    }
    return true;
}

void DrawDialog(Frontend* pFrontend)
{
    if (pFrontend->mDialogType != kReplaceDialog)
    {
        return;
    }
    static const wchar_t* const kCities[PortSaves::kNumCities] = {L"DOWNTOWN", L"RESIDENTIAL", L"INDUSTRIAL"};
    const u8 city = static_cast<u8>(gGameSession.GetMainStageIdx());
    wchar_t text[128];
    swprintf(text, PortSettings::Translate(L"YOUR %ls AUTOSAVE WILL BE REPLACED"),
             PortSettings::Translate(kCities[city < PortSaves::kNumCities ? city : 0]));
    // The small font has no hyphen.
    for (wchar_t* p = text; *p; p++)
    {
        *p = *p == L'-' ? L' ' : *p;
    }
    // In the title bar as the original dialog's DELETE PLAYER, then over the lines, ending above them.
    DrawStringDefaultPalette(PortSettings::Translate(L"AUTOSAVE"), Fix16(275), Fix16(gText_0x14->LangIsJapanese() ? 16 : 12),
                             pFrontend->mHeadingFont, Fix16(1));
    const u16 font = static_cast<u16>(gFrontendFont7);
    // Up to the screen's right edge, on lines of about the same width.
    const s32 kWidth = 330;
    const s32 total = Frontend::GetMaxTextWidth(text, font);
    const s32 rows = (total + kWidth - 1) / kWidth;
    const s32 wrap = rows > 1 && total / rows + 40 < kWidth ? total / rows + 40 : kWidth;
    wchar_t lines[160];
    const s32 count = TextTable::InsertLineBreaksAndGetNumLines(lines, text, wrap, font);
    DrawStringDefaultPalette(lines, Fix16(300), Fix16(kSureY - 8 - count * GetLineSpacingFromFontType(font)), font, Fix16(1));
}

void Draw(Frontend* pFrontend)
{
    if (pFrontend->mCurrentPageIdx == kMenuPage)
    {
        PortMenu::Draw(1.0f, 0.0f, 0.0f, false);
    }
}

void VersionText(wchar_t* pOut)
{
    char text[32];
    snprintf(text, sizeof(text), "REG2 %s", GTA2_VERSION);
    Widen(text, pOut);
}

void DrawUpdate(wchar_t* pVersion, s16 font)
{
#if defined(GTA2_UPDATE_CHECK)
    if (const s32 newer = NewerRelease())
    {
        char text[32];
        snprintf(text, sizeof(text), "  %d.%d.%d AVAILABLE!", newer / 1000000, newer / 1000 % 1000, newer % 1000);
        wchar_t wide[32];
        Widen(text, wide);
        // Remap 10 gives this font the red of the selected menu option.
        DrawString(wide, 300 + Frontend::GetMaxTextWidth(pVersion, font), 460, font, 1, palette_types_enum::font_remaps_8, 10, 0, 0);
    }
#endif
}

bool UpdateNameEntry(Frontend* pFrontend)
{
    if (!PortMenu::PadMode())
    {
        return false;
    }
    static const wchar_t kLetters[] = L"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ";
    const s32 numLetters = static_cast<s32>(wcslen(kLetters));
    wchar_t* pName = pFrontend->mCurrPlyrName;
    u8& len = pFrontend->mCurrPlyrNameLength;
    if (len == 0)
    {
        pName[len++] = L'A';
    }
    if (pFrontend->mReturnPressed || pFrontend->mEscapePressed)
    {
        pFrontend->mState = FrontendState::Menu_Navigation_1;
        pFrontend->StripPlayerNameToCurrLength();
        if (pFrontend->mReturnPressed)
        {
            pFrontend->SaveAndUpdatePlayerName();
        }
        pFrontend->mMenuPagesArray[MENUPAGE_PLAY].mCurrentOptionIdx = 0;
        snd1.mObjectType = pFrontend->mReturnPressed ? 5 : 6;
        return true;
    }
    if (pFrontend->mUpPressed || pFrontend->mDownPressed)
    {
        s32 i = 0;
        while (i < numLetters && kLetters[i] != pName[len - 1])
        {
            i++;
        }
        i = Wrap((i < numLetters ? i : 0) + (pFrontend->mUpPressed ? 1 : -1), numLetters);
        pName[len - 1] = kLetters[i];
        snd1.mObjectType = 7;
    }
    else if (pFrontend->mRightPressed && len < 8)
    {
        pName[len] = pName[len - 1] == L' ' ? L'A' : pName[len - 1];
        len++;
        snd1.mObjectType = 7;
    }
    else if (pFrontend->mLeftPressed && len > 1)
    {
        len--;
        snd1.mObjectType = 8;
    }
    pFrontend->StripPlayerNameToCurrLength();
    return true;
}

void PadKeyHint(u8 hint, wchar_t* pText)
{
    if (!PortMenu::PadMode())
    {
        return;
    }
    switch (hint)
    {
        case 0: // ESC
            wcscpy(pText, L"B");
            break;
        case 1: // DEL
            wcscpy(pText, L"X");
            break;
        case 2: // ENTER
            wcscpy(pText, L"A");
            break;
        case 3: // BACKSPACE
            wcscpy(pText, L"LEFT");
            break;
        default:
            break;
    }
}

bool IsLevel(Frontend* pFrontend, const PortSaves::Header& header)
{
    const MainBlockStrings& level = pFrontend->mBlocks[header.mMainStage][header.mBonusStage];
    return !strncmp(header.mMapName, "data\\", 5) && !strcmp(header.mMapName + 5, level.mGmpName) &&
        !strncmp(header.mStyleName, "data\\", 5) && !strcmp(header.mStyleName + 5, level.mStyName) &&
        !strncmp(header.mScriptName, "data\\", 5) && !strcmp(header.mScriptName + 5, level.mScrName);
}

bool HasSaves(Frontend* pFrontend)
{
    PortSaves::Entry saves[PortSaves::kMaxSaves];
    return ListSaves(pFrontend, saves) > 0;
}

bool HasSave(Frontend* pFrontend, u8 mainStage, u8 bonusStage)
{
    PortSaves::Entry saves[PortSaves::kMaxSaves];
    const s32 count = ListSaves(pFrontend, saves);
    for (s32 i = 0; i < count; i++)
    {
        if (saves[i].mMainStage == mainStage && saves[i].mBonusStage == bonusStage)
        {
            return true;
        }
    }
    return false;
}

bool Resume(Frontend* pFrontend)
{
    PortSaves::Entry saves[PortSaves::kMaxSaves];
    const s32 count = ListSaves(pFrontend, saves);
    char_type latest[64];
    snprintf(latest, sizeof(latest), "player\\plyslot%d.svg", gGameSession.GetPlySlotIdx());
    if (count == 1 && strcmp(saves[0].mFileName, latest))
    {
        Boot(pFrontend, saves[0].mFileName);
        return true;
    }
    if (count < 2)
    {
        return false;
    }
    sSavesReturnPage = pFrontend->mCurrentPageIdx;
    sSavesReturnOption = pFrontend->mMenuPagesArray[sSavesReturnPage].mCurrentOptionIdx;
    bool cities[PortSaves::kNumCities];
    PlySlot* pStats = pFrontend->GetCurrPlayerStats();
    for (u8 city = 0; city < PortSaves::kNumCities; city++)
    {
        cities[city] = city < pFrontend->mNumMainStages && pStats->mPlyrStageStats[city][0].mIsStageUnlocked &&
            FreeLoader::CheckCityInstalled(city);
    }
    PortMenu::OpenSaves(saves, count, cities);
    sShown = true;
    pFrontend->ChangeMenuPage(kMenuPage);
    return true;
}

void AfterGame(Frontend* pFrontend)
{
    char_type name[64];
    PortSaves::CheckpointName(gGameSession.GetPlySlotIdx(), name, sizeof(name));
    PortSaves::Header header;
    if (PortRetry::TakeRetry() && PortSaves::IsValid(name, &header) && IsLevel(pFrontend, header))
    {
        Boot(pFrontend, name);
    }
}

void ResumeFileName(char_type* pFileName)
{
    if (sResumeFile[0])
    {
        strcpy(pFileName, sResumeFile);
        sResumeFile[0] = 0;
    }
}
} // namespace PortOptionsMenu
