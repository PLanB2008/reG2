// AUTOSAVE (settings.hpp): the city's autosave (saves.hpp) once a job or frenzy is over and the player is safe,
// and on the pause menu's SAVE AND QUIT; SAVING at the top of the screen after it.
#pragma once

namespace PortAutosave
{
// A level starts.
void Start();
// After each logic step.
void AfterStep();
// SAVE AND QUIT, as the pause menu opens: NULL if the player can be saved now, else why not (English, to translate).
const wchar_t* QuitSaveBlocked();
// SAVE AND QUIT chosen: false if the save failed.
bool SaveToQuit();
// FINISH_LEVEL, the player in the area's exit arrow: saved a few steps back, so that loading does not end the area.
void AreaFinished();
// With the HUD.
void Draw();
} // namespace PortAutosave
