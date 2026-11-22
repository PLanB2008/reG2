// The game's calls into the page (web_platform.cpp).
addToLibrary({
  port_WebWaitFrame__async: true,
  port_WebWaitFrame__deps: ['$Asyncify'],
  port_WebWaitFrame: () => Asyncify.handleSleep((wakeUp) => requestAnimationFrame(() => wakeUp())),
});
