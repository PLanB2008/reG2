// What follows from a network game player's look (the remap a map script's PLAYER_PED gives their ped): the colour of
// their HUD digits (Player::ColorScoreFromRemap) and of the arrow pointing at them (Hud_Arrow::SetPlayerArrowColour).
// The original's looks keep the original's values; the others give more players colours of their own.
#pragma once
#include "../../Source/types.hpp"

struct PortPlayerLook
{
    s16 mHudPalette; // the user remap of the score and frag digits; 0: the digits keep their colour
    s16 mArrowSprite; // the user sprite of the arrow; 0: the arrow keeps its sprite
    s16 mArrowRemap; // the user remap the arrow is drawn with; 0: its sprite's own colours
};

// NULL: a look the HUD gives nothing
inline const PortPlayerLook* PortLookOf(s32 remap)
{
    // By look (ped_remap_enum). Unused by the original: the user remaps 1, 9 and 11 (0 and 14 hide digits, 10, 12, 13 repeat others)
    static const PortPlayerLook kLooks[] = {
        {1, 1, 13}, // 0 blue police: teal, mustard
        {0, 0, 0},
        {0, 0, 0},
        {0, 0, 0},
        {0, 0, 0},
        {4, 4, 0}, // 5 red-head redneck
        {4, 4, 0}, // 6 blond redneck, as the red-head
        {8, 5, 0}, // 7 yellow scientist
        {7, 3, 0}, // 8 Zaibatsu
        {5, 6, 0}, // 9 Krishna
        {6, 7, 0}, // 10 Russian
        {3, 1, 0}, // 11 Loonie
        {9, 6, 9}, // 12 Elvis: cyan, cyan
        {2, 2, 0}, // 13 Yakuza
        {11, 2, 1}, // 14 fireman: pink, rust
    };
    const PortPlayerLook* look = remap >= 0 && remap < (s32)(sizeof(kLooks) / sizeof(kLooks[0])) ? &kLooks[remap] : NULL;
    return look && (look->mHudPalette || look->mArrowSprite) ? look : NULL;
}
