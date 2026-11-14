// AUTOSAVE (settings.hpp): the city's autosave (saves.hpp) once a job or frenzy is over and the player is safe,
// and on quitting after one or after two minutes of play; SAVING at the top of the screen after it.
#pragma once

namespace PortAutosave
{
// A level starts.
void Start();
// After each logic step.
void AfterStep();
// QUIT GAME in the pause menu, before the game ends, or the window closing (its close button, Alt+F4, the system
// ending the program), before the game is destroyed; nothing outside a game.
void Quit(const char* pWhy);
// FINISH_LEVEL, the player in the area's exit arrow: saved a few steps back, so that loading does not end the area.
void AreaFinished();
// With the HUD.
void Draw();
} // namespace PortAutosave
