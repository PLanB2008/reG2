// RETRY JOBS (settings.hpp): the retry checkpoint (saves.hpp) written as a job or a kill frenzy starts, deleted
// once it is passed; after a failure, a line on the HUD says how to go back to it from the pause menu.
#pragma once

#include "saves.hpp"

namespace PortRetry
{
// A level starts: a new game in a city, or loading another save, deletes the checkpoint.
void Start();
// ScriptThread, as a job is launched or a kill frenzy starts, before either marks anything as started.
void BeforeStart(PortSaves::Checkpoint kind);
// After each logic step.
void AfterStep();
// With the HUD.
void Draw();
// The pause menu's RETRY: right after a failure it comes selected.
bool JustFailed();
void Retry();
// The frontend is back after the game RETRY ended: true once, with the checkpoint to boot.
bool TakeRetry();
} // namespace PortRetry
