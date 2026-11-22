// The game's calls into the page (web_platform.cpp).
addToLibrary({
  $webFrames: { last: 0, deltas: [], period: 1000 / 60 },
  port_WebWaitFrame__async: true,
  port_WebWaitFrame__deps: ['$Asyncify', '$webFrames'],
  port_WebWaitFrame: () => Asyncify.handleSleep((wakeUp) => requestAnimationFrame((t) => {
    // The display's period: the median of the last 31 frame intervals that look like one refresh.
    const f = webFrames;
    const d = t - f.last;
    f.last = t;
    if (d > 2 && d < 50) {
      f.deltas.push(d);
      if (f.deltas.length > 31) f.deltas.shift();
      f.period = [...f.deltas].sort((a, b) => a - b)[f.deltas.length >> 1];
    }
    wakeUp();
  })),
  port_WebFramePeriodMs__deps: ['$webFrames'],
  port_WebFramePeriodMs: () => webFrames.period,
});
