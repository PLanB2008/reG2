# Web port: status

Scratch notes for following the work on `port/web`; `web.md` is the documentation.

## Works
- The game and the importer build with Emscripten (`port/web/deps.sh`, `web.cmake`).
- The game runs in Chromium: intro movie, menus, a game in Downtown, 60 fps on a desktop GPU.
- Frames paced by requestAnimationFrame; no WebGL errors.

## Next
- The page: import, storage, play, settings around the game.
- Touch controls, gamepad, pointer lock, fullscreen, orientation.
- Service worker, offline, GitHub Pages workflow.
- Playwright tests, performance on mobile emulation.

## Decisions
- Main loop: Asyncify limited to the stack main, WinMain, GameWinMain, port_WebYield
  (`port/web/asyncify_only.json`): the game's own loop stays as it is, one yield per frame.
- No pthreads: the importer runs in a Web Worker of its own (a separate small wasm).
- Hidden on the web: DISPLAY MODE, VSYNC, 3D SOUND (no OpenAL), CHECK FOR UPDATES (not built).
