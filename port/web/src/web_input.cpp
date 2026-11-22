// What the page asks of the game between frames: its state, the touch controls' gamepad, the pause.
#include "Game.hpp"
#include "Ped.hpp"
#include "Player.hpp"
#include "../../src/pause_menu.hpp"
#include <SDL3/SDL.h>
#include <dinput.h>
#include <emscripten.h>

bool port_GameRunning();
bool port_FrontendActive();

namespace
{
SDL_Joystick* sTouchPad;
} // namespace

extern "C" {
// Bits: 1 the frontend, 2 a game, 4 the pause menu, 8 the player in a vehicle.
EMSCRIPTEN_KEEPALIVE int port_WebState()
{
    int state = port_FrontendActive() ? 1 : 0;
    if (port_GameRunning())
    {
        state |= 2;
        state |= PortPauseMenu::IsOpen() ? 4 : 0;
        const Player* pPlayer = gGame_0x40->mLocalPlayer;
        state |= pPlayer && pPlayer->mPlayerPed && pPlayer->mPlayerPed->mCar ? 8 : 0;
    }
    return state;
}

// Opens the pause menu in a game, as Escape does.
EMSCRIPTEN_KEEPALIVE void port_WebPause()
{
    if (port_GameRunning() && !PortPauseMenu::IsOpen() && gGame_0x40->mLocalPlayer)
    {
        PortPauseMenu::HandleKey(DIK_ESCAPE, gGame_0x40->mLocalPlayer);
    }
}

// The touch controls as a gamepad: SDL_GamepadButton bits, the sticks and triggers from -32768 to 32767
// (triggers 0 to 32767), in SDL_GamepadAxis order.
EMSCRIPTEN_KEEPALIVE void port_WebPad(int buttons, int lx, int ly, int rx, int ry, int lt, int rt)
{
    if (!sTouchPad)
    {
        SDL_VirtualJoystickDesc desc;
        SDL_INIT_INTERFACE(&desc);
        desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
        desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
        desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
        desc.name = "Touch controls";
        const SDL_JoystickID id = SDL_AttachVirtualJoystick(&desc);
        sTouchPad = id ? SDL_OpenJoystick(id) : NULL;
        if (!sTouchPad)
        {
            return;
        }
    }
    for (int i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; i++)
    {
        SDL_SetJoystickVirtualButton(sTouchPad, i, (buttons >> i) & 1);
    }
    const int axes[SDL_GAMEPAD_AXIS_COUNT] = {lx, ly, rx, ry, lt, rt};
    for (int i = 0; i < SDL_GAMEPAD_AXIS_COUNT; i++)
    {
        SDL_SetJoystickVirtualAxis(sTouchPad, i, (Sint16)SDL_clamp(axes[i], -32768, 32767));
    }
}
}
