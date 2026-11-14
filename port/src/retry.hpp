// RETRY JOBS (settings.hpp): the retry checkpoint (saves.hpp) written as a job or a kill frenzy starts, deleted
// once it is passed; after a failure, a line on the HUD says how to go back to it from the pause menu.
#pragma once

#include "saves.hpp"

struct SCR_CMD_HEADER;

namespace PortRetry
{
// A level starts, gGameSession's save name still the save it was loaded from: a new game in a city, or loading a
// save older than the checkpoint, deletes the checkpoint.
void Start();
// ScriptThread, as a job is launched (by that command) or a kill frenzy starts, before either marks anything as
// started.
void BeforeStart(PortSaves::Checkpoint kind, const SCR_CMD_HEADER* pCmd = NULL);
// ScriptThread::launch_mission, once the job's script is loaded at that base pointer: the checkpoint just
// written gets its title and the gang that gave it, or the command that launched it if no phone did.
void JobLoaded(const SCR_CMD_HEADER* pCmd, u16 base);
// After each logic step.
void AfterStep();
// The player's latest steady spot (one a second on foot on the ground or driving on a road, with RETRY JOBS or
// AUTOSAVE on), false if none since the level started or he was last wasted or busted.
bool LastSteadySpot(PortSaves::Spot* pSpot);
// The latest of the last seconds a few steps from where the player is, out of what he walked into; false if none.
bool SpotAway(PortSaves::Spot* pSpot);
// With the HUD.
void Draw();
// The pause menu's RETRY: right after a failure it comes selected; RESTART while the job or frenzy runs.
bool JustFailed();
bool Running();
void Retry();
// The game ends for RETRY.
bool Retrying();
// The frontend is back after the game RETRY ended: true once, with the checkpoint to boot.
bool TakeRetry();
} // namespace PortRetry
