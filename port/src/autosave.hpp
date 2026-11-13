// AUTOSAVE (settings.hpp): the city's autosave (saves.hpp) once a job is over and the player is safe, and
// on quitting off a job.
#pragma once

namespace PortAutosave
{
// A level starts.
void Start();
// After each logic step.
void AfterStep();
// QUIT GAME in the pause menu, before the game ends.
void Quit();
} // namespace PortAutosave
