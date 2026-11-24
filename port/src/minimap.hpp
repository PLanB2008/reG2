// MINIMAP: a small map of the streets around the player in the HUD's bottom left corner, built from the level's
// blocks and lid tiles when it loads, so any level has one (port/docs/minimap). On it the church, the garages, the
// gangs' phones while off a job, the job's targets above or below the player, and each gang's icon on the border
// toward its territory.
#pragma once

#include "types.hpp"

namespace PortMinimap
{
// With the HUD, under the briefs and the pause menu: builds the map's picture for a new map, then draws it.
void Draw();
} // namespace PortMinimap
