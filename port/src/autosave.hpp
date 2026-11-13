// AUTOSAVE (settings.hpp): the city's autosave (saves.hpp) once a job or a kill frenzy is over and the
// player is safe, and on quitting off a job; a SAVING line in a corner after it.
#pragma once

namespace PortAutosave
{
// A level starts.
void Start();
// After each logic step.
void AfterStep();
// QUIT GAME in the pause menu, before the game ends.
void Quit();
// With the HUD.
void Draw();
} // namespace PortAutosave
