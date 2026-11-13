#include "autosave.hpp"
#include "frame_loop.hpp"
#include "Camera.hpp"
#include "Car.hpp"
#include "Draw.hpp"
#include "Frontend.hpp"
#include "Game.hpp"
#include "GameSession.hpp"
#include "Globals.hpp"
#include "Hud.hpp"
#include "Ped.hpp"
#include "Player.hpp"
#include "ScriptManager.hpp"
#include "map_0x370.hpp"
#include "saves.hpp"
#include "settings.hpp"
#include "sprite.hpp"
#include "winmain.hpp"
#include <math.h>

#if defined(GTA2_PORT_SDL)
#include <SDL3/SDL_log.h>
#endif

namespace PortAutosave
{
namespace
{
// At 30 steps a second: past the job's or frenzy's end messages, then the player safe for a second.
const s32 kWaitSteps = 90;
const s32 kSafeSteps = 30;
const s32 kShowSteps = 45;
// Quitting saves once a job or frenzy ended, or after this much play: less is a look around, which must not
// replace an autosave of a longer game.
const s32 kQuitSaveSteps = 2 * 60 * 30;

bool sPending;
s32 sWaited;
s32 sSafe;
s32 sShown;
s32 sLastMission;
bool sLastFrenzy;
const char* sPendingWhy;
bool sJobEnded;
s32 sPlayed;

// As at a church: off a job and out of a kill frenzy. Then on foot and on firm ground, or driving a car
// out of water (stopped unless quitting), and neither dead, arrested nor burning.
bool Safe(bool quitting)
{
    if (!PortSettings::Autosave() || !gGame_0x40 || !gScriptManager || IsNetworkGame() || gGameSession.IsBonusStage() ||
        gGame_0x40->IsExitTimerSet())
    {
        return false;
    }
    const s32* pMission = gScriptManager->mMissionFlag;
    Player* pPlayer = gGame_0x40->mLocalPlayer;
    Ped* pPed = pPlayer ? pPlayer->mPlayerPed : NULL;
    if (!pMission || *pMission || gScriptManager->IsKillFrenzyActive() || !pPed || pPlayer->mWastedOrBusted ||
        pPed->get_health() <= 0 || pPed->IsOnFire())
    {
        return false;
    }
    if (pPed->mPedState1 == ped_state_1::in_car_10)
    {
        Car* pCar = pPed->mCar;
        return pCar && pCar->mDriver == pPed && !pCar->IsTrainModel() && !pCar->mCarSprite->GetWaterCornerMask() &&
            (quitting || pCar->GetCarLinearSpeed().mValue < 0x200);
    }
    const s32 state = pPed->mPedState1;
    return (state == ped_state_1::walking_0 || state == ped_state_1::flee_or_running_1 ||
            state == ped_state_1::standing_still_7) &&
        !pPlayer->mJumping &&
        !gMap_0x370->IsWaterBlockAt(pPed->get_cam_x().ToInt(), pPed->get_cam_y().ToInt(), pPed->get_cam_z().ToInt() - 1);
}

void Rect(f32 left, f32 top, f32 right, f32 bottom, s32 colour)
{
    Vert verts[4] = {};
    verts[0].x = verts[3].x = left;
    verts[1].x = verts[2].x = right;
    verts[0].y = verts[1].y = top;
    verts[2].y = verts[3].y = bottom;
    pgbh_DrawFlatRect(verts, colour);
}

// Font pixels (layout pixels) to whole screen pixels, as the HUD font's own texels land.
struct FontPixels
{
    f32 mX0;
    f32 mY0;
    f32 mScale;

    void Fill(s32 x, s32 y, s32 w, s32 h, s32 colour) const
    {
        Rect(floorf(mX0 + x * mScale + 0.5f), floorf(mY0 + y * mScale + 0.5f), floorf(mX0 + (x + w) * mScale + 0.5f),
             floorf(mY0 + (y + h) * mScale + 0.5f), colour);
    }
};

// A loading spinner in the HUD font's look (outline, shadow, lit top left): 8 dots on a ring 15 font pixels high,
// the brightest going round once a second at the logic steps' pace, the others darker behind it.
void DrawSpinner(s32 left, s32 top)
{
    const s32 kDots = 8;
    // A dot's top left pixel, clockwise from the top.
    const s8 kPos[kDots][2] = {{6, 1}, {10, 2}, {11, 6}, {10, 10}, {6, 11}, {2, 10}, {1, 6}, {2, 2}};
    // By age, the head first: the font's highlight, bevel and body colours, then darker toward its outline.
    const s32 kFill[kDots] = {0xCECA73, 0xAD9A52, 0xAD9A52, 0x8C7531, 0x8C7531, 0x7A6528, 0x67561F, 0x554616};
    const s32 kLight[kDots] = {0xCECA73, 0xCECA73, 0xCECA73, 0xAD9A52, 0xAD9A52, 0x8C7531, 0x7A6528, 0x67561F};
    const s32 kOutline = 0x313500;
    const s32 kShadow = 0x000000;

    const FontPixels pixels = {PortUiX(Fix16(left)).ToFloat(), PortUiY(Fix16(top)).ToFloat(), gViewCamera->mUiScale.ToFloat()};
    const f32 turns = (PortFrameLoop::StepCount() % 30 + PortFrameLoop::DrawAlpha()) / 30.0f;
    const s32 head = static_cast<s32>(turns * kDots) % kDots;
    for (s32 layer = 0; layer < 3; layer++)
    {
        for (s32 i = 0; i < kDots; i++)
        {
            const s32 x = kPos[i][0];
            const s32 y = kPos[i][1];
            const s32 age = (head - i + kDots) % kDots;
            if (layer == 0)
            {
                pixels.Fill(x, y, 4, 4, kShadow);
            }
            else if (layer == 1)
            {
                pixels.Fill(x - 1, y - 1, 4, 4, kOutline);
            }
            else
            {
                pixels.Fill(x, y, 2, 2, kFill[age]);
                pixels.Fill(x, y, 1, 1, kLight[age]);
            }
        }
    }
}

void Save(const char* pWhy)
{
#if defined(GTA2_PORT_SDL)
    SDL_Log("Autosave: %s", pWhy);
#else
    (void)pWhy;
#endif
    PortSaves::Autosave();
}
} // namespace

void Start()
{
    sPending = false;
    sShown = 0;
    sLastMission = -1;
    sLastFrenzy = false;
    sJobEnded = false;
    sPlayed = 0;
}

void AfterStep()
{
    if (sShown > 0)
    {
        sShown--;
    }
    if (!gGame_0x40 || !gScriptManager || PortSaves::Replaying())
    {
        return;
    }
    const s32* pMission = gScriptManager->mMissionFlag;
    const s32 mission = pMission ? (*pMission != 0) : -1;
    const bool frenzy = gScriptManager->IsKillFrenzyActive();
    if ((sLastMission == 1 && mission == 0) || (sLastFrenzy && !frenzy))
    {
        sPending = true;
        sPendingWhy = sLastFrenzy && !frenzy ? "frenzy over" : "job over";
        sJobEnded = true;
        sWaited = 0;
        sSafe = 0;
    }
    sLastMission = mission;
    sLastFrenzy = frenzy;
    sPlayed += gGame_0x40->IsPaused() ? 0 : 1;
    if (!sPending || mission != 0 || gGame_0x40->IsPaused())
    {
        sPending = sPending && mission == 0;
        return;
    }
    sWaited++;
    sSafe = Safe(false) ? sSafe + 1 : 0;
    if (sWaited >= kWaitSteps && sSafe >= kSafeSteps)
    {
        sPending = false;
        Save(sPendingWhy);
        sShown = kShowSteps;
    }
}

void Quit()
{
    if (PortSaves::Replaying())
    {
        return;
    }
    if (!sJobEnded && sPlayed < kQuitSaveSteps)
    {
#if defined(GTA2_PORT_SDL)
        SDL_Log("Autosave: none on quitting, no job or frenzy over and %d seconds played", sPlayed / 30);
#endif
    }
    else if (Safe(true))
    {
        Save("quit");
    }
}

void Draw()
{
    if (!sShown)
    {
        return;
    }
    // Under the zone name and the wanted level, above a job's titles; the spinner level with the letters.
    const s32 kTop = 58;
    const s32 kSpinnerTop = 5;
    const s32 kSpinner = 15;
    const s32 kGap = 4;
    const wchar_t* pText = PortSettings::Translate(L"SAVING");
    const s32 left = 320 - (Frontend::GetMaxTextWidth(const_cast<wchar_t*>(pText), gDebugFont) + kSpinner + kGap) / 2;
    DrawSpinner(left, kTop + kSpinnerTop);
    DrawStringScaled(pText, Fix16(left + kSpinner + kGap), Fix16(kTop), gDebugFont);
}
} // namespace PortAutosave
