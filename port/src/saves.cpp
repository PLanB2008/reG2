#include "saves.hpp"
#include "Car.hpp"
#include "Char_Pool.hpp"
#include "Game.hpp"
#include "GameSession.hpp"
#include "Globals.hpp"
#include "Ped.hpp"
#include "Player.hpp"
#include "ScriptManager.hpp"
#include "char.hpp"
#include "map_0x370.hpp"
#include "sprite.hpp"
#include <stdio.h>
#include <string.h>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#else
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#if defined(GTA2_PORT_SDL)
#include <SDL3/SDL_log.h>
#endif

namespace PortSaves
{
namespace
{
// The room Map_0x370::load_dmap leaves after the map's own columns and blocks for the changes made in play.
const u32 kMaxColumnBytes = 1024 * sizeof(u32);
const u32 kMaxBlockBytes = 200 * sizeof(gmp_block_info);
const u32 kMaxDmapInfoBytes = sizeof(((DmapInfoList*)0)->mDmapInfos);

const size_t kBasePointerCount = sizeof(((ScriptManager*)0)->mBasePointers) / sizeof(u16);
const size_t kScriptDataSize = sizeof(((ScriptManager*)0)->mScriptData);

void Reject(const char_type* pFileName, const char* pWhy)
{
#if defined(GTA2_PORT_SDL)
    SDL_Log("Save %s ignored: %s", pFileName, pWhy);
#else
    (void)pFileName;
    (void)pWhy;
#endif
}

// A name of the header, 24 characters at most.
bool NameOk(const char_type* pName)
{
    return memchr(pName, 0, 24) != NULL && pName[0] != 0;
}

// The level script as ScriptManager::Load reads it: the base pointer table, then the commands.
struct Script
{
    std::vector<u16> mBasePointers;
    std::vector<u8> mData;

    const SCR_TWO_PARAMS* Command(u16 idx) const
    {
        if (idx >= mBasePointers.size() || !mBasePointers[idx] || mBasePointers[idx] + sizeof(SCR_TWO_PARAMS) > mData.size())
        {
            return NULL;
        }
        return reinterpret_cast<const SCR_TWO_PARAMS*>(&mData[mBasePointers[idx]]);
    }
};

bool ReadScript(const char_type* pScriptName, Script* pScript)
{
    FILE* pFile = fopen(pScriptName, "rb");
    if (!pFile)
    {
        return false;
    }
    pScript->mBasePointers.resize(kBasePointerCount);
    pScript->mData.resize(kScriptDataSize);
    const bool ok = fread(&pScript->mBasePointers[0], sizeof(u16), kBasePointerCount, pFile) == kBasePointerCount &&
        fread(&pScript->mData[0], 1, kScriptDataSize, pFile) == kScriptDataSize;
    fclose(pFile);
    return ok;
}

bool IsSavedCounter(const SCR_CMD_HEADER* pCmd)
{
    return pCmd && (pCmd->mType == SCRCMD_COUNTER_SAVE || pCmd->mType == SCRCMD_COUNTER_SET_SAVE);
}

// A counter's value in the save, else the one the script starts it with.
s32 CounterValue(const SaveData& save, const Script& script, u16 idx)
{
    const SavedCarAndScriptData& data = save.mCarAndScriptData;
    for (s32 i = 0; i < GTA2_COUNTOF_S(data.mScriptCounter); i++)
    {
        if (data.mScriptCounter[i].mPointer == idx)
        {
            return data.mScriptCounter[i].mSavedValue;
        }
    }
    const SCR_TWO_PARAMS* pCounter = script.Command(idx);
    return pCounter ? pCounter->mS32 : 0;
}

// The main stages in the frontend's order.
const char* const kCityFiles[kNumCities] = {"downtown", "residential", "industrial"};

void LatestChurchName(u8 slot, char_type* pOut, size_t size)
{
    snprintf(pOut, size, "player\\plyslot%d.svg", slot);
}

// pKind: church or auto.
void FileName(u8 slot, const char* pKind, u8 mainStage, char_type* pOut, size_t size)
{
    snprintf(pOut, size, "player\\plyslot%d_%s_%s.svg", slot, pKind, kCityFiles[mainStage < kNumCities ? mainStage : 0]);
}

bool CopySave(const char_type* pFrom, const char_type* pTo)
{
    FILE* pIn = fopen(pFrom, "rb");
    if (!pIn)
    {
        return false;
    }
    std::vector<u8> data;
    u8 buffer[4096];
    for (size_t n; (n = fread(buffer, 1, sizeof(buffer), pIn)) > 0;)
    {
        data.insert(data.end(), buffer, buffer + n);
    }
    fclose(pIn);
    char_type temp[260];
    TempName(pTo, temp, sizeof(temp));
    FILE* pOut = fopen(temp, "wb");
    if (!pOut)
    {
        return false;
    }
    const bool written = fwrite(data.data(), 1, data.size(), pOut) == data.size();
    return fclose(pOut) == 0 && written && Commit(temp, pTo);
}

bool Move(const char_type* pFrom, const char_type* pTo)
{
#if defined(_WIN32)
    return MoveFileExA(pFrom, pTo, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    char from[PATH_MAX];
    char to[PATH_MAX];
    port_resolve_path(pFrom, from);
    port_resolve_path(pTo, to);
    const int fd = open(from, O_RDONLY);
    if (fd >= 0)
    {
        fsync(fd);
        close(fd);
    }
    return rename(from, to) == 0;
#endif
}

// An autosave's or a checkpoint's mUnused50, which the original leaves 0: the player spawns where he was.
// Its mPad4E is 1 if he drove a car, a checkpoint's mPad4F what it was taken before.
const s32 kExactSpawn = 0x32474552;

enum Writing
{
    kWritingChurchSave,
    kWritingAutosave,
    kWritingCheckpoint,
};
Writing sWriting;
Checkpoint sWritingCheckpoint;
Spot sWritingSpot;

// The checkpoint as its file was last written, deleted or read.
struct CheckpointInfo
{
    Checkpoint mKind;
    u8 mMainStage;
    s32 mMoney;
    long long mTime;
};
CheckpointInfo sCheckpoint;

void RemoveFile(const char_type* pFileName)
{
#if defined(_WIN32)
    DeleteFileA(pFileName);
#else
    char path[PATH_MAX];
    unlink(port_resolve_path(pFileName, path));
#endif
}

void BackupName(const char_type* pFileName, char_type* pOut, size_t size)
{
    snprintf(pOut, size, "%s.bak", pFileName);
}

// Not a train: RestoreCarsFromSave spawns single cars.
Car* DrivenCar(Player* pPlayer)
{
    Ped* pPed = pPlayer->mPlayerPed;
    Car* pCar = pPed ? pPed->mCar : NULL;
    if (!pCar || pPed->mPedState1 != ped_state_1::in_car_10 || pCar->mDriver != pPed || pCar->IsTrainModel())
    {
        return NULL;
    }
    return pCar;
}

// As Ped::UpdateCharB4 ends a walk into a car.
void PutInCar(Ped* pPed, Car* pCar)
{
    if (Char* pChar = pPed->mGameObject)
    {
        if (pChar->mObj2c.mP18)
        {
            pChar->mObj2c.DestroyAllSprites();
        }
        gChar_B4_Pool->DeAllocate(pChar);
        pPed->mGameObject = NULL;
    }
    pPed->mCar = pCar;
    pPed->mEnterCarAsPassenger = 0;
    pPed->mTargetCarDoor = 0;
    pPed->mPedState1 = ped_state_1::in_car_10;
    pPed->mPedState2 = ped_state_2::ped2_driving_10;
    pCar->AssignDriver(pPed);
}

long long FileTime(const char_type* pFileName)
{
#if defined(_WIN32)
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesExA(pFileName, GetFileExInfoStandard, &data))
    {
        return -1;
    }
    const unsigned long long ticks =
        (static_cast<unsigned long long>(data.ftLastWriteTime.dwHighDateTime) << 32) | data.ftLastWriteTime.dwLowDateTime;
    return static_cast<long long>(ticks / 10000000ull) - 11644473600ll;
#else
    char path[PATH_MAX];
    struct stat st;
    return stat(port_resolve_path(pFileName, path), &st) == 0 ? static_cast<long long>(st.st_mtime) : -1;
#endif
}

bool ReadEntry(const char_type* pFileName, Kind kind, LevelCheck pCheck, Entry* pEntry)
{
    Header header;
    if (!IsValid(pFileName, &header) || !pCheck(header))
    {
        return false;
    }
    snprintf(pEntry->mFileName, sizeof(pEntry->mFileName), "%s", pFileName);
    pEntry->mKind = kind;
    pEntry->mCheckpoint = static_cast<Checkpoint>(header.mCheckpoint);
    pEntry->mMainStage = header.mMainStage;
    pEntry->mBonusStage = header.mBonusStage;
    pEntry->mMoney = header.mMoney;
    pEntry->mTime = FileTime(pFileName);
    JobsPassed jobs;
    const bool read = ReadJobsPassed(pFileName, &jobs);
    pEntry->mJobsPassed = read ? jobs.mPassed : -1;
    pEntry->mJobsTotal = read ? jobs.mTotal : -1;
    return true;
}
} // namespace

void CheckpointName(u8 slot, char_type* pOut, size_t size)
{
    snprintf(pOut, size, "player\\plyslot%d_retry.svg", slot);
}

void TempName(const char_type* pFileName, char_type* pOut, size_t size)
{
    snprintf(pOut, size, "%s.tmp", pFileName);
}

bool Commit(const char_type* pTempName, const char_type* pFileName)
{
    if (sWriting == kWritingAutosave)
    {
        char_type backup[260];
        BackupName(pFileName, backup, sizeof(backup));
        Move(pFileName, backup);
    }
    const bool ok = Move(pTempName, pFileName);
    if (!ok)
    {
        Reject(pFileName, "the new save could not replace it");
    }
    return ok;
}

bool IsValid(const char_type* pFileName, Header* pHeader)
{
    FILE* pFile = fopen(pFileName, "rb");
    if (!pFile)
    {
        return false;
    }
    SaveData save;
    bool complete = fread(&save, sizeof(save), 1, pFile) == 1;
    const u32 limits[3] = {kMaxColumnBytes, kMaxBlockBytes, kMaxDmapInfoBytes};
    const u32 units[3] = {sizeof(u32), sizeof(gmp_block_info), sizeof(gmp_dmap_info)};
    for (s32 i = 0; i < 3 && complete; i++)
    {
        u32 len;
        complete = fread(&len, sizeof(len), 1, pFile) == 1 && len <= limits[i] && len % units[i] == 0 &&
            fseek(pFile, static_cast<long>(len), SEEK_CUR) == 0;
    }
    if (complete)
    {
        const long end = ftell(pFile);
        complete = fseek(pFile, 0, SEEK_END) == 0 && ftell(pFile) == end;
    }
    fclose(pFile);
    if (!complete)
    {
        Reject(pFileName, "incomplete");
        return false;
    }

    u8 main_stage = static_cast<u8>(save.mMainStage);
    u8 bonus_stage = 0;
    if (save.mBonusStage)
    {
        main_stage = static_cast<u8>(save.mLhv) >> 4;
        bonus_stage = static_cast<u8>(save.mLhv) & 0xF;
    }
    if (!NameOk(save.mMapName) || !NameOk(save.mStyleName) || !NameOk(save.mScriptName) || main_stage >= 3 ||
        bonus_stage >= 4)
    {
        Reject(pFileName, "bad header");
        return false;
    }

    Script script;
    if (!ReadScript(save.mScriptName, &script))
    {
        Reject(pFileName, "its script cannot be read");
        return false;
    }
    for (s32 i = 0; i < GTA2_COUNTOF_S(save.mCarAndScriptData.mScriptCounter); i++)
    {
        const u16 idx = save.mCarAndScriptData.mScriptCounter[i].mPointer;
        if (idx && !IsSavedCounter(script.Command(idx)))
        {
            Reject(pFileName, "its counters are not those of its script");
            return false;
        }
    }

    if (pHeader)
    {
        memcpy(pHeader->mMapName, save.mMapName, sizeof(pHeader->mMapName));
        memcpy(pHeader->mStyleName, save.mStyleName, sizeof(pHeader->mStyleName));
        memcpy(pHeader->mScriptName, save.mScriptName, sizeof(pHeader->mScriptName));
        pHeader->mMainStage = main_stage;
        pHeader->mBonusStage = bonus_stage;
        pHeader->mMoney = save.mPlayerAndWorldStats.mMoney;
        pHeader->mCheckpoint = save.mUnused50 == kExactSpawn && save.mPad4F <= kBeforeFrenzy ? save.mPad4F : kNoCheckpoint;
    }
    return true;
}

bool ReadJobsPassed(const char_type* pFileName, JobsPassed* pJobs)
{
    FILE* pFile = fopen(pFileName, "rb");
    if (!pFile)
    {
        return false;
    }
    SaveData save;
    const bool read = fread(&save, sizeof(save), 1, pFile) == 1;
    fclose(pFile);
    Script script;
    if (!read || !NameOk(save.mScriptName) || !ReadScript(save.mScriptName, &script))
    {
        return false;
    }

    // As ScriptThread::SCRCMD_SET_MISSIONS_TOTAL and SCRCMD_PASSED_FAILED_FLAGS set them for the pause screen.
    memset(pJobs, 0, sizeof(*pJobs));
    bool counted = false;
    for (u16 idx = 0; idx < script.mBasePointers.size(); idx++)
    {
        const SCR_TWO_PARAMS* pCmd = script.Command(idx);
        if (!pCmd)
        {
            continue;
        }
        switch (pCmd->mType)
        {
            case SCRCMD_TOTAL_MISSIONS:
                pJobs->mTotal = pCmd->mSigned2;
                counted = true;
                break;
            case SCRCMD_GANG_1_MISSION_TOTAL:
            case SCRCMD_GANG_2_MISSION_TOTAL:
            case SCRCMD_GANG_3_MISSION_TOTAL:
                pJobs->mGangTotal[pCmd->mType - SCRCMD_GANG_1_MISSION_TOTAL] = pCmd->mSigned2;
                break;
            case SCRCMD_TOTAL_SECRETS:
                pJobs->mFrenziesTotal = pCmd->mSigned2;
                break;
            case SCRCMD_PASSED_FLAG:
                pJobs->mPassed = CounterValue(save, script, pCmd->mUnsigned1);
                break;
            case SCRCMD_1_PASSED_FLAG:
            case SCRCMD_2_PASSED_FLAG:
            case SCRCMD_3_PASSED_FLAG:
                pJobs->mGangPassed[pCmd->mType - SCRCMD_1_PASSED_FLAG] = CounterValue(save, script, pCmd->mUnsigned1);
                break;
            case SCRCMD_SECRETS_PASSED:
                pJobs->mFrenziesPassed = CounterValue(save, script, pCmd->mUnsigned1);
                break;
            default:
                break;
        }
    }
    return counted;
}

void AfterSave(const char_type* pFileName)
{
    char_type latest[260];
    LatestChurchName(gGameSession.GetPlySlotIdx(), latest, sizeof(latest));
    if (sWriting != kWritingChurchSave || strcmp(pFileName, latest))
    {
        return;
    }
    DeleteCheckpoint();
    if (gGameSession.IsBonusStage())
    {
        return;
    }
    char_type copy[260];
    FileName(gGameSession.GetPlySlotIdx(), "church", gGameSession.GetMainStageIdx(), copy, sizeof(copy));
    CopySave(pFileName, copy);
}

void FillSave(SaveData* pSave)
{
    SavedPlayerStats& stats = pSave->mPlayerAndWorldStats;
    SavedCarAndScriptData& data = pSave->mCarAndScriptData;
    SavedCarInfo& cars = data.mSavedCars;
    memset(stats.mPowerUps, 0, sizeof(stats.mPowerUps));
    memset(&cars, 0, sizeof(cars));
    data.mUnused44 = 0;
    pSave->mPad4E = 0;
    pSave->mPad4F = 0;
    pSave->mUnused50 = 0;
    if (sWriting == kWritingChurchSave)
    {
        return;
    }
    pSave->mPad4F = static_cast<char_type>(sWriting == kWritingCheckpoint ? sWritingCheckpoint : kNoCheckpoint);
    Player* pPlayer = gGame_0x40->mLocalPlayer;
    pSave->mUnused50 = kExactSpawn;
    for (s32 i = 0; i < GTA2_COUNTOF_S(stats.mPowerUps); i++)
    {
        stats.mPowerUps[i] = pPlayer->mPowerUpTimers[i];
    }
    Spot spot;
    if (sWriting == kWritingCheckpoint)
    {
        spot = sWritingSpot;
        stats.mX = spot.mX;
        stats.mY = spot.mY;
        stats.mZ = spot.mZ;
        stats.mRotation = spot.mRotation;
    }
    else
    {
        GetSpot(&spot);
    }
    if (spot.mDriving)
    {
        cars.mX[0].mValue = spot.mX;
        cars.mY[0].mValue = spot.mY;
        cars.mZ[0].mValue = spot.mZ;
        cars.mAng[0] = spot.mRotation;
        cars.mDamage[0] = spot.mCarDamage;
        cars.mDamagedAreas[0] = spot.mCarDamagedAreas;
        cars.mCarModel[0] = spot.mCarModel;
        // RestoreCarsFromSave clears the cars' fields before AfterRestore.
        data.mUnused44 = spot.mCarColour;
        pSave->mPad4E = 1;
    }
}

void GetSpot(Spot* pSpot)
{
    Player* pPlayer = gGame_0x40->mLocalPlayer;
    Ped* pPed = pPlayer->mPlayerPed;
    *pSpot = Spot();
    if (Car* pCar = DrivenCar(pPlayer))
    {
        Sprite* pSprite = pCar->mCarSprite;
        pSpot->mX = pSprite->GetXPos();
        pSpot->mY = pSprite->GetYPos();
        pSpot->mZ = pSprite->GetZPos();
        pSpot->mRotation = pSprite->mRotation;
        pSpot->mDriving = true;
        pSpot->mCarModel = static_cast<u16>(pCar->mCarInfoIdx);
        pSpot->mCarColour = static_cast<s16>(pSprite->mPaletteType == palette_types_enum::car_remaps_3 ? pSprite->mRemap + 1 : 0);
        pSpot->mCarDamage = pCar->mDamage;
        pSpot->mCarDamagedAreas = pCar->mDamagedAreas.m_var;
    }
    else if (pPed)
    {
        pSpot->mX = pPed->get_cam_x();
        pSpot->mY = pPed->get_cam_y();
        pSpot->mZ = pPed->get_cam_z();
        pSpot->mRotation = pPed->GetRotation();
    }
}

void Autosave()
{
    char_type name[260];
    FileName(gGameSession.GetPlySlotIdx(), "auto", gGameSession.GetMainStageIdx(), name, sizeof(name));
    sWriting = kWritingAutosave;
    gScriptManager->SaveGame(name);
    sWriting = kWritingChurchSave;
}

void WriteCheckpoint(Checkpoint kind, const Spot& spot)
{
    char_type name[260];
    CheckpointName(gGameSession.GetPlySlotIdx(), name, sizeof(name));
    sWriting = kWritingCheckpoint;
    sWritingCheckpoint = kind;
    sWritingSpot = spot;
    gScriptManager->SaveGame(name);
    sWriting = kWritingChurchSave;
    sCheckpoint.mTime = FileTime(name);
    sCheckpoint.mKind = sCheckpoint.mTime >= 0 ? kind : kNoCheckpoint;
    sCheckpoint.mMainStage = static_cast<u8>(gGameSession.GetMainStageIdx());
    sCheckpoint.mMoney = gGameSave.mPlayerAndWorldStats.mMoney;
}

void DeleteCheckpoint()
{
    char_type name[260];
    CheckpointName(gGameSession.GetPlySlotIdx(), name, sizeof(name));
    RemoveFile(name);
    sCheckpoint.mKind = kNoCheckpoint;
}

Checkpoint GetCheckpoint(s32* pMoney, long long* pTime)
{
    if (gGameSession.IsBonusStage() || sCheckpoint.mMainStage != static_cast<u8>(gGameSession.GetMainStageIdx()))
    {
        return kNoCheckpoint;
    }
    if (pMoney)
    {
        *pMoney = sCheckpoint.mMoney;
    }
    if (pTime)
    {
        *pTime = sCheckpoint.mTime;
    }
    return sCheckpoint.mKind;
}

void RefreshCheckpoint()
{
    char_type name[260];
    CheckpointName(gGameSession.GetPlySlotIdx(), name, sizeof(name));
    Header header = {};
    const bool valid = IsValid(name, &header) && !header.mBonusStage;
    sCheckpoint.mKind = valid ? static_cast<Checkpoint>(header.mCheckpoint) : kNoCheckpoint;
    sCheckpoint.mMainStage = header.mMainStage;
    sCheckpoint.mMoney = header.mMoney;
    sCheckpoint.mTime = FileTime(name);
}

bool ExactSpawn()
{
    return gGameSave.mUnused50 == kExactSpawn;
}

Checkpoint LoadedCheckpoint()
{
    return ExactSpawn() && gGameSave.mPad4F <= kBeforeFrenzy ? static_cast<Checkpoint>(gGameSave.mPad4F) : kNoCheckpoint;
}

void AfterRestore(Player* pPlayer)
{
    Ped* pPed = pPlayer->mPlayerPed;
    if (!ExactSpawn() || !gGameSave.mPad4E || !pPed || pPed->mCar)
    {
        return;
    }
    // The car restored last.
    Car* pCar = NULL;
    for (s32 i = 0; i < GTA2_COUNTOF_S(pPlayer->mCarHistory); i++)
    {
        pCar = pPlayer->mCarHistory[i] ? pPlayer->mCarHistory[i] : pCar;
    }
    if (!pCar)
    {
        return;
    }
    const s16 colour = gGameSave.mCarAndScriptData.mUnused44;
    if (colour)
    {
        pCar->SetCarRemap(static_cast<u8>(colour - 1));
    }
    else
    {
        pCar->mCarSprite->SetPaletteSprites();
    }
    PutInCar(pPed, pCar);
}

s32 List(u8 slot, LevelCheck pCheck, Entry* pEntries)
{
    char_type name[260];
    LatestChurchName(slot, name, sizeof(name));
    Entry latest;
    bool latestLeft = ReadEntry(name, kChurch, pCheck, &latest);
    s32 count = 0;
    for (u8 city = 0; city < kNumCities; city++)
    {
        // plyslotN.svg alone, as the original game writes it, is its city's church save unless the copy is newer.
        Entry& church = pEntries[count];
        FileName(slot, "church", city, name, sizeof(name));
        bool found = ReadEntry(name, kChurch, pCheck, &church) && church.mMainStage == city && !church.mBonusStage;
        if (latestLeft && latest.mMainStage == city && !latest.mBonusStage)
        {
            if (!found || latest.mTime > church.mTime)
            {
                church = latest;
            }
            found = true;
            latestLeft = false;
        }
        count += found ? 1 : 0;

        char_type backup[260];
        FileName(slot, "auto", city, name, sizeof(name));
        BackupName(name, backup, sizeof(backup));
        Entry& autosave = pEntries[count];
        if ((ReadEntry(name, kAutosave, pCheck, &autosave) || ReadEntry(backup, kAutosave, pCheck, &autosave)) &&
            autosave.mMainStage == city && !autosave.mBonusStage)
        {
            count++;
        }
    }
    if (latestLeft)
    {
        pEntries[count++] = latest;
    }
    CheckpointName(slot, name, sizeof(name));
    Entry& checkpoint = pEntries[count];
    if (ReadEntry(name, kRetry, pCheck, &checkpoint) && checkpoint.mCheckpoint != kNoCheckpoint && !checkpoint.mBonusStage)
    {
        count++;
    }
    for (s32 i = 1; i < count; i++)
    {
        for (s32 j = i; j > 0 && pEntries[j].mTime > pEntries[j - 1].mTime; j--)
        {
            const Entry swapped = pEntries[j];
            pEntries[j] = pEntries[j - 1];
            pEntries[j - 1] = swapped;
        }
    }
    return count;
}
} // namespace PortSaves
