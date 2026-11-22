// What the web build does in place of the desktop's: the game folder the page filled, and the yield to
// the browser once per frame.
#include <SDL3/SDL.h>
#include <emscripten.h>
#include <unistd.h>

extern "C" {
// web_library.js: resolves at the browser's next frame; the browser's frame period in ms.
void port_WebWaitFrame();
double port_WebFramePeriodMs();
}

namespace
{
bool sPresented;
bool sIdle;
Uint64 sLastYield;
} // namespace

// The page mounts the game's data and the player's files at /reG2 before main.
bool port_PrepareGameFolder(bool)
{
    // SDL's own yields (in SDL_Delay and the swap) would need Asyncify on every stack that reaches them.
    SDL_SetHint(SDL_HINT_EMSCRIPTEN_ASYNCIFY, "0");
    // The sound is mixed on the page's thread, between frames: SDL doubles this to 4096 frames, 85 ms at 48 kHz,
    // which rides over the game's short hitches where its default 2048 ran dry.
    SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, "2048");
    return chdir("/reG2") == 0;
}

void port_SetWindowIcon(SDL_Window*)
{
}

extern "C" void port_WebMarkPresented()
{
    sPresented = true;
}

extern "C" void port_WebMarkIdle()
{
    sIdle = true;
}

extern "C" unsigned long long port_WebRefreshNs()
{
    return static_cast<unsigned long long>(port_WebFramePeriodMs() * 1e6);
}

bool port_FrontendActive();

// Called at the top of GameWinMain's loop: after a frame was drawn, when the loop waits for the next step,
// or at the latest every 50 ms, the browser gets its turn (events, sound, presenting the canvas). The frontend
// polls the clock between its 30 Hz updates without waiting: after 2 ms of that, it yields too.
extern "C" void port_WebYield()
{
    const Uint64 now = SDL_GetTicks();
    if (sPresented || sIdle || now - sLastYield >= 50 || (now - sLastYield >= 2 && port_FrontendActive()))
    {
        sPresented = false;
        sIdle = false;
        port_WebWaitFrame();
        sLastYield = SDL_GetTicks();
    }
}
