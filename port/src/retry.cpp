#include "retry.hpp"
#include "Game.hpp"
#include "GameSession.hpp"
#include "Globals.hpp"
#include "Ped.hpp"
#include "Player.hpp"
#include "ScriptManager.hpp"
#include "settings.hpp"
#include "winmain.hpp"

#if defined(GTA2_PORT_SDL)
#include <SDL3/SDL_log.h>
#endif

namespace PortRetry
{
namespace
{
// The job or frenzy the checkpoint was taken before, while it runs, and its passed counter then.
PortSaves::Checkpoint sRunning;
s32 sPassedBefore;
s32 sLastMission;
bool sLastFrenzy;
bool sRetrying;

void Log(const char* pWhat)
{
#if defined(GTA2_PORT_SDL)
    SDL_Log("Retry: %s", pWhat);
#else
    (void)pWhat;
#endif
}

bool Enabled()
{
    return PortSettings::Retry() && gGame_0x40 && gScriptManager && !IsNetworkGame() && !gGameSession.IsBonusStage();
}

s32 PassedCount(PortSaves::Checkpoint kind)
{
    const s32* pCounter = kind == PortSaves::kBeforeJob ? gScriptManager->mPassedFlag : gScriptManager->mSecretsPassed;
    return pCounter ? *pCounter : 0;
}

void End(bool passed)
{
    sRunning = PortSaves::kNoCheckpoint;
    if (passed)
    {
        Log("passed");
        PortSaves::DeleteCheckpoint();
    }
    else
    {
        Log("failed");
    }
}
} // namespace

void Start()
{
    sRunning = PortSaves::kNoCheckpoint;
    sLastMission = -1;
    sLastFrenzy = false;
    sRetrying = false;
    PortSaves::RefreshCheckpoint();
    if (gScriptManager && !gScriptManager->IsLoadedFromSave() && !IsNetworkGame() && !gGameSession.IsBonusStage())
    {
        PortSaves::DeleteCheckpoint();
    }
}

void BeforeStart(PortSaves::Checkpoint kind)
{
    if (!Enabled() || !gScriptManager->mMissionFlag || *gScriptManager->mMissionFlag || gScriptManager->IsKillFrenzyActive())
    {
        return;
    }
    Player* pPlayer = gGame_0x40->mLocalPlayer;
    Ped* pPed = pPlayer ? pPlayer->mPlayerPed : NULL;
    if (!pPed || pPlayer->mWastedOrBusted || pPed->get_health() <= 0)
    {
        return;
    }
    Log(kind == PortSaves::kBeforeJob ? "checkpoint before a job" : "checkpoint before a frenzy");
    PortSaves::WriteCheckpoint(kind);
    sRunning = kind;
    sPassedBefore = PassedCount(kind);
}

void AfterStep()
{
    if (!gGame_0x40 || !gScriptManager)
    {
        return;
    }
    const s32* pMission = gScriptManager->mMissionFlag;
    const s32 mission = pMission ? (*pMission != 0) : -1;
    const bool frenzy = gScriptManager->IsKillFrenzyActive();
    if ((sRunning == PortSaves::kBeforeJob && sLastMission == 1 && mission == 0) ||
        (sRunning == PortSaves::kBeforeFrenzy && sLastFrenzy && !frenzy))
    {
        End(PassedCount(sRunning) > sPassedBefore);
    }
    sLastMission = mission;
    sLastFrenzy = frenzy;
    if (gGame_0x40->IsExitTimerSet() && gGame_0x40->GetExitType() == GameExitType::AreaCompleted_4 &&
        PortSaves::GetCheckpoint() != PortSaves::kNoCheckpoint)
    {
        Log("area complete");
        PortSaves::DeleteCheckpoint();
    }
}

void Retry()
{
    Log("chosen");
    sRetrying = true;
    gGame_0x40->ExitGameNoBonus(1, GameExitType::PlayerQuit_2);
}

bool TakeRetry()
{
    const bool retrying = sRetrying;
    sRetrying = false;
    return retrying;
}
} // namespace PortRetry
