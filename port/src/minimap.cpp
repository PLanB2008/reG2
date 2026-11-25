#include "minimap.hpp"
#include "Camera.hpp"
#include "Car.hpp"
#include "Cranes.hpp"
#include "Draw.hpp"
#include "Game.hpp"
#include "Gang.hpp"
#include "Hud.hpp"
#include "ObjectPool.hpp"
#include "Ped.hpp"
#include "Player.hpp"
#include "ScriptManager.hpp"
#include "ScriptThread.hpp"
#include "TextTable.hpp"
#include "block_face.hpp"
#include <dinput.h>
#include "enums.hpp"
#include "gtx_0x106C.hpp"
#include "map_0x370.hpp"
#include "settings.hpp"
#include "sprite.hpp"
#include "winmain.hpp"
#include <math.h>
#include <string.h>
#include <algorithm>
#include <vector>
#if defined(GTA2_PORT_SDL)
#include <SDL3/SDL.h>

void port_DrawMinimap(s32 slot, s32 width, s32 height, const u32* pRgba, u32 version, const f32* pXyuv, const u32* pColours, s32 count,
                      bool nearest);
void port_ClipMinimap(const f32* pRect);
bool port_DikHeld(DWORD dik);
bool port_TakeMouse(float* pX, float* pY, int* pLeft, int* pRight, int* pWheel);
bool port_MouseLeftDown();
bool port_TwinStickInput(float* pMoveX, float* pMoveY, float* pAimX, float* pAimY);
#endif

namespace PortMinimap
{
#if defined(GTA2_PORT_SDL)
namespace
{
// The markers and pins, original art from port/docs/minimap/assets (tools/make_all.py; the pins in the style of the key
// printed with the game's city maps from tools/key_pins.py), at 1x and 2x.
const unsigned char kArrow1x[] = {
#embed "../resources/minimap/player_arrow_1x.png"
};
const unsigned char kArrow2x[] = {
#embed "../resources/minimap/player_arrow_2x.png"
};
const unsigned char kBlip1x[] = {
#embed "../resources/minimap/blip_1x.png"
};
const unsigned char kBlip2x[] = {
#embed "../resources/minimap/blip_2x.png"
};
const unsigned char kSave1x[] = {
#embed "../resources/minimap/save_1x.png"
};
const unsigned char kSave2x[] = {
#embed "../resources/minimap/save_2x.png"
};
const unsigned char kGarage1x[] = {
#embed "../resources/minimap/garage_1x.png"
};
const unsigned char kGarage2x[] = {
#embed "../resources/minimap/garage_2x.png"
};
const unsigned char kRestart1x[] = {
#embed "../resources/minimap/restart_1x.png"
};
const unsigned char kRestart2x[] = {
#embed "../resources/minimap/restart_2x.png"
};
const unsigned char kArrest1x[] = {
#embed "../resources/minimap/arrest_1x.png"
};
const unsigned char kArrest2x[] = {
#embed "../resources/minimap/arrest_2x.png"
};
const unsigned char kCrane1x[] = {
#embed "../resources/minimap/crane_1x.png"
};
const unsigned char kCrane2x[] = {
#embed "../resources/minimap/crane_2x.png"
};
const unsigned char kInfo1x[] = {
#embed "../resources/minimap/info_1x.png"
};
const unsigned char kInfo2x[] = {
#embed "../resources/minimap/info_2x.png"
};
const unsigned char kPhoneGreen1x[] = {
#embed "../resources/minimap/phone_green_1x.png"
};
const unsigned char kPhoneGreen2x[] = {
#embed "../resources/minimap/phone_green_2x.png"
};
const unsigned char kPhoneYellow1x[] = {
#embed "../resources/minimap/phone_yellow_1x.png"
};
const unsigned char kPhoneYellow2x[] = {
#embed "../resources/minimap/phone_yellow_2x.png"
};
const unsigned char kPhoneRed1x[] = {
#embed "../resources/minimap/phone_red_1x.png"
};
const unsigned char kPhoneRed2x[] = {
#embed "../resources/minimap/phone_red_2x.png"
};
const unsigned char kStation1x[] = {
#embed "../resources/minimap/station_1x.png"
};
const unsigned char kStation2x[] = {
#embed "../resources/minimap/station_2x.png"
};

// Texture slots of port_DrawMinimap.
enum Slot
{
    kMapSlot,
    kNorthSlot,
    kArrowSlot,
    kBlipSlot,
    kGreyBlipSlot, // the blip without its red, tinted per phone
    kSaveSlot,
    kGarageSlot,  // a garage, its spray shop beside its car weapon shops
    kRestartSlot, // a hospital, where the player comes back after dying
    kArrestSlot,  // a police station, where the player comes back after an arrest
    kStationSlot, // a train station
    kCraneSlot,   // a crane, which loads cars for export
    kInfoSlot,    // a gang's information phone
    kPhoneGreenSlot,
    kPhoneYellowSlot,
    kPhoneRedSlot,
    kStyledSlot, // the MAP's picture, coloured by territory
    kWaypointArrowSlot, // the game's job arrow in the waypoint's blue
};
const s32 kNumSlots = kWaypointArrowSlot + 1;

struct Picture
{
    std::vector<u32> mPixels;
    s32 mWidth;
    s32 mHeight;
};

struct Art
{
    Slot mSlot;
    const unsigned char* mPng[2];
    size_t mSize[2];
};
const Art kArt[] = {
    {kArrowSlot, {kArrow1x, kArrow2x}, {sizeof(kArrow1x), sizeof(kArrow2x)}},
    {kBlipSlot, {kBlip1x, kBlip2x}, {sizeof(kBlip1x), sizeof(kBlip2x)}},
    {kSaveSlot, {kSave1x, kSave2x}, {sizeof(kSave1x), sizeof(kSave2x)}},
    {kGarageSlot, {kGarage1x, kGarage2x}, {sizeof(kGarage1x), sizeof(kGarage2x)}},
    {kRestartSlot, {kRestart1x, kRestart2x}, {sizeof(kRestart1x), sizeof(kRestart2x)}},
    {kArrestSlot, {kArrest1x, kArrest2x}, {sizeof(kArrest1x), sizeof(kArrest2x)}},
    {kStationSlot, {kStation1x, kStation2x}, {sizeof(kStation1x), sizeof(kStation2x)}},
    {kCraneSlot, {kCrane1x, kCrane2x}, {sizeof(kCrane1x), sizeof(kCrane2x)}},
    {kInfoSlot, {kInfo1x, kInfo2x}, {sizeof(kInfo1x), sizeof(kInfo2x)}},
    {kPhoneGreenSlot, {kPhoneGreen1x, kPhoneGreen2x}, {sizeof(kPhoneGreen1x), sizeof(kPhoneGreen2x)}},
    {kPhoneYellowSlot, {kPhoneYellow1x, kPhoneYellow2x}, {sizeof(kPhoneYellow1x), sizeof(kPhoneYellow2x)}},
    {kPhoneRedSlot, {kPhoneRed1x, kPhoneRed2x}, {sizeof(kPhoneRed1x), sizeof(kPhoneRed2x)}},
};

// By slot and scale (0 for 1x, 1 for 2x; the N has one size, drawn with nearest texels).
Picture sPictures[kNumSlots][2];
bool sLoaded;
bool sLoadedOk;

// The N on the border: 5x7 texels, outlined in black.
const char* const kNorthGlyph[7] = {"#...#", "##..#", "##..#", "#.#.#", "#..##", "#..##", "#...#"};

// In HUD pixels at 1x: the map's square, its border and its distance from the screen's bottom left corner.
const f32 kMapSize = 76.0f;
const f32 kBorder = 2.0f;
const f32 kMargin = 8.0f;
const f32 kGangIconSize = 11.0f;

// Blocks across the map: closer on foot, further out as the camera rises with speed (its height 0 to 14).
const f32 kMinBlocksAcross = 22.0f;
const f32 kMaxBlocksAcross = 36.0f;
const f32 kBlocksPerCameraHeight = 1.0f;

// Pins up to this many times the map's half size away show at its edge.
const f32 kPinLeadOut = 2.0f;

// A target this far above or below the player (blocks) shows as a triangle pointing that way.
const f32 kLevelTolerance = 0.5f;

// The map: 256x256 blocks at 4 texels each, a texel the average colour of 16x16 texels of the block's lid.
const s32 kMapBlocks = 256;
const s32 kTexelsPerBlock = 4;
const s32 kMapTexels = kMapBlocks * kTexelsPerBlock;
const s32 kLevels = 8;

std::vector<u32> sMap;
u32 sMapVersion;
// The MAP's picture: the map's brightness in each territory's colour, as the city maps printed with the game colour them.
std::vector<u32> sStyled;
u32 sStyledVersion;
const void* spBlocks;

struct Pin
{
    f32 mX, mY;
    Slot mSlot;
};
std::vector<Pin> sPins;

// A gang's territory: the gang zones (Gang_14) of its name.
struct Turf
{
    Gang* mGang;
    f32 mRect[4]; // left, top, right, bottom in blocks
};
std::vector<Turf> sTurfs;

// Frames until the pins are looked for again: the garages are objects the level's script creates after the map loads.
s32 sPinCountdown;
const s32 kPinRefreshFrames = 120;

bool Decode(const unsigned char* pPng, size_t size, Picture* pOut)
{
    SDL_Surface* pLoaded = SDL_LoadPNG_IO(SDL_IOFromConstMem(pPng, size), true);
    SDL_Surface* pSurface = pLoaded ? SDL_ConvertSurface(pLoaded, SDL_PIXELFORMAT_RGBA32) : NULL;
    SDL_DestroySurface(pLoaded);
    if (!pSurface)
    {
        return false;
    }
    pOut->mWidth = pSurface->w;
    pOut->mHeight = pSurface->h;
    pOut->mPixels.resize(static_cast<size_t>(pSurface->w) * pSurface->h);
    for (s32 y = 0; y < pSurface->h; y++)
    {
        memcpy(&pOut->mPixels[static_cast<size_t>(y) * pSurface->w], static_cast<const u8*>(pSurface->pixels) + y * pSurface->pitch,
               static_cast<size_t>(pSurface->w) * 4);
    }
    SDL_DestroySurface(pSurface);
    return true;
}

bool Load()
{
    if (sLoaded)
    {
        return sLoadedOk;
    }
    sLoaded = true;
    sLoadedOk = true;
    for (const Art& art : kArt)
    {
        for (s32 s = 0; s < 2; s++)
        {
            sLoadedOk &= Decode(art.mPng[s], art.mSize[s], &sPictures[art.mSlot][s]);
        }
    }
    // The grey blip: the red dome's shading, its brightest texel white, to take a phone's colour.
    for (s32 s = 0; s < 2 && sLoadedOk; s++)
    {
        const Picture& blip = sPictures[kBlipSlot][s];
        Picture& grey = sPictures[kGreyBlipSlot][s];
        grey = blip;
        u32 brightest = 1;
        for (u32 c : blip.mPixels)
        {
            brightest = (c & 0xFF) > brightest ? (c & 0xFF) : brightest;
        }
        for (u32& c : grey.mPixels)
        {
            const u32 v = (c & 0xFF) * 255 / brightest;
            c = (c & 0xFF000000u) | v | (v << 8) | (v << 16);
        }
    }
    // The N, white in a black outline.
    Picture& north = sPictures[kNorthSlot][0];
    north.mWidth = 7;
    north.mHeight = 9;
    north.mPixels.assign(7 * 9, 0);
    for (s32 y = 0; y < 7; y++)
    {
        for (s32 x = 0; x < 5; x++)
        {
            if (kNorthGlyph[y][x] != '#')
            {
                continue;
            }
            for (s32 dy = 0; dy <= 2; dy++)
            {
                for (s32 dx = 0; dx <= 2; dx++)
                {
                    u32& c = north.mPixels[(y + dy) * 7 + x + dx];
                    c = c ? c : 0xFF000000u;
                }
            }
        }
    }
    for (s32 y = 0; y < 7; y++)
    {
        for (s32 x = 0; x < 5; x++)
        {
            if (kNorthGlyph[y][x] == '#')
            {
                north.mPixels[(y + 1) * 7 + x + 1] = 0xFFFFFFFFu;
            }
        }
    }
    sPictures[kNorthSlot][1] = north;
    return sLoadedOk;
}

u32 Rgba(f32 r, f32 g, f32 b, f32 a)
{
    const auto byte = [](f32 v) { return static_cast<u32>(fminf(fmaxf(v, 0.0f), 1.0f) * 255.0f + 0.5f); };
    return byte(r) | (byte(g) << 8) | (byte(b) << 16) | (byte(a) << 24);
}

f32 ToF(Fix16 v)
{
    return v.ToFloat();
}

// Per lid tile, -1 until worked out: the share of its texels that are see-through (colour 0), and its colour in 4x4
// cells (RGB, 0 to 1). Tiles live four to a 256 texel wide row (gtx_0x106C::get_tile), their palette is the tile's
// (palette_types_enum::tiles_1), a physical palette's colours 64 palettes apart (gtx_0x106C::GetPalData), BGRA.
const s32 kNumTiles = block_face::tile_idx_mask + 1;
f32 sTileClear[kNumTiles];
f32 sTileCells[kNumTiles][kTexelsPerBlock][kTexelsPerBlock][3];

void ReadTile(s32 tile)
{
    if (sTileClear[tile] >= 0.0f)
    {
        return;
    }
    const u8* pTexels = gGtx_0x106C->get_tile(static_cast<u16>(tile));
    const s16 palette = gGtx_0x106C->GetTruePalette(palette_types_enum::tiles_1, static_cast<s16>(tile));
    const u8* pPalette = gGtx_0x106C->GetPalData(gGtx_0x106C->get_phys_pal(palette));
    const s32 cell = 64 / kTexelsPerBlock;
    s32 clear = 0;
    for (s32 cy = 0; cy < kTexelsPerBlock; cy++)
    {
        for (s32 cx = 0; cx < kTexelsPerBlock; cx++)
        {
            f32 sum[3] = {0, 0, 0};
            s32 n = 0;
            for (s32 y = cy * cell; y < (cy + 1) * cell; y++)
            {
                for (s32 x = cx * cell; x < (cx + 1) * cell; x++)
                {
                    const u8 index = pTexels[y * 256 + x];
                    if (!index)
                    {
                        clear++;
                        continue;
                    }
                    const u8* pColour = pPalette + index * 256;
                    sum[0] += pColour[2];
                    sum[1] += pColour[1];
                    sum[2] += pColour[0];
                    n++;
                }
            }
            for (s32 k = 0; k < 3; k++)
            {
                sTileCells[tile][cy][cx][k] = n ? sum[k] / (n * 255.0f) : 0.0f;
            }
        }
    }
    sTileClear[tile] = clear / 4096.0f;
}

// The top lid of a column: its level (1 and 2 the ground, 3 up roofs) and face word. A mostly see-through lid above
// the ground lets the one below show, as for tree tops and wires (render_map.py).
bool TopLid(s32 x, s32 y, s32* pHeight, u16* pLid)
{
    for (s32 z = kLevels - 1; z >= 0; z--)
    {
        const gmp_block_info* pBlock = gMap_0x370->GetBlockOrNull(static_cast<u8>(x), static_cast<u8>(y), static_cast<u8>(z));
        const s32 tile = pBlock ? pBlock->mLid & block_face::tile_idx_mask : 0;
        if (!tile)
        {
            continue;
        }
        ReadTile(tile);
        if (z > 0 && sTileClear[tile] > 0.6f)
        {
            continue;
        }
        *pHeight = z;
        *pLid = pBlock->mLid;
        return true;
    }
    return false;
}

// The map from the lids as seen from above (turned and flipped as the face word says), roofs lighter as they rise and
// a shadow to the bottom right of what stands higher (render_map.py's shading).
// Water, or no lid at all (the sea round the city), per block, for the MAP's colours.
bool sWater[kMapBlocks][kMapBlocks];

void BuildMap()
{
    static s8 sHeight[kMapBlocks][kMapBlocks];
    static u16 sLid[kMapBlocks][kMapBlocks];
    for (f32& clear : sTileClear)
    {
        clear = -1.0f; // the level's style may differ from the last one's
    }
    for (s32 y = 0; y < kMapBlocks; y++)
    {
        for (s32 x = 0; x < kMapBlocks; x++)
        {
            s32 height = 0;
            u16 lid = 0;
            sHeight[y][x] = TopLid(x, y, &height, &lid) ? static_cast<s8>(height) : -1;
            sLid[y][x] = lid;
            sWater[y][x] = sHeight[y][x] < 0 || gGtx_0x106C->IsWaterTile(lid & block_face::tile_idx_mask);
        }
    }
    const auto heightAt = [](s32 x, s32 y) {
        x = x < 0 ? 0 : (x >= kMapBlocks ? kMapBlocks - 1 : x);
        y = y < 0 ? 0 : (y >= kMapBlocks ? kMapBlocks - 1 : y);
        return static_cast<f32>(sHeight[y][x] < 0 ? 0 : sHeight[y][x]);
    };
    sMap.assign(static_cast<size_t>(kMapTexels) * kMapTexels, Rgba(0.04f, 0.06f, 0.08f, 1.0f));
    for (s32 y = 0; y < kMapBlocks; y++)
    {
        for (s32 x = 0; x < kMapBlocks; x++)
        {
            if (sHeight[y][x] < 0)
            {
                continue;
            }
            const f32 h = heightAt(x, y);
            f32 cast = 0.0f;
            for (s32 k = 1; k <= 2; k++)
            {
                cast = fmaxf(cast, fminf(fmaxf(heightAt(x - k, y - k) - h, 0.0f), 3.0f) / (3.0f * k));
            }
            // Brighter than the lids' own colours, which are dark at this size.
            const f32 shade = 1.25f * (0.85f + 0.045f * fminf(fmaxf(h - 2.0f, 0.0f), 6.0f)) * (1.0f - 0.35f * fminf(cast, 1.0f));
            const u16 lid = sLid[y][x];
            const s32 tile = lid & block_face::tile_idx_mask;
            const bool flip = (lid & block_face::flip) != 0;
            const s32 turns = (lid >> 14) & 3; // quarter turns clockwise
            for (s32 ty = 0; ty < kTexelsPerBlock; ty++)
            {
                for (s32 tx = 0; tx < kTexelsPerBlock; tx++)
                {
                    // The tile's cell shown here: undo the turns, then the flip.
                    s32 cx = tx, cy = ty;
                    for (s32 t = 0; t < turns; t++)
                    {
                        const s32 ox = cx;
                        cx = cy;
                        cy = kTexelsPerBlock - 1 - ox;
                    }
                    if (flip)
                    {
                        cx = kTexelsPerBlock - 1 - cx;
                    }
                    const f32* c = sTileCells[tile][cy][cx];
                    sMap[static_cast<size_t>(y * kTexelsPerBlock + ty) * kMapTexels + x * kTexelsPerBlock + tx] =
                        Rgba(c[0] * shade, c[1] * shade, c[2] * shade, 1.0f);
                }
            }
        }
    }
    sMapVersion++;
}

// The gangs' territories, from the map's gang zones.
void FindTurfs()
{
    sTurfs.clear();
    if (!gGangPool_CA8 || !gMap_0x370->mpZones)
    {
        return;
    }
    for (u16 i = 0; i < gMap_0x370->mpZones->mNumZones; i++)
    {
        const gmp_map_zone* pZone = gMap_0x370->GetZone(i);
        Gang* pGang = pZone && pZone->mZoneType == Gang_14 ? gGangPool_CA8->FindGangByName(pZone->mName) : NULL;
        // As the respect meters (GangPool::FirstGang): the zones of the police and others are no gang of the player's.
        if (pGang && pGang->mUsed && pGang->HasKillRespectChange() && pGang->mArrowColour)
        {
            sTurfs.push_back(Turf{pGang, {static_cast<f32>(pZone->mX), static_cast<f32>(pZone->mY), static_cast<f32>(pZone->mX + pZone->mW),
                                          static_cast<f32>(pZone->mY + pZone->mH)}});
        }
    }
}

// The church: an area where the player on foot starts a thread whose first commands save the game (THREAD_DECLARE5,
// then DO_SAVE_GAME, as tools/fullmap.py reads the .scr). The trigger's position is the area's centre (its touch point).
void FindChurches()
{
    if (!gScriptManager)
    {
        return;
    }
    const u16 kNumBasePointers = GTA2_COUNTOF(gScriptManager->mBasePointers);
    for (u16 i = 0; i < kNumBasePointers; i++)
    {
        const SCR_CMD_HEADER* pCmd = gScriptManager->GetBasePointer(i);
        if (!pCmd || pCmd->mType != SCRCMD_THREAD_DECLARE5)
        {
            continue;
        }
        const SCR_CHAR_AREA_ANY* pArea = reinterpret_cast<const SCR_CHAR_AREA_ANY*>(pCmd);
        s32 next = pArea->mUnusedE; // the thread's first command
        bool saves = false;
        for (s32 k = 0; k < 4 && next > 0 && next < kNumBasePointers && !saves; k++)
        {
            const SCR_CMD_HEADER* pThreadCmd = gScriptManager->GetBasePointer(static_cast<u16>(next));
            if (!pThreadCmd)
            {
                break;
            }
            saves = pThreadCmd->mType == SCRCMD_DO_SAVE_GAME;
            next = pThreadCmd->mCmdNext;
        }
        if (saves)
        {
            sPins.push_back(Pin{ToF(pArea->mX), ToF(pArea->mY), kSaveSlot});
        }
    }
}

// A pin, unless one of its kind is already within this many blocks: one for a garage's bays, a station's platforms.
void AddPin(f32 x, f32 y, Slot slot, f32 apart)
{
    for (const Pin& pin : sPins)
    {
        if (pin.mSlot == slot && fabsf(pin.mX - x) < apart && fabsf(pin.mY - y) < apart)
        {
            return;
        }
    }
    sPins.push_back(Pin{x, y, slot});
}

// The garages: car shop bays (car_shop_130 objects), each respraying or selling a car weapon (Car::HandleShops).
// A garage is a row of bays two blocks apart (machine guns, oil, mines, respray, car bomb): one pin for it.
void AddGarageBay(f32 x, f32 y)
{
    AddPin(x, y, kGarageSlot, 12.0f);
}

// Those the level's script declares (OBJ_DATA, opcodes 15 to 20, as tools/fullmap.py reads them), wherever the player is,
// and those in the object pool, which a job's script may add.
void FindGarages()
{
    if (gScriptManager)
    {
        const u16 kNumBasePointers = GTA2_COUNTOF(gScriptManager->mBasePointers);
        for (u16 i = 0; i < kNumBasePointers; i++)
        {
            const SCR_CMD_HEADER* pCmd = gScriptManager->GetBasePointer(i);
            if (!pCmd || pCmd->mType < SCRCMD_OBJ_DECSET_2D || pCmd->mType > SCRCMD_OBJ_DECSET_3D_STR)
            {
                continue;
            }
            const SCR_OBJ_DATA* pObj = reinterpret_cast<const SCR_OBJ_DATA*>(pCmd);
            if (pObj->mObjId == objects::car_shop_130)
            {
                AddGarageBay(ToF(pObj->mPos.x), ToF(pObj->mPos.y));
            }
        }
    }
    for (Object* pObj = gObject_2C_Pool ? gObject_2C_Pool->GetFirstUsed() : NULL; pObj; pObj = pObj->mpNext)
    {
        if (pObj->mModel != objects::car_shop_130 || !pObj->mSprite)
        {
            continue;
        }
        AddGarageBay(ToF(pObj->mSprite->mXy.x), ToF(pObj->mSprite->mXy.y));
    }
}

// The cranes the level's script set up (CranePool), at their rotors.
void FindCranes()
{
    if (!gCranePool_D9C)
    {
        return;
    }
    for (s32 i = 0; i < gCranePool_D9C->mCount && i < GTA2_COUNTOF_S(gCranePool_D9C->mCranes); i++)
    {
        const Crane& crane = gCranePool_D9C->mCranes[i];
        if (crane.mRotorObj && crane.mRotorObj->mSprite)
        {
            AddPin(ToF(crane.mRotorObj->mSprite->mXy.x), ToF(crane.mRotorObj->mSprite->mXy.y), kCraneSlot, 4.0f);
        }
    }
}

// The hospitals and police stations (the map's restart zones) and the train stations (their platforms' zones).
void FindZonePins()
{
    if (!gMap_0x370->mpZones)
    {
        return;
    }
    for (u16 i = 0; i < gMap_0x370->mpZones->mNumZones; i++)
    {
        const gmp_map_zone* pZone = gMap_0x370->GetZone(i);
        if (!pZone)
        {
            continue;
        }
        const f32 x = pZone->mX + pZone->mW / 2.0f, y = pZone->mY + pZone->mH / 2.0f;
        switch (pZone->mZoneType)
        {
            case gmp_zone_type_enum::restart:
                AddPin(x, y, kRestartSlot, 2.0f);
                break;
            case gmp_zone_type_enum::arrest_restart:
                AddPin(x, y, kArrestSlot, 2.0f);
                break;
            case gmp_zone_type_enum::railway_station_platform:
                AddPin(x, y, kStationSlot, 8.0f);
                break;
        }
    }
}

// Where the minimap is on screen, and what it shows.
struct View
{
    f32 mUi;                // the HUD's scale
    s32 mScale;             // 0 for the 1x pictures, 1 for 2x
    f32 mHole[4];           // the map: left, top, right, bottom
    f32 mCentreX, mCentreY; // the player, on screen
    f32 mPlayerX, mPlayerY; // and in the world (blocks)
    f32 mPixelsPerBlock;
    f32 mAlpha;
};

void DrawRect(f32 left, f32 top, f32 right, f32 bottom, u32 rgba)
{
    const f32 xyuv[24] = {left, top, 0, 0, right, top, 0, 0, right, bottom, 0, 0, left, top, 0, 0, right, bottom, 0, 0, left, bottom, 0, 0};
    const u32 colours[6] = {rgba, rgba, rgba, rgba, rgba, rgba};
    port_DrawMinimap(-1, 0, 0, NULL, 0, xyuv, colours, 6, false);
}

// A picture centred at (x, y), at its size on the HUD times size.
void DrawIcon(const View& v, Slot slot, f32 x, f32 y, u32 rgba, bool nearest = false, f32 size = 1.0f)
{
    const Picture& p = sPictures[slot][v.mScale];
    const f32 k = (slot == kNorthSlot ? v.mUi : v.mUi / (v.mScale + 1)) * size;
    const f32 hw = p.mWidth * k / 2;
    const f32 hh = p.mHeight * k / 2;
    const f32 l = x - hw, t = y - hh, r = x + hw, b = y + hh;
    const f32 xyuv[24] = {l, t, 0, 0, r, t, 1, 0, r, b, 1, 1, l, t, 0, 0, r, b, 1, 1, l, b, 0, 1};
    const u32 colours[6] = {rgba, rgba, rgba, rgba, rgba, rgba};
    port_DrawMinimap(slot, p.mWidth, p.mHeight, p.mPixels.data(), static_cast<u32>(v.mScale + 1), xyuv, colours, 6, nearest);
}

void DrawTriangle(f32 x, f32 y, f32 size, bool up, u32 rgba)
{
    const f32 dir = up ? -1.0f : 1.0f;
    const f32 xyuv[12] = {x - size, y - dir * size * 0.6f, 0, 0, x + size, y - dir * size * 0.6f, 0, 0, x, y + dir * size * 0.8f, 0, 0};
    const u32 colours[3] = {rgba, rgba, rgba};
    port_DrawMinimap(-1, 0, 0, NULL, 0, xyuv, colours, 3, false);
}

// A world position on screen, held inside the map less inset: false when it is further than maxOut times the map's
// half size. pOut: how many half sizes away it is (1 at the edge).
bool ToScreen(const View& v, f32 wx, f32 wy, f32 inset, f32 maxOut, f32* pX, f32* pY, f32* pOut)
{
    const f32 halfW = (v.mHole[2] - v.mHole[0]) / 2 - inset;
    const f32 halfH = (v.mHole[3] - v.mHole[1]) / 2 - inset;
    const f32 dx = (wx - v.mPlayerX) * v.mPixelsPerBlock;
    const f32 dy = (wy - v.mPlayerY) * v.mPixelsPerBlock;
    const f32 out = fmaxf(fabsf(dx) / halfW, fabsf(dy) / halfH);
    *pOut = out;
    if (out > maxOut)
    {
        return false;
    }
    const f32 k = out > 1.0f ? 1.0f / out : 1.0f;
    *pX = v.mCentreX + dx * k;
    *pY = v.mCentreY + dy * k;
    return true;
}

// Full inside the map, fading to half from 1 to 5 times its size (as the radar mods for the original do).
f32 EdgeAlpha(f32 out)
{
    return out <= 1.0f ? 1.0f : (out >= 5.0f ? 0.5f : 1.0f - 0.5f * (out - 1.0f) / 4.0f);
}

void DrawMap(const View& v, bool styled = false)
{
    const f32* h = v.mHole;
    const f32 ppb = v.mPixelsPerBlock;
    const auto u = [&](f32 sx) { return (v.mPlayerX + (sx - v.mCentreX) / ppb) / kMapBlocks; };
    const auto w = [&](f32 sy) { return (v.mPlayerY + (sy - v.mCentreY) / ppb) / kMapBlocks; };
    // Off the map's edge: the empty map's colour.
    DrawRect(h[0], h[1], h[2], h[3], Rgba(0.04f, 0.06f, 0.08f, v.mAlpha));
    const f32 left = fmaxf(h[0], v.mCentreX - v.mPlayerX * ppb);
    const f32 top = fmaxf(h[1], v.mCentreY - v.mPlayerY * ppb);
    const f32 right = fminf(h[2], v.mCentreX + (kMapBlocks - v.mPlayerX) * ppb);
    const f32 bottom = fminf(h[3], v.mCentreY + (kMapBlocks - v.mPlayerY) * ppb);
    if (right <= left || bottom <= top)
    {
        return;
    }
    const f32 xyuv[24] = {left, top, u(left), w(top), right, top, u(right), w(top), right, bottom, u(right), w(bottom),
                          left, top, u(left), w(top), right, bottom, u(right), w(bottom), left, bottom, u(left), w(bottom)};
    const u32 white = Rgba(1, 1, 1, v.mAlpha);
    const u32 colours[6] = {white, white, white, white, white, white};
    if (styled)
    {
        port_DrawMinimap(kStyledSlot, kMapTexels, kMapTexels, sStyled.data(), sStyledVersion, xyuv, colours, 6, false);
    }
    else
    {
        port_DrawMinimap(kMapSlot, kMapTexels, kMapTexels, sMap.data(), sMapVersion, xyuv, colours, 6, false);
    }
}

void DrawPins(const View& v)
{
    for (const Pin& pin : sPins)
    {
        // As GTA3's radar: a place a little past the edge already shows there, fading, before it comes into view.
        f32 x, y, out;
        if (ToScreen(v, pin.mX, pin.mY, 4.0f * v.mUi, kPinLeadOut, &x, &y, &out))
        {
            DrawIcon(v, pin.mSlot, x, y, Rgba(1, 1, 1, EdgeAlpha(out) * v.mAlpha));
        }
    }
}

// A job's targets: pink, as GTA3's radar shows them, apart from the phones' red, and a little larger.
const f32 kTargetSize = 1.3f;
u32 TargetColour(f32 alpha)
{
    return Rgba(1.0f, 0.35f, 0.8f, alpha);
}

// A job phone whose gang the player has too little respect with for its job yet (Hud_Arrow::CheckVisibility): its blip
// darker.
bool PhoneAvailable(const Hud_ArrowTargets& t)
{
    const Player* pPlayer = gGame_0x40 ? gGame_0x40->mLocalPlayer : NULL;
    return !pPlayer || !t.mGangInfo.mGang || t.mGangInfo.mGang->GetRespectForPlayer(pPlayer->get_idx()) >= t.mGangInfo.mMinRespect;
}

// The phones' colours by the arrow's (Hud.cpp get_phone_colour): 1 yellow, 2 red, 3 green.
Slot PhoneSlot(s32 arrowColour)
{
    return arrowColour == 1 ? kPhoneYellowSlot : (arrowColour == 2 ? kPhoneRedSlot : kPhoneGreenSlot);
}
// A phone the player cannot take yet: darker.
const f32 kLockedShade = 0.45f;

// A gang's icon from the respect meters (user sprite 63 plus its arrow colour, as Hud_GangRespectBars draws it), from
// the player's own style file, size HUD pixels wide.
void DrawGangIcon(const View& v, s32 arrowColour, f32 x, f32 y, f32 size)
{
    const s16 sprite = static_cast<s16>(63 + arrowColour);
    const sprite_index* pIndex = gGtx_0x106C->get_sprite_index(gGtx_0x106C->GetSpriteTrueIndex(sprite_types_enum::user_6, sprite));
    const s32 texels = pIndex ? (pIndex->mWidth > pIndex->mHeight ? pIndex->mWidth : pIndex->mHeight) : 0;
    if (!texels)
    {
        return;
    }
    const s32 palette = palette_types_enum::sprites_2;
    DrawFigure(sprite_types_enum::user_6, sprite, Fix16(x), Fix16(y), Ang16(0), Fix16(v.mUi * size / texels), palette, 0, 0, 0, false);
}

// The HUD's arrows: a job's targets, and while off a job the gangs' phones (the arrow's colour 1 to 3) and info phones
// (4), shown as the game shows their arrows or when close.
void DrawTargets(const View& v, f32 playerZ, bool city = false)
{
    const bool onMission = gScriptManager && gScriptManager->IsOnMission();
    for (Hud_Arrow& arrow : gHud_2B00->mArrows.mArray)
    {
        if (arrow.HasNeitherTarget())
        {
            continue;
        }
        const Hud_ArrowTargets& t = arrow.mTargets;
        const ArrowTrace* traces[2] = {&t.mPrimaryTarget, &t.mSecondaryTarget};
        for (const ArrowTrace* pTrace : traces)
        {
            if (pTrace->HasNoTarget())
            {
                continue;
            }
            const f32 wx = ToF(pTrace->mAimX), wy = ToF(pTrace->mAimY), wz = ToF(pTrace->mAimZ);
            f32 x, y, out;
            if (t.mGangInfo.mGang)
            {
                // A phone: off a job, where its arrow shows or within twice the map's size; on the city map wherever it is.
                if (onMission || !ToScreen(v, wx, wy, 3.0f * v.mUi, city ? 1.0f : 2.0f, &x, &y, &out) ||
                    (out > 1.0f && !arrow.IsVisible()))
                {
                    continue;
                }
                const f32 alpha = EdgeAlpha(out) * v.mAlpha;
                if (t.mArrowColour >= 1 && t.mArrowColour <= 3)
                {
                    const f32 k = PhoneAvailable(t) ? 1.0f : kLockedShade;
                    DrawIcon(v, PhoneSlot(t.mArrowColour), x, y, Rgba(k, k, k, alpha));
                }
                else
                {
                    DrawIcon(v, kInfoSlot, x, y, Rgba(1, 1, 1, alpha));
                }
                continue;
            }
            // A job's target, at the edge when far.
            ToScreen(v, wx, wy, 3.0f * v.mUi, 1e9f, &x, &y, &out);
            const f32 alpha = EdgeAlpha(out) * v.mAlpha;
            const f32 dz = wz - playerZ;
            if (fabsf(dz) <= kLevelTolerance)
            {
                DrawIcon(v, kGreyBlipSlot, x, y, TargetColour(alpha), false, kTargetSize);
            }
            else
            {
                const f32 size = 3.5f * v.mUi;
                DrawTriangle(x, y, size * 1.4f, dz > 0, Rgba(0, 0, 0, alpha));
                DrawTriangle(x, y, size, dz > 0, TargetColour(alpha));
            }
        }
    }
}

// The player's arrow at (cx, cy), turned as the game turns sprites (Fix16_Point::RotateByAngle, 1440 a turn).
void DrawPlayer(const View& v, const Sprite* pSprite, f32 cx, f32 cy)
{
    const Picture& p = sPictures[kArrowSlot][v.mScale];
    const f32 k = v.mUi / (v.mScale + 1);
    const f32 hw = p.mWidth * k / 2, hh = p.mHeight * k / 2;
    // A heading of 0 faces south, +y (char.cpp moves by its sine and cosine), the arrow's picture north: half a turn more.
    const f32 angle = pSprite->mRotation.rValue * (2.0f * 3.14159265f / 1440.0f) + 3.14159265f;
    const f32 s = sinf(angle), c = cosf(angle);
    const f32 corners[4][4] = {{-hw, -hh, 0, 0}, {hw, -hh, 1, 0}, {hw, hh, 1, 1}, {-hw, hh, 0, 1}};
    f32 pts[4][4];
    for (s32 i = 0; i < 4; i++)
    {
        const f32 x = corners[i][0], y = corners[i][1];
        pts[i][0] = cx + x * c + y * s;
        pts[i][1] = cy - x * s + y * c;
        pts[i][2] = corners[i][2];
        pts[i][3] = corners[i][3];
    }
    const s32 order[6] = {0, 1, 2, 0, 2, 3};
    f32 xyuv[24];
    u32 colours[6];
    for (s32 i = 0; i < 6; i++)
    {
        memcpy(&xyuv[i * 4], pts[order[i]], sizeof(pts[0]));
        colours[i] = Rgba(1, 1, 1, v.mAlpha);
    }
    port_DrawMinimap(kArrowSlot, p.mWidth, p.mHeight, p.mPixels.data(), static_cast<u32>(v.mScale + 1), xyuv, colours, 6, false);
}

// Each gang's icon, as the game's gang arrows lead there: on the border toward its information phone, above the phone
// once it is on the map. A gang without one: toward the nearest part of its territory, in the border's top left corner
// while the player is in it.
void DrawTurfs(const View& v)
{
    Gang* drawn[16];
    s32 numDrawn = 0;
    f32 placed[16][2];
    s32 numPlaced = 0;
    for (const Turf& turf : sTurfs)
    {
        bool seen = false;
        for (s32 i = 0; i < numDrawn && !seen; i++)
        {
            seen = drawn[i] == turf.mGang;
        }
        if (seen)
        {
            continue;
        }
        // The nearest point of all its zones.
        f32 bestX = 0, bestY = 0, best = 1e9f;
        for (const Turf& other : sTurfs)
        {
            if (other.mGang != turf.mGang)
            {
                continue;
            }
            const f32 nx = fminf(fmaxf(v.mPlayerX, other.mRect[0]), other.mRect[2]);
            const f32 ny = fminf(fmaxf(v.mPlayerY, other.mRect[1]), other.mRect[3]);
            const f32 d = (nx - v.mPlayerX) * (nx - v.mPlayerX) + (ny - v.mPlayerY) * (ny - v.mPlayerY);
            if (d < best)
            {
                best = d;
                bestX = nx;
                bestY = ny;
            }
        }
        if (numDrawn < 16)
        {
            drawn[numDrawn++] = turf.mGang;
        }
        // On the border, the direction stretched to it; in its territory, the top left corner.
        const bool inside = best < 0.01f;
        const Gang* pGang = turf.mGang;
        const bool phone = pGang->mInfoPhoneX.mValue || pGang->mInfoPhoneY.mValue;
        if (phone)
        {
            bestX = ToF(pGang->mInfoPhoneX);
            bestY = ToF(pGang->mInfoPhoneY);
        }
        const bool corner = inside && !phone;
        const f32 dx = corner ? -1.0f : bestX - v.mPlayerX, dy = corner ? -1.0f : bestY - v.mPlayerY;
        const f32 halfW = (v.mHole[2] - v.mHole[0]) / 2, halfH = (v.mHole[3] - v.mHole[1]) / 2;
        const f32 out = fmaxf(fabsf(dx * v.mPixelsPerBlock) / halfW, fabsf(dy * v.mPixelsPerBlock) / halfH);
        if (phone && out < 0.85f)
        {
            // The phone in view: the icon just above it, which says whose it is.
            DrawGangIcon(v, pGang->mArrowColour, v.mCentreX + dx * v.mPixelsPerBlock, v.mCentreY + dy * v.mPixelsPerBlock - 9.0f * v.mUi,
                         kGangIconSize);
            continue;
        }
        const f32 k = 1.0f / fmaxf(fabsf(dx) / halfW, fabsf(dy) / halfH);
        f32 x = corner ? v.mHole[0] : v.mCentreX + dx * k;
        f32 y = corner ? v.mHole[1] : v.mCentreY + dy * k;
        // Beside the N rather than over it, and along the border from the icons already there.
        const f32 clearance = (kGangIconSize / 2 + 5.0f) * v.mUi;
        if (y < v.mHole[1] + clearance && fabsf(x - v.mCentreX) < clearance)
        {
            x = v.mCentreX + (x < v.mCentreX ? -clearance : clearance);
        }
        const bool alongX = fabsf(dy) / halfH >= fabsf(dx) / halfW; // on the top or bottom edge
        const f32 step = (kGangIconSize + 1.0f) * v.mUi;
        for (s32 tries = 0; tries < 4; tries++)
        {
            bool overlaps = false;
            for (s32 i = 0; i < numPlaced && !overlaps; i++)
            {
                overlaps = fabsf(placed[i][0] - x) < step && fabsf(placed[i][1] - y) < step;
            }
            if (!overlaps)
            {
                break;
            }
            // Toward the side its territory lies on, else away from the centre.
            const f32 side = alongX ? (dx != 0 ? dx : x - v.mCentreX) : (dy != 0 ? dy : y - v.mCentreY);
            (alongX ? x : y) += side < 0 ? -step : step;
        }
        if (numPlaced < 16)
        {
            placed[numPlaced][0] = x;
            placed[numPlaced][1] = y;
            numPlaced++;
        }
        DrawGangIcon(v, turf.mGang->mArrowColour, x, y, kGangIconSize);
    }
}
// The waypoint set on the pause menu's MAP, cleared as the player reaches it or the level changes.
bool sWaypoint;
f32 sWaypointX, sWaypointY;
const f32 kWaypointReached = 3.0f;

u32 WaypointColour(f32 alpha)
{
    return Rgba(0.25f, 0.85f, 1.0f, alpha);
}

void DrawDiamond(f32 x, f32 y, f32 size, u32 rgba)
{
    const f32 xyuv[24] = {x, y - size, 0, 0, x + size, y, 0, 0, x, y + size, 0, 0, x, y - size, 0, 0, x, y + size, 0, 0, x - size, y, 0, 0};
    const u32 colours[6] = {rgba, rgba, rgba, rgba, rgba, rgba};
    port_DrawMinimap(-1, 0, 0, NULL, 0, xyuv, colours, 6, false);
}

void DrawWaypointMark(f32 x, f32 y, f32 ui, f32 alpha)
{
    DrawDiamond(x, y, 5.5f * ui, Rgba(0, 0, 0, alpha));
    DrawDiamond(x, y, 4.0f * ui, WaypointColour(alpha));
}

// Each gang's colour, the brighter half of its respect meter icon's texels (user sprite 63 plus its arrow colour, as
// tools/fullmap.py takes it), read from the player's style once per level.
const s32 kMaxArrowColours = 16;
f32 sGangColours[kMaxArrowColours][3];
bool sGangColourRead[kMaxArrowColours];

bool GangColour(s32 arrowColour, f32* pRgb)
{
    if (arrowColour <= 0 || arrowColour >= kMaxArrowColours)
    {
        return false;
    }
    if (!sGangColourRead[arrowColour])
    {
        sGangColourRead[arrowColour] = true;
        sGangColours[arrowColour][0] = -1;
        const u16 index = gGtx_0x106C->GetSpriteTrueIndex(sprite_types_enum::user_6, static_cast<s16>(63 + arrowColour));
        const sprite_index* pSprite = gGtx_0x106C->get_sprite_index(index);
        if (pSprite && pSprite->mpData)
        {
            const s16 palette = gGtx_0x106C->GetTruePalette(palette_types_enum::sprites_2, static_cast<s16>(index));
            const u8* pPalette = gGtx_0x106C->GetPalData(gGtx_0x106C->get_phys_pal(palette));
            // Brightness, then colour, of each opaque texel; the brighter half averaged.
            std::vector<f32> texels;
            for (s32 y = 0; y < pSprite->mHeight; y++)
            {
                for (s32 x = 0; x < pSprite->mWidth; x++)
                {
                    const u8 c = pSprite->mpData[y * 256 + x];
                    if (c)
                    {
                        const u8* pColour = pPalette + c * 256;
                        texels.push_back(static_cast<f32>(pColour[0] + pColour[1] + pColour[2]));
                        texels.push_back(pColour[2] / 255.0f);
                        texels.push_back(pColour[1] / 255.0f);
                        texels.push_back(pColour[0] / 255.0f);
                    }
                }
            }
            const size_t n = texels.size() / 4;
            if (n)
            {
                std::vector<f32> brightness(n);
                for (size_t i = 0; i < n; i++)
                {
                    brightness[i] = texels[i * 4];
                }
                std::vector<f32> sorted = brightness;
                std::sort(sorted.begin(), sorted.end());
                const f32 median = sorted[n / 2];
                f32 sum[3] = {0, 0, 0};
                s32 count = 0;
                for (size_t i = 0; i < n; i++)
                {
                    if (brightness[i] >= median)
                    {
                        for (s32 k = 0; k < 3; k++)
                        {
                            sum[k] += texels[i * 4 + 1 + k];
                        }
                        count++;
                    }
                }
                for (s32 k = 0; k < 3; k++)
                {
                    sGangColours[arrowColour][k] = sum[k] / count;
                }
            }
        }
    }
    if (sGangColours[arrowColour][0] < 0)
    {
        return false;
    }
    for (s32 k = 0; k < 3; k++)
    {
        pRgb[k] = sGangColours[arrowColour][k];
    }
    return true;
}

// A waypoint at (x, y), or none if one was within near blocks of it.
void ToggleWaypoint(f32 x, f32 y, f32 near)
{
    if (sWaypoint && fabsf(sWaypointX - x) < near && fabsf(sWaypointY - y) < near)
    {
        sWaypoint = false;
        return;
    }
    sWaypoint = true;
    sWaypointX = fminf(fmaxf(x, 0.0f), static_cast<f32>(kMapBlocks));
    sWaypointY = fminf(fmaxf(y, 0.0f), static_cast<f32>(kMapBlocks));
}

// The MAP's district names, where DrawCityMap put them, for the pause menu to write.
struct Label
{
    wchar_t mText[48];
    f32 mX, mY;
};
const s32 kMaxLabels = 64;
Label sLabels[kMaxLabels];
s32 sNumLabels;

// The territories' colours, as the printed city maps have them, by the gang's name (its first four letters, as
// GangPool::FindGangByName matches); another level's gangs take their icon's (GangColour).
struct TerritoryColour
{
    const char* mPrefix;
    const wchar_t* mName;
    f32 mRgb[3];
};
const TerritoryColour kTerritoryColours[] = {
    {"yaku", L"YAKUZA", {0.22f, 0.30f, 0.72f}},     {"zaib", L"ZAIBATSU", {0.56f, 0.56f, 0.58f}},
    {"loon", L"LOONIES", {0.20f, 0.60f, 0.36f}},    {"redn", L"REDNECKS", {0.20f, 0.44f, 0.84f}},
    {"scie", L"SCIENTISTS", {0.86f, 0.64f, 0.18f}}, {"russ", L"RUSSIANS", {0.82f, 0.18f, 0.14f}},
    {"kris", L"KRISHNA", {0.96f, 0.58f, 0.22f}},
};
const f32 kNeutralColour[3] = {0.47f, 0.45f, 0.86f};
const f32 kWaterColour[3] = {0.38f, 0.60f, 0.86f};

bool TurfColour(const Gang* pGang, f32* pRgb, const wchar_t** ppName)
{
    *ppName = NULL;
    for (const TerritoryColour& t : kTerritoryColours)
    {
        bool same = true;
        for (s32 k = 0; k < 4 && same; k++)
        {
            const char a = pGang->mName[k], b = t.mPrefix[k];
            same = (a >= 'A' && a <= 'Z' ? a - 'A' + 'a' : a) == b;
        }
        if (same)
        {
            memcpy(pRgb, t.mRgb, sizeof(t.mRgb));
            *ppName = t.mName;
            return true;
        }
    }
    return GangColour(pGang->mArrowColour, pRgb);
}

// The MAP key's colour rows after its icons: neutral, each gang's territory, water.
struct KeyRow
{
    const wchar_t* mName;
    f32 mRgb[3];
};
std::vector<KeyRow> sKeyRows;

void BuildStyled()
{
    static s8 sTurf[kMapBlocks][kMapBlocks]; // index in colours, -1 neutral
    std::vector<KeyRow> colours;
    memset(sTurf, -1, sizeof(sTurf));
    sKeyRows.clear();
    sKeyRows.push_back(KeyRow{L"NEUTRAL", {kNeutralColour[0], kNeutralColour[1], kNeutralColour[2]}});
    std::vector<const Gang*> gangs;
    for (const Turf& turf : sTurfs)
    {
        size_t g = 0;
        while (g < gangs.size() && gangs[g] != turf.mGang)
        {
            g++;
        }
        if (g == gangs.size())
        {
            KeyRow row = {};
            if (!TurfColour(turf.mGang, row.mRgb, &row.mName))
            {
                memcpy(row.mRgb, kNeutralColour, sizeof(row.mRgb));
            }
            gangs.push_back(turf.mGang);
            colours.push_back(row);
            if (row.mName)
            {
                sKeyRows.push_back(row);
            }
        }
        for (s32 y = static_cast<s32>(turf.mRect[1]); y < static_cast<s32>(turf.mRect[3]) && y < kMapBlocks; y++)
        {
            for (s32 x = static_cast<s32>(turf.mRect[0]); x < static_cast<s32>(turf.mRect[2]) && x < kMapBlocks; x++)
            {
                sTurf[y][x] = static_cast<s8>(g);
            }
        }
    }
    sKeyRows.push_back(KeyRow{L"WATER", {kWaterColour[0], kWaterColour[1], kWaterColour[2]}});
    sStyled.resize(sMap.size());
    for (s32 ty = 0; ty < kMapTexels; ty++)
    {
        for (s32 tx = 0; tx < kMapTexels; tx++)
        {
            const s32 bx = tx / kTexelsPerBlock, by = ty / kTexelsPerBlock;
            const u32 c = sMap[static_cast<size_t>(ty) * kMapTexels + tx];
            const f32 lum = (0.30f * (c & 0xFF) + 0.59f * ((c >> 8) & 0xFF) + 0.11f * ((c >> 16) & 0xFF)) / 255.0f;
            f32 rgb[3];
            if (sWater[by][bx])
            {
                for (s32 k = 0; k < 3; k++)
                {
                    rgb[k] = kWaterColour[k] * (0.94f + 0.12f * lum);
                }
            }
            else
            {
                const f32* base = sTurf[by][bx] >= 0 ? colours[static_cast<size_t>(sTurf[by][bx])].mRgb : kNeutralColour;
                // Dark in the colour for the low and dark, lighter toward white for the light: the printed maps' shades.
                f32 t = (lum - 0.08f) / 0.62f;
                t = t < 0 ? 0 : (t > 1 ? 1 : t);
                t = t * t * (3 - 2 * t);
                for (s32 k = 0; k < 3; k++)
                {
                    const f32 dark = base[k] * 0.28f, light = base[k] + (1.0f - base[k]) * 0.4f;
                    rgb[k] = dark + (light - dark) * t;
                }
            }
            sStyled[static_cast<size_t>(ty) * kMapTexels + tx] = Rgba(rgb[0], rgb[1], rgb[2], 1.0f);
        }
    }
    sStyledVersion++;
}

// The game's job arrow (user sprite 0, the big arrow Hud_Arrow::DrawArrow draws) from the player's style, recoloured in
// the waypoint's blue: its brightness kept, its hue turned.
std::vector<u32> sWaypointArrow;
s32 sWaypointArrowW, sWaypointArrowH;
u32 sWaypointArrowVersion;

void BuildWaypointArrow()
{
    sWaypointArrowW = sWaypointArrowH = 0;
    const u16 index = gGtx_0x106C->GetSpriteTrueIndex(sprite_types_enum::user_6, 0);
    const sprite_index* pSprite = gGtx_0x106C->get_sprite_index(index);
    if (!pSprite || !pSprite->mpData || !pSprite->mWidth || !pSprite->mHeight)
    {
        return;
    }
    const s16 palette = gGtx_0x106C->GetTruePalette(palette_types_enum::sprites_2, static_cast<s16>(index));
    const u8* pPalette = gGtx_0x106C->GetPalData(gGtx_0x106C->get_phys_pal(palette));
    sWaypointArrowW = pSprite->mWidth;
    sWaypointArrowH = pSprite->mHeight;
    sWaypointArrow.assign(static_cast<size_t>(sWaypointArrowW) * sWaypointArrowH, 0);
    for (s32 y = 0; y < sWaypointArrowH; y++)
    {
        for (s32 x = 0; x < sWaypointArrowW; x++)
        {
            const u8 c = pSprite->mpData[y * 256 + x];
            if (!c)
            {
                continue;
            }
            const u8* pColour = pPalette + c * 256;
            const f32 r = pColour[2] / 255.0f, g = pColour[1] / 255.0f, b = pColour[0] / 255.0f;
            const f32 bright = fmaxf(r, fmaxf(g, b)), low = fminf(r, fminf(g, b));
            // Grey (its rim, its shading) stays grey; the colour goes to the waypoint's.
            const f32 sat = bright > 0 ? (bright - low) / bright : 0.0f;
            const f32 to[3] = {0.25f, 0.85f, 1.0f};
            f32 out[3];
            for (s32 k = 0; k < 3; k++)
            {
                const f32 grey = (r + g + b) / 3;
                out[k] = fminf(grey * (1 - sat) + bright * to[k] * sat, 1.0f);
            }
            sWaypointArrow[static_cast<size_t>(y) * sWaypointArrowW + x] = Rgba(out[0], out[1], out[2], 1.0f);
        }
    }
    sWaypointArrowVersion++;
}

// The player's sprite (its car's in one) once the art, the map's picture and the pins are ready; NULL without a game.
const Sprite* Prepare()
{
    if (!gGame_0x40 || !gViewCamera || !gHud_2B00 || !gGtx_0x106C || !gMap_0x370 || !gMap_0x370->mpDmap || !gMap_0x370->mpDmap->mBlock)
    {
        return NULL;
    }
    Player* pPlayer = gGame_0x40->mLocalPlayer;
    Ped* pPed = pPlayer ? pPlayer->mPlayerPed : NULL;
    const Sprite* pSprite = pPed ? pPed->GetSprite() : NULL;
    if (!pSprite || !Load())
    {
        return NULL;
    }
    if (spBlocks != gMap_0x370->mpDmap->mBlock)
    {
        spBlocks = gMap_0x370->mpDmap->mBlock;
        BuildMap();
        sPinCountdown = 0;
        sWaypoint = false;
        memset(sGangColourRead, 0, sizeof(sGangColourRead));
        BuildWaypointArrow();
    }
    if (sWaypoint && fabsf(ToF(pSprite->mXy.x) - sWaypointX) < kWaypointReached && fabsf(ToF(pSprite->mXy.y) - sWaypointY) < kWaypointReached)
    {
        sWaypoint = false;
    }
    if (--sPinCountdown <= 0)
    {
        sPinCountdown = kPinRefreshFrames;
        sPins.clear();
        FindTurfs(); // the level's script gives the gangs their respect after the map loads
        FindZonePins();
        FindGarages();
        FindCranes();
        FindChurches(); // last, drawn over the others
    }
    return pSprite;
}

// The pause menu's MAP: its centre in blocks, the blocks across its height, and the mouse dragging it.
f32 sCityX, sCityY;
f32 sCityAcross;
bool sCityDragging;
f32 sCityMouseX, sCityMouseY;
f32 sCityDragTravel; // screen pixels the mouse moved since its button went down: a click if few
f32 sCityNear = 4.0f; // blocks: a click or Enter this close to the waypoint takes it away, further moves it
Uint64 sCityTicks;
const f32 kCityMinAcross = 20.0f;
const f32 kCityMaxAcross = kMapBlocks * 1.05f;
const f32 kCityZoomStep = 1.25f;

// Zoomed by steps (in for more than 0), keeping the place under (ax, ay) where it is on screen.
void ZoomCity(f32 steps, f32 ax, f32 ay, f32 centreX, f32 centreY, f32 height)
{
    const f32 ppb = height / sCityAcross;
    const f32 wx = sCityX + (ax - centreX) / ppb, wy = sCityY + (ay - centreY) / ppb;
    sCityAcross = fminf(fmaxf(sCityAcross * powf(kCityZoomStep, -steps), kCityMinAcross), kCityMaxAcross);
    const f32 ppb2 = height / sCityAcross;
    sCityX = wx - (ax - centreX) / ppb2;
    sCityY = wy - (ay - centreY) / ppb2;
}

enum LegendLook
{
    kPlainLook,
    kTargetLook,
    kLockedLook,
    kWaypointLook,
};
struct LegendEntry
{
    Slot mSlot;
    const wchar_t* mName;
    LegendLook mLook;
};
const LegendEntry kLegend[] = {
    {kArrowSlot, L"YOU", kPlainLook},
    {kGreyBlipSlot, L"WAYPOINT", kWaypointLook},
    {kGreyBlipSlot, L"JOB TARGET", kTargetLook},
    {kPhoneGreenSlot, L"EASY JOB", kPlainLook},
    {kPhoneYellowSlot, L"MEDIUM JOB", kPlainLook},
    {kPhoneRedSlot, L"HARD JOB", kPlainLook},
    {kPhoneRedSlot, L"NEEDS MORE RESPECT", kLockedLook},
    {kInfoSlot, L"GANG INFORMATION", kPlainLook},
    {kSaveSlot, L"CHURCH - SAVE", kPlainLook},
    {kGarageSlot, L"GARAGE", kPlainLook},
    {kCraneSlot, L"CRANE", kPlainLook},
    {kRestartSlot, L"HOSPITAL", kPlainLook},
    {kArrestSlot, L"POLICE STATION", kPlainLook},
    {kStationSlot, L"TRAIN STATION", kPlainLook},
};
const s32 kNumLegend = static_cast<s32>(sizeof(kLegend) / sizeof(kLegend[0]));
} // namespace
#endif

void Draw()
{
#if defined(GTA2_PORT_SDL)
    const Sprite* pSprite = PortSettings::Minimap() ? Prepare() : NULL;
    if (!pSprite)
    {
        return;
    }

    View v;
    v.mUi = ToF(gViewCamera->mUiScale);
    v.mScale = v.mUi >= 1.5f ? 1 : 0;
    // At the screen's bottom left, kept at its distance from that corner of the safe area.
    v.mHole[0] = ToF(UI_X(Fix16(0))) + (kMargin + kBorder) * v.mUi;
    v.mHole[3] = ToF(UI_Y(Fix16(480))) - (kMargin + kBorder) * v.mUi;
    v.mHole[2] = v.mHole[0] + kMapSize * v.mUi;
    v.mHole[1] = v.mHole[3] - kMapSize * v.mUi;
    v.mCentreX = (v.mHole[0] + v.mHole[2]) / 2;
    v.mCentreY = (v.mHole[1] + v.mHole[3]) / 2;
    v.mPlayerX = ToF(pSprite->mXy.x);
    v.mPlayerY = ToF(pSprite->mXy.y);
    const f32 across = fminf(fmaxf(kMinBlocksAcross + kBlocksPerCameraHeight * ToF(gViewCamera->mCurPos.mZ), kMinBlocksAcross),
                             kMaxBlocksAcross);
    v.mPixelsPerBlock = (v.mHole[2] - v.mHole[0]) / across;
    // A brief's face and text, centred with the layout, reach this corner on narrower screens: faded while one shows there.
    f32 briefLeft;
    {
        PortUiCentred centred;
        briefLeft = ToF(UI_X(Fix16(0)));
    }
    v.mAlpha = gHud_2B00->mBrief.mCurrBriefs && v.mHole[2] + (kBorder + 4.0f) * v.mUi > briefLeft ? 0.3f : 1.0f;

    // The border: a thin dark line round the map.
    const f32 b = kBorder * v.mUi;
    DrawRect(v.mHole[0] - b, v.mHole[1] - b, v.mHole[2] + b, v.mHole[3] + b, Rgba(0.02f, 0.02f, 0.02f, 0.9f * v.mAlpha));
    port_ClipMinimap(v.mHole);
    DrawMap(v);
    DrawPins(v);
    DrawTargets(v, ToF(pSprite->mZpos));
    if (sWaypoint)
    {
        f32 x, y, out;
        ToScreen(v, sWaypointX, sWaypointY, 4.0f * v.mUi, 1e9f, &x, &y, &out);
        DrawWaypointMark(x, y, v.mUi, v.mAlpha);
    }
    DrawPlayer(v, pSprite, v.mCentreX, v.mCentreY);
    port_ClipMinimap(NULL);
    DrawTurfs(v);
    DrawIcon(v, kNorthSlot, v.mCentreX, v.mHole[1] - b / 2, Rgba(1, 1, 1, v.mAlpha), true);
#endif
}

void DrawWaypointArrow()
{
#if defined(GTA2_PORT_SDL)
    // With the GUIDE ARROWS, as the game's arrows go round the player toward their targets (Hud_Arrow::UpdateScreenPos):
    // at a little distance from the player in the waypoint's direction, or just before it when it is that close.
    if (!PortSettings::GuideArrows() || IsNetworkGame())
    {
        return;
    }
    const Sprite* pSprite = Prepare();
    Player* pPlayer = gGame_0x40 ? gGame_0x40->mLocalPlayer : NULL;
    if (!pSprite || !sWaypoint || !pPlayer)
    {
        return;
    }
    Fix16 px, py, pz;
    pPlayer->get_pos(&px, &py, &pz);
    const f32 dx = sWaypointX - ToF(px), dy = sWaypointY - ToF(py);
    const f32 distance = sqrtf(dx * dx + dy * dy);
    if (distance < 0.01f)
    {
        return;
    }
    if (!sWaypointArrowW)
    {
        return;
    }
    const f32 radius = fminf(pPlayer->GetPlayerCar() ? 1.6f : 1.2f, fmaxf(distance - 0.25f, 0.0f));
    Camera* pCam = pPlayer->get_camera();
    Fix16 sx, sy;
    pCam->ProjectWorldToScreen_4B90E0(Fix16(ToF(px) + dx / distance * radius), Fix16(ToF(py) + dy / distance * radius), pz, &sx, &sy);
    // Turned as the game turns its arrows: the angle of the player less the target (Hud_Arrow::UpdateScreenPos), the quad's
    // corners as DrawFigure turns them (Fix16_Point::RotateByAngle), at the HUD's scale.
    Fix16_Point displacement;
    displacement.SetXY(Fix16(-dx), Fix16(-dy));
    const Ang16 rotation = displacement.GetAngle();
    const f32 angle = rotation.rValue * (2.0f * 3.14159265f / 1440.0f);
    const f32 sn = sinf(angle), cs = cosf(angle);
    const f32 scale = ToF(pCam->mUiScale);
    const f32 hw = sWaypointArrowW * scale / 2, hh = sWaypointArrowH * scale / 2;
    const f32 corners[4][4] = {{-hw, -hh, 0, 0}, {hw, -hh, 1, 0}, {hw, hh, 1, 1}, {-hw, hh, 0, 1}};
    f32 pts[4][4];
    for (s32 k = 0; k < 4; k++)
    {
        const f32 x = corners[k][0], y = corners[k][1];
        pts[k][0] = ToF(sx) + x * cs + y * sn;
        pts[k][1] = ToF(sy) - x * sn + y * cs;
        pts[k][2] = corners[k][2];
        pts[k][3] = corners[k][3];
    }
    const s32 order[6] = {0, 1, 2, 0, 2, 3};
    f32 xyuv[24];
    u32 colours[6];
    for (s32 k = 0; k < 6; k++)
    {
        memcpy(&xyuv[k * 4], pts[order[k]], sizeof(pts[0]));
        colours[k] = Rgba(1, 1, 1, 1);
    }
    port_DrawMinimap(kWaypointArrowSlot, sWaypointArrowW, sWaypointArrowH, sWaypointArrow.data(), sWaypointArrowVersion, xyuv, colours, 6, false);
#endif
}

void OpenCityMap()
{
#if defined(GTA2_PORT_SDL)
    const Sprite* pSprite = Prepare();
    // The whole city, as the printed maps show it.
    sCityX = kMapBlocks / 2.0f;
    sCityY = kMapBlocks / 2.0f;
    sCityAcross = kCityMaxAcross;
    sCityDragging = false;
    if (pSprite)
    {
        BuildStyled();
    }
    sCityTicks = 0;
    int left, right, wheel;
    float x, y;
    port_TakeMouse(&x, &y, &left, &right, &wheel);
#endif
}

void CentreCityMap()
{
#if defined(GTA2_PORT_SDL)
    if (const Sprite* pSprite = Prepare())
    {
        sCityX = ToF(pSprite->mXy.x);
        sCityY = ToF(pSprite->mXy.y);
    }
#endif
}

void ToggleCityWaypoint()
{
#if defined(GTA2_PORT_SDL)
    if (sCityAcross > 0)
    {
        ToggleWaypoint(sCityX, sCityY, sCityNear);
    }
#endif
}

s32 CityMapLabels()
{
#if defined(GTA2_PORT_SDL)
    return sNumLabels;
#else
    return 0;
#endif
}

const wchar_t* CityMapLabel(s32 i, f32* pX, f32* pY)
{
#if defined(GTA2_PORT_SDL)
    if (i < 0 || i >= sNumLabels)
    {
        return NULL;
    }
    *pX = sLabels[i].mX;
    *pY = sLabels[i].mY;
    return sLabels[i].mText;
#else
    (void)i;
    (void)pX;
    (void)pY;
    return NULL;
#endif
}

void ZoomCityMap(s32 steps)
{
#if defined(GTA2_PORT_SDL)
    // About the view's centre: its size does not matter.
    ZoomCity(static_cast<f32>(steps), 0, 0, 0, 0, 1);
#else
    (void)steps;
#endif
}

bool DrawCityMap(f32 left, f32 top, f32 right, f32 bottom, f32 ui)
{
#if defined(GTA2_PORT_SDL)
    const Sprite* pSprite = Prepare();
    if (!pSprite || sCityAcross <= 0)
    {
        return false;
    }
    const f32 width = right - left, height = bottom - top;
    const f32 centreX = (left + right) / 2, centreY = (top + bottom) / 2;

    // Moved by the arrows or WASD while held, the left stick or the d-pad, dragged by the mouse; zoomed by its wheel, under
    // the pointer, and the right stick.
    const Uint64 now = SDL_GetTicks();
    const f32 dt = sCityTicks ? fminf((now - sCityTicks) / 1000.0f, 0.1f) : 0.0f;
    sCityTicks = now;
    f32 moveX, moveY, aimX, aimY;
    port_TwinStickInput(&moveX, &moveY, &aimX, &aimY);
    moveX += (port_DikHeld(DIK_RIGHT) || port_DikHeld(DIK_D)) - (port_DikHeld(DIK_LEFT) || port_DikHeld(DIK_A));
    moveY += (port_DikHeld(DIK_DOWN) || port_DikHeld(DIK_S)) - (port_DikHeld(DIK_UP) || port_DikHeld(DIK_W));
    moveX = fminf(fmaxf(moveX, -1.0f), 1.0f);
    moveY = fminf(fmaxf(moveY, -1.0f), 1.0f);
    sCityX += moveX * sCityAcross * 0.75f * dt;
    sCityY += moveY * sCityAcross * 0.75f * dt;
    if (fabsf(aimY) > 0.2f)
    {
        ZoomCity(-aimY * 4.0f * dt, centreX, centreY, centreX, centreY, height);
    }
    float mouseX, mouseY;
    int leftClicks, rightClicks, wheel;
    port_TakeMouse(&mouseX, &mouseY, &leftClicks, &rightClicks, &wheel);
    const bool inside = mouseX >= left && mouseX < right && mouseY >= top && mouseY < bottom;
    if (wheel && inside)
    {
        ZoomCity(static_cast<f32>(wheel), mouseX, mouseY, centreX, centreY, height);
    }
    if (port_MouseLeftDown() && (sCityDragging || inside))
    {
        if (sCityDragging)
        {
            sCityX -= (mouseX - sCityMouseX) * sCityAcross / height;
            sCityY -= (mouseY - sCityMouseY) * sCityAcross / height;
            sCityDragTravel += fabsf(mouseX - sCityMouseX) + fabsf(mouseY - sCityMouseY);
        }
        else
        {
            sCityDragTravel = 0;
        }
        sCityDragging = true;
        sCityMouseX = mouseX;
        sCityMouseY = mouseY;
    }
    else
    {
        // A click, not a drag: the waypoint there, or none if it was on it. A tap of a touchpad can press and release
        // between two frames, never seen down: its press counted (leftClicks) at the pointer.
        const bool click = sCityDragging ? sCityDragTravel < 6.0f * ui : leftClicks > 0 && inside;
        const f32 clickX = sCityDragging ? sCityMouseX : mouseX, clickY = sCityDragging ? sCityMouseY : mouseY;
        if (click)
        {
            ToggleWaypoint(sCityX + (clickX - centreX) * sCityAcross / height, sCityY + (clickY - centreY) * sCityAcross / height, sCityNear);
        }
        sCityDragging = false;
    }
    sCityX = fminf(fmaxf(sCityX, 0.0f), static_cast<f32>(kMapBlocks));
    sCityY = fminf(fmaxf(sCityY, 0.0f), static_cast<f32>(kMapBlocks));
    sCityNear = 7.0f * ui * sCityAcross / height; // about the waypoint mark's size

    View v;
    v.mUi = ui;
    v.mScale = ui >= 1.5f ? 1 : 0;
    v.mHole[0] = left;
    v.mHole[1] = top;
    v.mHole[2] = right;
    v.mHole[3] = bottom;
    v.mCentreX = centreX;
    v.mCentreY = centreY;
    v.mPlayerX = sCityX;
    v.mPlayerY = sCityY;
    v.mPixelsPerBlock = height / sCityAcross;
    v.mAlpha = 1.0f;
    (void)width;
    port_ClipMinimap(v.mHole);
    DrawMap(v, true);
    // The districts' names (navigation zones) where the zone is wide enough on screen for one, away from the others.
    sNumLabels = 0;
    if (gMap_0x370->mpZones)
    {
        for (u16 i = 0; i < gMap_0x370->mpZones->mNumZones && sNumLabels < kMaxLabels; i++)
        {
            gmp_map_zone* pZone = gMap_0x370->GetZone(i);
            if (!pZone || pZone->mZoneType != Navigation_1 || pZone->mW * v.mPixelsPerBlock < 60.0f * ui / 1.5f)
            {
                continue;
            }
            f32 x, y, out;
            // Not under the page's title, legend or hints near the edges.
            if (!ToScreen(v, pZone->mX + pZone->mW / 2.0f, pZone->mY + pZone->mH / 2.0f, 0.0f, 1.0f, &x, &y, &out) ||
                fabsf(y - centreY) > (bottom - top) * 0.36f || fabsf(x - centreX) > (right - left) * 0.42f)
            {
                continue;
            }
            bool crowded = false;
            for (s32 k = 0; k < sNumLabels && !crowded; k++)
            {
                crowded = fabsf(sLabels[k].mX - x) < 90.0f * ui / 1.5f && fabsf(sLabels[k].mY - y) < 14.0f * ui / 1.5f;
            }
            wchar_t* pName = crowded ? NULL : pZone->get_zone_str();
            if (pName)
            {
                Label& label = sLabels[sNumLabels++];
                wcsncpy(label.mText, gText_0x14->StrToUpper(pName), 47);
                label.mText[47] = 0;
                label.mX = x;
                label.mY = y;
            }
        }
    }
    for (const Pin& pin : sPins)
    {
        f32 x, y, out;
        if (ToScreen(v, pin.mX, pin.mY, 0.0f, 1.0f, &x, &y, &out))
        {
            DrawIcon(v, pin.mSlot, x, y, Rgba(1, 1, 1, 1));
        }
    }
    DrawTargets(v, ToF(pSprite->mZpos), true);
    if (sWaypoint)
    {
        f32 x, y, out;
        if (ToScreen(v, sWaypointX, sWaypointY, 0.0f, 1.0f, &x, &y, &out))
        {
            DrawWaypointMark(x, y, ui, 1.0f);
        }
    }
    // The cross at the middle, where Enter puts the waypoint.
    const f32 arm = 6.0f * ui, thin = 0.5f * ui;
    DrawRect(centreX - arm, centreY - thin, centreX + arm, centreY + thin, Rgba(1, 1, 1, 0.7f));
    DrawRect(centreX - thin, centreY - arm, centreX + thin, centreY + arm, Rgba(1, 1, 1, 0.7f));
    f32 px, py, out;
    if (ToScreen(v, ToF(pSprite->mXy.x), ToF(pSprite->mXy.y), 6.0f * ui, 1e9f, &px, &py, &out))
    {
        View big = v;
        big.mUi = ui * 1.5f;
        big.mScale = 1;
        DrawPlayer(big, pSprite, px, py);
    }
    port_ClipMinimap(NULL);
    return rightClicks > 0;
#else
    (void)left;
    (void)top;
    (void)right;
    (void)bottom;
    (void)ui;
    return false;
#endif
}

const wchar_t* LegendName(s32 i)
{
#if defined(GTA2_PORT_SDL)
    if (i >= 0 && i < kNumLegend)
    {
        return kLegend[i].mName;
    }
    i -= kNumLegend;
    return i >= 0 && i < static_cast<s32>(sKeyRows.size()) ? sKeyRows[static_cast<size_t>(i)].mName : NULL;
#else
    (void)i;
    return NULL;
#endif
}

void DrawLegendIcon(s32 i, f32 x, f32 y, f32 ui)
{
#if defined(GTA2_PORT_SDL)
    if (!LegendName(i) || !Load())
    {
        return;
    }
    if (i >= kNumLegend)
    {
        // A territory's swatch: its colour from dark to light, as the printed keys show it.
        const f32* rgb = sKeyRows[static_cast<size_t>(i - kNumLegend)].mRgb;
        const f32 w = 6.0f * ui, h = 4.0f * ui;
        DrawRect(x - w - 0.5f * ui, y - h - 0.5f * ui, x + w + 0.5f * ui, y + h + 0.5f * ui, Rgba(0, 0, 0, 1));
        for (s32 k = 0; k < 3; k++)
        {
            const f32 shade = k == 0 ? 1.15f : (k == 1 ? 0.8f : 0.45f);
            DrawRect(x - w + k * 2 * w / 3, y - h, x - w + (k + 1) * 2 * w / 3, y + h,
                     Rgba(fminf(rgb[0] * shade, 1.0f), fminf(rgb[1] * shade, 1.0f), fminf(rgb[2] * shade, 1.0f), 1));
        }
        return;
    }
    View v = {};
    v.mUi = ui;
    v.mScale = ui >= 1.5f ? 1 : 0;
    v.mAlpha = 1.0f;
    const LegendEntry& entry = kLegend[i];
    switch (entry.mLook)
    {
        case kTargetLook:
            DrawIcon(v, entry.mSlot, x, y, TargetColour(1.0f), false, kTargetSize);
            break;
        case kLockedLook:
            DrawIcon(v, entry.mSlot, x, y, Rgba(kLockedShade, kLockedShade, kLockedShade, 1.0f));
            break;
        case kWaypointLook:
            DrawWaypointMark(x, y, ui, 1.0f);
            break;
        default:
            DrawIcon(v, entry.mSlot, x, y, Rgba(1, 1, 1, 1));
            break;
    }
#else
    (void)i;
    (void)x;
    (void)y;
    (void)ui;
#endif
}
void DrawShade(f32 left, f32 top, f32 right, f32 bottom, f32 alpha)
{
#if defined(GTA2_PORT_SDL)
    DrawRect(left, top, right, bottom, Rgba(0.0f, 0.0f, 0.0f, alpha));
#else
    (void)left;
    (void)top;
    (void)right;
    (void)bottom;
    (void)alpha;
#endif
}
} // namespace PortMinimap
