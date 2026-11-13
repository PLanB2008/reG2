// The church save files (.svg): written whole or not at all, and checked before the game reads one.
#pragma once

#include "types.hpp"
#include <stddef.h>

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
} // namespace PortSaves
