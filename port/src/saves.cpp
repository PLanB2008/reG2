#include "saves.hpp"
#include "ScriptManager.hpp"
#include "map_0x370.hpp"
#include <stdio.h>
#include <string.h>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#else
#include <fcntl.h>
#include <limits.h>
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

    const SCR_CMD_HEADER* Command(u16 idx) const
    {
        if (idx >= mBasePointers.size() || !mBasePointers[idx] || mBasePointers[idx] + sizeof(SCR_TWO_PARAMS) > mData.size())
        {
            return NULL;
        }
        return reinterpret_cast<const SCR_CMD_HEADER*>(&mData[mBasePointers[idx]]);
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
} // namespace

void TempName(const char_type* pFileName, char_type* pOut, size_t size)
{
    snprintf(pOut, size, "%s.tmp", pFileName);
}

bool Commit(const char_type* pTempName, const char_type* pFileName)
{
#if defined(_WIN32)
    const bool ok = MoveFileExA(pTempName, pFileName, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    char temp[PATH_MAX];
    char dest[PATH_MAX];
    port_resolve_path(pTempName, temp);
    port_resolve_path(pFileName, dest);
    const int fd = open(temp, O_RDONLY);
    if (fd >= 0)
    {
        fsync(fd);
        close(fd);
    }
    const bool ok = rename(temp, dest) == 0;
#endif
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
    }
    return true;
}
} // namespace PortSaves
