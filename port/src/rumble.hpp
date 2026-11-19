// VIBRATION (settings.hpp): the active gamepad shakes for the local player's hits on foot, explosions near him and
// crashes of the car he drives (port_Rumble in compat/src/pad.cpp). Overlapping shakes keep the stronger.
#pragma once

#include "fix16.hpp"

class Car;
class Ped;

namespace PortRumble
{
// In Ped::TakeDamage, before the damage is taken.
void Hit(const Ped* pPed, s32 damage);
// In Game::ShakeCamerasAtPos, the first step of an explosion: stronger the nearer it is, nothing past a few blocks.
void Explosion(Fix16 x, Fix16 y);
// In Car::ApplyImpactDamage, with the collision's damage.
void Crash(const Car* pCar, Fix16 damage);
} // namespace PortRumble
