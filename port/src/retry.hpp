// RETRY JOBS (settings.hpp): the retry checkpoint (saves.hpp) written as a job or a kill frenzy starts, deleted
// once it is passed.
#pragma once

#include "saves.hpp"

namespace PortRetry
{
// A level starts: a new game in a city deletes the checkpoint.
void Start();
// ScriptThread, as a job is launched or a kill frenzy starts, before either marks anything as started.
void BeforeStart(PortSaves::Checkpoint kind);
// After each logic step.
void AfterStep();
} // namespace PortRetry
