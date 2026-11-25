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
// With GUIDE ARROWS (or without the MINIMAP), an arrow by the player toward the MAP's waypoint, as the game's own arrows
// point to their targets.
void DrawWaypointArrow();

// The pause menu's MAP: the whole city, opened on the player. Moved while the arrows, WASD, the left stick or the d-pad
// are held or the mouse drags it, zoomed by the wheel and the right stick; ZoomCityMap by steps (in above 0), CentreCityMap
// back on the player; a click (not a drag) sets or clears the waypoint. DrawCityMap draws it in that screen rectangle with icons at the HUD scale ui; true when the right
// button asks to go back.
void OpenCityMap();
void CentreCityMap();
void ZoomCityMap(s32 steps);
// A waypoint at the MAP's middle, or none if one is there; it shows on both maps until the player reaches it.
void ToggleCityWaypoint();
// The districts' names DrawCityMap placed, centred on (x, y): NULL past the last.
s32 CityMapLabels();
const wchar_t* CityMapLabel(s32 i, f32* pX, f32* pY);
bool DrawCityMap(f32 left, f32 top, f32 right, f32 bottom, f32 ui);
// The MAP's legend: entry i's name in English, NULL past the last, and its icon centred at (x, y).
const wchar_t* LegendName(s32 i);
void DrawLegendIcon(s32 i, f32 x, f32 y, f32 ui);
// A dark see-through panel over the map, for the page's texts.
void DrawShade(f32 left, f32 top, f32 right, f32 bottom, f32 alpha);
} // namespace PortMinimap
