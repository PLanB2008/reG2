// The game's calls into the page (web_platform.cpp).
addToLibrary({
  // period: the display's frame period, the median of the last 31 intervals that look like one refresh.
  // busy: milliseconds the game ran between yields, summed until the page reads it (?stats in app.js).
  $webFrames: { last: 0, deltas: [], period: 1000 / 60, resumed: 0, busy: 0, frames: 0 },
  port_WebWaitFrame__async: true,
  port_WebWaitFrame__deps: ['$Asyncify', '$webFrames'],
  port_WebWaitFrame: () => Asyncify.handleSleep((wakeUp) => {
    const f = webFrames;
    Module['webFrames'] = f;
    if (f.resumed) {
      f.busy += performance.now() - f.resumed;
      f.frames++;
    }
    requestAnimationFrame((t) => {
      const d = t - f.last;
      f.last = t;
      if (d > 2 && d < 50) {
        f.deltas.push(d);
        if (f.deltas.length > 31) f.deltas.shift();
        f.period = [...f.deltas].sort((a, b) => a - b)[f.deltas.length >> 1];
      }
      f.resumed = performance.now();
      wakeUp();
    });
  }),
  // The game quit (QUIT, SAVE AND QUIT): the page's onGameQuit (app.js).
  port_WebQuit: (code) => {
    Module['onGameQuit']?.(code);
  },
  port_WebFramePeriodMs__deps: ['$webFrames'],
  port_WebFramePeriodMs: () => webFrames.period,
});
