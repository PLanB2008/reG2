// The service worker keeps one build of the site (its files listed below by build.sh) in a cache of its own, so
// that a return visit starts at once and works offline. A new build installs beside the old one and takes over
// when the page asks (app.js, never during a game), so a page never mixes two builds' files.
// The pattern is OpenCE's (port/web/site/sw.js of OpenCommunityEdition/OpenCE), without its isolation headers.
'use strict';
const VERSION = '@VERSION@';
const FILES = ['./', ...@FILES@];
const CACHE = 'reg2-site-' + VERSION;

self.addEventListener('install', (event) => {
  event.waitUntil(caches.open(CACHE).then((cache) => cache.addAll(FILES.map((f) => new Request(f, { cache: 'reload' })))));
});

self.addEventListener('activate', (event) => {
  event.waitUntil((async () => {
    for (const name of await caches.keys()) {
      if (name.startsWith('reg2-site-') && name !== CACHE) await caches.delete(name);
    }
    await self.clients.claim();
  })());
});

self.addEventListener('message', (event) => {
  if (event.data === 'update') self.skipWaiting();
});

self.addEventListener('fetch', (event) => {
  const request = event.request;
  const url = new URL(request.url);
  if (request.method !== 'GET' || url.origin !== location.origin) return;
  event.respondWith((async () => {
    const cache = await caches.open(CACHE);
    const key = request.mode === 'navigate' ? './' : request;
    const cached = await cache.match(key, { ignoreSearch: request.mode === 'navigate' });
    if (cached) return cached;
    try {
      return await fetch(request);
    } catch {
      // Offline with this build not cached yet (or cleared), or the request was cancelled: the page from any build
      // kept, a short page saying so, or a plain error for a file.
      if (request.mode !== 'navigate') return Response.error();
      return (await caches.match('./', { ignoreSearch: true })) ||
        new Response('<!doctype html><title>reG2</title><p>reG2 needs the network once to start.</p>',
          { status: 503, headers: { 'content-type': 'text/html' } });
    }
  })());
});
