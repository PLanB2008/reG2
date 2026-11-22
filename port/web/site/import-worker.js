// Reads the player's GTA2 with the native setup's code (port/setup/install.cpp, compiled to reg2_import.wasm),
// off the page's thread. The source is mounted read-only from the chosen files (WORKERFS), the result is written
// to /out and sent back file by file.
//
// In:  {cmd: 'check' | 'install', entries: [{path, file}], root, installed: [paths]}
// Out: {type: 'progress', done, total} | {type: 'file', path, data} | {type: 'result', result, fullInstall}
//      | {type: 'error', message}
'use strict';
importScripts('reg2_import.js');

let modulePromise = null;

function module() {
  modulePromise ??= createReg2Import({ print: () => {}, printErr: (s) => console.warn('import:', s) });
  return modulePromise;
}

function removeTree(FS, path) {
  if (!FS.analyzePath(path).exists) return;
  for (const name of FS.readdir(path)) {
    if (name === '.' || name === '..') continue;
    const p = path + '/' + name;
    if (FS.isDir(FS.stat(p).mode)) removeTree(FS, p);
    else FS.unlink(p);
  }
  FS.rmdir(path);
}

function walk(FS, dir, base, out) {
  for (const name of FS.readdir(dir)) {
    if (name === '.' || name === '..') continue;
    const p = dir + '/' + name;
    const rel = base ? base + '/' + name : name;
    if (FS.isDir(FS.stat(p).mode)) walk(FS, p, rel, out);
    else out.push(rel);
  }
  return out;
}

onmessage = async ({ data }) => {
  try {
    const M = await module();
    const FS = M.FS;
    try { FS.unmount('/src'); } catch {}
    removeTree(FS, '/src');
    removeTree(FS, '/out');
    FS.mkdir('/src');
    FS.mkdir('/out');
    FS.mount(M.WORKERFS, { blobs: data.entries.map((e) => ({ name: e.path, data: e.file })) }, '/src');
    // The installed game as empty files: the setup then sees it, to add the music or the movie to it.
    for (const path of data.installed || []) {
      const p = '/out/' + path;
      FS.mkdirTree(p.slice(0, p.lastIndexOf('/')));
      FS.writeFile(p, new Uint8Array(0));
    }
    FS.chdir('/out');
    const source = '/src/' + data.root;

    if (data.cmd === 'check') {
      const result = JSON.parse(M.ccall('import_check', 'string', ['string'], [source]));
      postMessage({ type: 'result', result });
      return;
    }

    let last = 0;
    const write = FS.write;
    FS.write = function (...args) {
      const n = write.apply(this, args);
      const now = performance.now();
      if (now - last > 100) {
        last = now;
        postMessage({ type: 'progress', done: M._import_done(), total: M._import_total() });
      }
      return n;
    };
    const result = JSON.parse(M.ccall('import_install', 'string', ['string'], [source]));
    FS.write = write;
    let fullInstall = false;
    if (result.ok) {
      const files = walk(FS, '/out', '', []);
      const total = files.reduce((sum, f) => sum + FS.stat('/out/' + f).size, 0);
      let done = 0;
      for (const path of files) {
        const bytes = FS.readFile('/out/' + path);
        FS.unlink('/out/' + path);
        if (!bytes.length) continue;
        fullInstall ||= path.toLowerCase() === 'data/fstyle.sty';
        done += bytes.length;
        postMessage({ type: 'file', path, data: bytes.buffer, done, total }, [bytes.buffer]);
      }
    }
    postMessage({ type: 'result', result, fullInstall });
  } catch (e) {
    postMessage({ type: 'error', message: String(e && e.message || e) });
  }
};
