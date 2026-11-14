// Pictures of the keys and gamepad buttons as printed on the player's pad (greatdocbrown's Gamepad UI), for the
// menus' hints. Sizes and positions in the frame the game draws.
#pragma once

#include "types.hpp"

namespace PortPrompts
{
enum Glyph
{
    kNoGlyph = -1,
    // As the pad binding codes (compat/src/pad.cpp): the face buttons by position, the shoulders, Back, Start, the
    // sticks' presses and the triggers.
    kPadSouth,
    kPadEast,
    kPadWest,
    kPadNorth,
    kPadLeftShoulder,
    kPadRightShoulder,
    kPadBack,
    kPadStart,
    kPadLeftStick,
    kPadRightStick,
    kPadLeftTrigger,
    kPadRightTrigger,
    // The d-pad's left, past the binding codes.
    kPadDpadLeft,
    kKeyEnter,
    kKeyEscape,
    kKeyDelete,
    kKeyLeft,
    kKeyRight,
    kKeyBackspace,
    kKeyA, // to kKeyA + 25 for Z
};

// False without the pictures (the DirectX build): the hints are then texts.
bool Available();
// A letter's key, kNoGlyph for anything else.
Glyph Letter(s32 letter);
// The height of a picture for about that height: a whole multiple of its 16 pixels on the window.
f32 Size(f32 height);
f32 Width(Glyph glyph, f32 size);
// At x, its middle at y, at a size from Size; a white picture takes the text's colour (0xRRGGBB), a coloured one
// is greyed when dim.
void Draw(Glyph glyph, f32 x, f32 y, f32 size, s32 colour, bool dim);
} // namespace PortPrompts
