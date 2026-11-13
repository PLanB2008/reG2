// The save files (.svg): written whole or not at all, and checked before the game reads one.
#pragma once

#include "types.hpp"
#include <stddef.h>

class Player;
class SaveData;

namespace PortSaves
{
// The file a save is written to before Commit moves it over pFileName.
void TempName(const char_type* pFileName, char_type* pOut, size_t size);
bool Commit(const char_type* pTempName, const char_type* pFileName);

// What the game reads first from a save: its level's files and stage.
struct Header
{
    char_type mMapName[25];
    char_type mStyleName[25];
    char_type mScriptName[25];
    u8 mMainStage;
    u8 mBonusStage; // 0 for a main stage
    s32 mMoney;
};

// The file is a whole save (sizes, header, map changes) whose saved counters are those of its
// level's script.
bool IsValid(const char_type* pFileName, Header* pHeader = NULL);

// The jobs passed in a save, and the totals, as the pause screen counts them. The counters the
// level's script names are saved ones (COUNTER_SET_SAVE in the three cities' main scripts).
struct JobsPassed
{
    s32 mPassed;
    s32 mTotal;
    s32 mGangPassed[3]; // in the order of the level's gangs
    s32 mGangTotal[3];
    s32 mFrenziesPassed;
    s32 mFrenziesTotal;
};
// False if the save or its script cannot be read, or the script counts no jobs (bonus stages).
bool ReadJobsPassed(const char_type* pFileName, JobsPassed* pJobs);

// A profile's saves in player\, per city: the church save, as plyslotN.svg (the latest, as the original
// writes it) and a copy, and the autosave, with the one before it kept as a backup.
const s32 kNumCities = 3;

// After ScriptManager::SaveGame wrote a church save: its copy for the city.
void AfterSave(const char_type* pFileName);

// SaveGame, as it fills the header: the autosave's exact position, power-ups and car, or none of them in a
// church save, as the original.
void FillSave(SaveData* pSave);
// The autosave of the current city, written now.
void Autosave();
// The level was loaded from an autosave: the player spawns where he was.
bool ExactSpawn();
// Player::UpdateGameFromSave restored the cars: the player back in the one he drove.
void AfterRestore(Player* pPlayer);

// The saves RESUME SAVED STATUS lists, newest first.
enum Kind
{
    kChurch,
    kAutosave,
};
// With plyslotN.svg of a bonus stage.
const s32 kMaxSaves = kNumCities * 2 + 1;

struct Entry
{
    char_type mFileName[64];
    Kind mKind;
    u8 mMainStage;
    u8 mBonusStage;
    s32 mMoney;
    s32 mJobsPassed; // -1 if unknown
    s32 mJobsTotal;
    long long mTime; // the file's, in seconds since 1970
};

// A save the frontend can boot: its level's files are those of its stage.
typedef bool (*LevelCheck)(const Header& header);

s32 List(u8 slot, LevelCheck pCheck, Entry* pEntries);
} // namespace PortSaves
