#include "autosave.hpp"
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
const s32 kShowSteps = 60;

bool sPending;
s32 sWaited;
s32 sSafe;
s32 sShown;
s32 sLastMission;
bool sLastFrenzy;
const char* sPendingWhy;

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
}

void AfterStep()
{
    if (sShown > 0)
    {
        sShown--;
    }
    if (!gGame_0x40 || !gScriptManager)
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
        sWaited = 0;
        sSafe = 0;
    }
    sLastMission = mission;
    sLastFrenzy = frenzy;
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
    if (Safe(true))
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
    const wchar_t* pText = PortSettings::Translate(L"SAVING");
    const s32 width = Frontend::GetMaxTextWidth(const_cast<wchar_t*>(pText), gDebugFont);
    DrawStringScaled(pText, Fix16(632 - width), Fix16(456), gDebugFont);
}
} // namespace PortAutosave
