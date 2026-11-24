#include "minimap.hpp"
#include "Camera.hpp"
#include "Car.hpp"
#include "Draw.hpp"
#include "Game.hpp"
#include "Gang.hpp"
#include "Hud.hpp"
#include "ObjectPool.hpp"
#include "Ped.hpp"
#include "Player.hpp"
#include "ScriptManager.hpp"
#include "ScriptThread.hpp"
#include "block_face.hpp"
#include "enums.hpp"
#include "gtx_0x106C.hpp"
#include "map_0x370.hpp"
#include "settings.hpp"
#include "sprite.hpp"
#include <math.h>
#include <string.h>
#include <vector>
#if defined(GTA2_PORT_SDL)
#include <SDL3/SDL.h>

void port_DrawMinimap(s32 slot, s32 width, s32 height, const u32* pRgba, u32 version, const f32* pXyuv, const u32* pColours, s32 count,
                      bool nearest);
void port_ClipMinimap(const f32* pRect);
#endif

namespace PortMinimap
{
#if defined(GTA2_PORT_SDL)
namespace
{
// The markers and pins, original art from port/docs/minimap/assets (tools/make_all.py), at 1x and 2x.
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

// Texture slots of port_DrawMinimap.
enum Slot
{
    kMapSlot,
    kNorthSlot,
    kArrowSlot,
    kBlipSlot,
    kGreyBlipSlot, // the blip without its red, tinted per phone
    kSaveSlot,
    kGarageSlot,
};
const s32 kNumSlots = kGarageSlot + 1;

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

// A target this far above or below the player (blocks) shows as a triangle pointing that way.
const f32 kLevelTolerance = 0.5f;

// The map: 256x256 blocks at 4 texels each, a texel the average colour of 16x16 texels of the block's lid.
const s32 kMapBlocks = 256;
const s32 kTexelsPerBlock = 4;
const s32 kMapTexels = kMapBlocks * kTexelsPerBlock;
const s32 kLevels = 8;

std::vector<u32> sMap;
u32 sMapVersion;
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

// The garages: car shop bays (car_shop_130 objects), one pin for bays within 4 blocks of each other.
void FindGarages()
{
    const size_t first = sPins.size();
    for (Object* pObj = gObject_2C_Pool ? gObject_2C_Pool->GetFirstUsed() : NULL; pObj; pObj = pObj->mpNext)
    {
        if (pObj->mModel != objects::car_shop_130 || !pObj->mSprite)
        {
            continue;
        }
        const f32 x = ToF(pObj->mSprite->mXy.x);
        const f32 y = ToF(pObj->mSprite->mXy.y);
        bool near = false;
        for (size_t i = first; i < sPins.size() && !near; i++)
        {
            near = fabsf(sPins[i].mX - x) < 4.0f && fabsf(sPins[i].mY - y) < 4.0f;
        }
        if (!near)
        {
            sPins.push_back(Pin{x, y, kGarageSlot});
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

// A picture centred at (x, y), at its size on the HUD.
void DrawIcon(const View& v, Slot slot, f32 x, f32 y, u32 rgba, bool nearest = false)
{
    const Picture& p = sPictures[slot][v.mScale];
    const f32 k = slot == kNorthSlot ? v.mUi : v.mUi / (v.mScale + 1);
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

void DrawMap(const View& v)
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
    port_DrawMinimap(kMapSlot, kMapTexels, kMapTexels, sMap.data(), sMapVersion, xyuv, colours, 6, false);
}

void DrawPins(const View& v)
{
    for (const Pin& pin : sPins)
    {
        f32 x, y, out;
        if (ToScreen(v, pin.mX, pin.mY, 0.0f, 1.3f, &x, &y, &out) && out <= 1.0f)
        {
            DrawIcon(v, pin.mSlot, x, y, Rgba(1, 1, 1, v.mAlpha));
        }
    }
}

// The phones' colours by the arrow's (Hud.cpp get_phone_colour): 1 yellow, 2 red, 3 green.
u32 PhoneColour(s32 arrowColour, f32 alpha)
{
    switch (arrowColour)
    {
        case 1:
            return Rgba(0.95f, 0.85f, 0.0f, alpha);
        case 2:
            return Rgba(0.9f, 0.05f, 0.0f, alpha);
        default:
            return Rgba(0.1f, 0.85f, 0.1f, alpha);
    }
}

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
void DrawTargets(const View& v, f32 playerZ)
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
                // A phone: off a job, where its arrow shows or within twice the map's size.
                if (onMission || !ToScreen(v, wx, wy, 3.0f * v.mUi, 2.0f, &x, &y, &out) || (out > 1.0f && !arrow.IsVisible()))
                {
                    continue;
                }
                if (t.mArrowColour >= 1 && t.mArrowColour <= 3)
                {
                    DrawIcon(v, kGreyBlipSlot, x, y, PhoneColour(t.mArrowColour, EdgeAlpha(out) * v.mAlpha));
                }
                else
                {
                    DrawGangIcon(v, t.mArrowSprtIdx, x, y, 9.0f);
                }
                continue;
            }
            // A job's target, at the edge when far.
            ToScreen(v, wx, wy, 3.0f * v.mUi, 1e9f, &x, &y, &out);
            const f32 alpha = EdgeAlpha(out) * v.mAlpha;
            const f32 dz = wz - playerZ;
            if (fabsf(dz) <= kLevelTolerance)
            {
                DrawIcon(v, kBlipSlot, x, y, Rgba(1, 1, 1, alpha));
            }
            else
            {
                const f32 size = 3.5f * v.mUi;
                DrawTriangle(x, y, size * 1.4f, dz > 0, Rgba(0, 0, 0, alpha));
                DrawTriangle(x, y, size, dz > 0, Rgba(0.95f, 0.15f, 0.1f, alpha));
            }
        }
    }
}

// The player's arrow at the centre, turned as the game turns sprites (Fix16_Point::RotateByAngle, 1440 a turn).
void DrawPlayer(const View& v, const Sprite* pSprite)
{
    const Picture& p = sPictures[kArrowSlot][v.mScale];
    const f32 k = v.mUi / (v.mScale + 1);
    const f32 hw = p.mWidth * k / 2, hh = p.mHeight * k / 2;
    const f32 angle = pSprite->mRotation.rValue * (2.0f * 3.14159265f / 1440.0f);
    const f32 s = sinf(angle), c = cosf(angle);
    const f32 corners[4][4] = {{-hw, -hh, 0, 0}, {hw, -hh, 1, 0}, {hw, hh, 1, 1}, {-hw, hh, 0, 1}};
    f32 pts[4][4];
    for (s32 i = 0; i < 4; i++)
    {
        const f32 x = corners[i][0], y = corners[i][1];
        pts[i][0] = v.mCentreX + x * c + y * s;
        pts[i][1] = v.mCentreY - x * s + y * c;
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

// Each gang's icon on the border toward the nearest part of its territory, none while the player is in it.
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
        if (best < 0.01f)
        {
            continue;
        }
        // On the border, the direction stretched to it.
        const f32 dx = bestX - v.mPlayerX, dy = bestY - v.mPlayerY;
        const f32 halfW = (v.mHole[2] - v.mHole[0]) / 2, halfH = (v.mHole[3] - v.mHole[1]) / 2;
        const f32 k = 1.0f / fmaxf(fabsf(dx) / halfW, fabsf(dy) / halfH);
        f32 x = v.mCentreX + dx * k;
        f32 y = v.mCentreY + dy * k;
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
} // namespace
#endif

void Draw()
{
#if defined(GTA2_PORT_SDL)
    if (!PortSettings::Minimap() || !gGame_0x40 || !gViewCamera || !gHud_2B00 || !gGtx_0x106C || !gMap_0x370 || !gMap_0x370->mpDmap ||
        !gMap_0x370->mpDmap->mBlock)
    {
        return;
    }
    Player* pPlayer = gGame_0x40->mLocalPlayer;
    Ped* pPed = pPlayer ? pPlayer->mPlayerPed : NULL;
    const Sprite* pSprite = pPed ? pPed->GetSprite() : NULL;
    if (!pSprite || !Load())
    {
        return;
    }
    if (spBlocks != gMap_0x370->mpDmap->mBlock)
    {
        spBlocks = gMap_0x370->mpDmap->mBlock;
        BuildMap();
        sPinCountdown = 0;
    }
    if (--sPinCountdown <= 0)
    {
        sPinCountdown = kPinRefreshFrames;
        sPins.clear();
        FindTurfs(); // the level's script gives the gangs their respect after the map loads
        FindChurches();
        FindGarages();
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
    DrawPlayer(v, pSprite);
    port_ClipMinimap(NULL);
    DrawTurfs(v);
    DrawIcon(v, kNorthSlot, v.mCentreX, v.mHole[1] - b / 2, Rgba(1, 1, 1, v.mAlpha), true);
#endif
}
} // namespace PortMinimap
