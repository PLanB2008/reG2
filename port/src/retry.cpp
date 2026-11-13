#include "retry.hpp"
#include "Draw.hpp"
#include "Frontend.hpp"
#include "Game.hpp"
#include "Gang.hpp"
#include "GameSession.hpp"
#include "Globals.hpp"
#include "Hud.hpp"
#include "Ped.hpp"
#include "Player.hpp"
#include "ScriptManager.hpp"
#include "ScriptThread.hpp"
#include "gtx_0x106C.hpp"
#include "map_0x370.hpp"
#include "menu.hpp"
#include "pause_menu.hpp"
#include "settings.hpp"
#include "winmain.hpp"
#include <math.h>

#if defined(GTA2_PORT_SDL)
#include <SDL3/SDL_log.h>
#endif

namespace PortRetry
{
namespace
{
// At 30 steps a second, not counting the time the player is wasted or busted.
const s32 kShowSteps = 240;

// The job or frenzy the checkpoint was taken before, while it runs, and its passed counter then.
PortSaves::Checkpoint sRunning;
s32 sPassedBefore;
s32 sLastMission;
bool sLastFrenzy;
s32 sShown;
bool sRetrying;
// A checkpoint was just written before a job, whose script is not loaded yet.
bool sJobToName;
// LoadMissionScript loads that many base pointers of a job's script.
const u32 kJobBasePointers = 0xC00 / sizeof(u16);

// The player's last steady spots, one a second over the last 12 seconds or so. A checkpoint puts him back
// at one a few steps from what started the job or frenzy, so that it starts again only if he goes back to it.
struct Sample
{
    PortSaves::Spot mSpot;
    u32 mStep;
};
const s32 kNumSamples = 12;
const u32 kSampleSteps = 30;
const f32 kMinTriggerDistance = 2;
// Further in a second: a teleport, the spots before it are of no use.
const f32 kMaxSampleDistance = 20;
// Cars catch fire from there (Car::AccumulateDamage).
const s16 kBurningDamage = 16000;
Sample sSamples[kNumSamples];
s32 sNumSamples;
u32 sSteps;

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
    else if (PortSaves::GetCheckpoint() != PortSaves::kNoCheckpoint)
    {
        Log("failed");
        sShown = kShowSteps;
    }
}

f32 Distance(const PortSaves::Spot& a, const PortSaves::Spot& b)
{
    const f32 dx = a.mX.ToFloat() - b.mX.ToFloat();
    const f32 dy = a.mY.ToFloat() - b.mY.ToFloat();
    return sqrtf(dx * dx + dy * dy);
}

// On the top of the map's ground there, not water; a road for a car.
bool OnGround(const PortSaves::Spot& spot)
{
    s32 z;
    const gmp_block_info* pBlock = gMap_0x370->FindHighestBlockForCoord(spot.mX.ToInt(), spot.mY.ToInt(), &z);
    if (!pBlock || IS_AIR_TYPE(pBlock->mSlopeType) || (spot.mDriving && GET_BLOCK_TYPE(pBlock->mSlopeType) != ROAD) ||
        gGtx_0x106C->GetSpecIgnoringRoadJunction(pBlock->mLid & 0x3FF) == tile_spec::water)
    {
        return false;
    }
    return fabsf(gMap_0x370->FindGroundZForCoord(spot.mX, spot.mY).ToFloat() - spot.mZ.ToFloat()) < 0.125f;
}

// Alive, not burning, walking or standing on the ground, or driving a car that is not burning on a road.
bool Steady(const PortSaves::Spot& spot)
{
    Player* pPlayer = gGame_0x40->mLocalPlayer;
    Ped* pPed = pPlayer ? pPlayer->mPlayerPed : NULL;
    if (!pPed || pPlayer->mWastedOrBusted || pPed->get_health() <= 0 || pPed->IsOnFire() || !OnGround(spot))
    {
        return false;
    }
    if (spot.mDriving)
    {
        return spot.mCarDamage < kBurningDamage;
    }
    const s32 state1 = pPed->mPedState1;
    const s32 state2 = pPed->mPedState2;
    return !pPed->mCar && (state1 == ped_state_1::walking_0 || state1 == ped_state_1::standing_still_7) &&
        (state2 == ped_state_2::ped2_walking_0 || state2 == ped_state_2::ped2_staying_14);
}

void TakeSample()
{
    Player* pPlayer = gGame_0x40->mLocalPlayer;
    if (!pPlayer || pPlayer->mWastedOrBusted)
    {
        sNumSamples = 0;
        return;
    }
    PortSaves::Spot spot;
    PortSaves::GetSpot(&spot);
    if (!Steady(spot))
    {
        return;
    }
    if (sNumSamples && Distance(sSamples[(sNumSamples - 1) % kNumSamples].mSpot, spot) > kMaxSampleDistance)
    {
        sNumSamples = 0;
    }
    sSamples[sNumSamples % kNumSamples] = {spot, sSteps};
    sNumSamples++;
}

// The latest steady spot of the last seconds far enough from where the player is, else where he is.
PortSaves::Spot RetrySpot()
{
    PortSaves::Spot here;
    PortSaves::GetSpot(&here);
    for (s32 i = sNumSamples - 1; i >= 0 && i >= sNumSamples - kNumSamples; i--)
    {
        const Sample& sample = sSamples[i % kNumSamples];
        if (sSteps - sample.mStep <= kNumSamples * kSampleSteps && Distance(sample.mSpot, here) >= kMinTriggerDistance)
        {
            Log("put back a few steps away");
            return sample.mSpot;
        }
    }
    return here;
}
} // namespace

void Start()
{
    sRunning = PortSaves::kNoCheckpoint;
    sLastMission = -1;
    sLastFrenzy = false;
    sShown = 0;
    sRetrying = false;
    sNumSamples = 0;
    PortSaves::RefreshCheckpoint();
    if (gScriptManager && (!gScriptManager->IsLoadedFromSave() || PortSaves::LoadedCheckpoint() == PortSaves::kNoCheckpoint) &&
        !IsNetworkGame() && !gGameSession.IsBonusStage())
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
    PortSaves::WriteCheckpoint(kind, RetrySpot());
    sJobToName = kind == PortSaves::kBeforeJob;
    sRunning = kind;
    sPassedBefore = PassedCount(kind);
    sShown = 0;
}

void JobLoaded(const SCR_CMD_HEADER* pCmd, u16 base)
{
    if (!sJobToName)
    {
        return;
    }
    sJobToName = false;
    PortSaves::JobName job = {};
    // Jobs from a phone: the gang the phone's command names. The others (tutorial, final jobs) have none.
    if (pCmd->mType == SCRCMD_PHONE_TEMPLATE || pCmd->mType == SCRCMD_DO_EASY_PHONE)
    {
        const SCR_PHONE_TEMPLATE* pPhone = static_cast<const SCR_PHONE_TEMPLATE*>(pCmd);
        str_table_entry* pName = gScriptManager->FindStringById(pPhone->mGangname);
        const Gang* pGang = pName ? gGangPool_CA8->FindGangByName(pName->get_name()) : NULL;
        job.mGang = pGang ? pGang->mArrowColour : 0;
    }
    // The title: in the game's scripts, the job's first DISPLAY_MESSAGE of type 0 (1 and 2 are JOB COMPLETE and
    // JOB FAILED).
    const u32 end = base + kJobBasePointers;
    for (u32 i = base; i < end && i < GTA2_COUNTOF(gScriptManager->mBasePointers) && !job.mTitle; i++)
    {
        const SCR_TWO_PARAMS* pMessage = static_cast<const SCR_TWO_PARAMS*>(gScriptManager->GetBasePointer(static_cast<u16>(i)));
        if (pMessage && pMessage->mType == SCRCMD_DISPLAY_MESSAGE && pMessage->mUnsigned2 == 0)
        {
            job.mTitle = pMessage->mUnsigned1;
        }
    }
#if defined(GTA2_PORT_SDL)
    SDL_Log("Retry: job title %u, gang %u", job.mTitle, job.mGang);
#endif
    PortSaves::SetCheckpointJob(job);
}

void AfterStep()
{
    if (!gGame_0x40 || !gScriptManager)
    {
        return;
    }
    Player* pPlayer = gGame_0x40->mLocalPlayer;
    if (sShown > 0 && pPlayer && !pPlayer->mWastedOrBusted)
    {
        sShown--;
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
    if (Enabled() && ++sSteps % kSampleSteps == 0)
    {
        TakeSample();
    }
    if (gGame_0x40->IsExitTimerSet() && gGame_0x40->GetExitType() == GameExitType::AreaCompleted_4 &&
        PortSaves::GetCheckpoint() != PortSaves::kNoCheckpoint)
    {
        Log("area complete");
        PortSaves::DeleteCheckpoint();
    }
}

void Draw()
{
    if (!sShown || !PortSettings::Retry() || PortPauseMenu::IsOpen() || PortSaves::GetCheckpoint() == PortSaves::kNoCheckpoint)
    {
        return;
    }
    // Under SAVING, above a job's titles.
    const s32 kTop = 78;
    wchar_t text[64];
    swprintf(text, PortSettings::Translate(L"%ls: RETRY"), PortMenu::PadMode() ? L"START" : PortSettings::Translate(L"ESC"));
    const s32 left = 320 - Frontend::GetMaxTextWidth(text, gDebugFont) / 2;
    DrawStringScaled(text, Fix16(left), Fix16(kTop), gDebugFont);
}

bool JustFailed()
{
    return sShown > 0;
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
