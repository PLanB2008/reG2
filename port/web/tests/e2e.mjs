// End-to-end tests of the web build with Playwright, against a served site (build.sh's output) and the player's
// own GTA2 (never part of the repository). Screenshots go to $OUT, outside the repository: they show the game.
//
//   npm i playwright && npx playwright install chromium firefox
//   python3 -m http.server -d <site> 8000 &
//   SITE=http://127.0.0.1:8000/ GTA2=~/GTA2INSTALLER.exe OUT=/tmp/shots node port/web/tests/e2e.mjs [chromium|firefox] [scenario...]
//
// Scenarios: import, play, save, contextloss, offline, tabs, hide, touch. Each runs in its own new profile (import first).
import { chromium, firefox } from 'playwright';
import { mkdtempSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';

const SITE = process.env.SITE || 'http://127.0.0.1:8000/';
const GTA2 = process.env.GTA2;
const OUT = process.env.OUT || tmpdir();
const [which = 'chromium', ...wanted] = process.argv.slice(2);
const engine = which === 'firefox' ? firefox : chromium;

const failures = [];
function check(name, ok, detail = '') {
  console.log(`${ok ? 'ok  ' : 'FAIL'} ${name}${detail ? ': ' + detail : ''}`);
  if (!ok) failures.push(name);
}

async function open(mobile) {
  const dir = mkdtempSync(join(tmpdir(), 'reg2-e2e-'));
  const options = { headless: true, locale: 'en-US', viewport: { width: 1280, height: 720 } };
  // GPU=vulkan (or gl): Chromium on the machine's GPU instead of SwiftShader.
  if (which === 'chromium' && process.env.GPU) options.args = ['--use-angle=' + process.env.GPU, '--enable-gpu', '--ignore-gpu-blocklist'];
  if (mobile) {
    Object.assign(options, { viewport: { width: 844, height: 390 }, deviceScaleFactor: 3, hasTouch: true, isMobile: which === 'chromium' });
  }
  const ctx = await engine.launchPersistentContext(dir, options);
  const page = ctx.pages()[0] || (await ctx.newPage());
  page.on('pageerror', (e) => console.log('  page error:', e.message));
  page.on('dialog', (d) => d.accept());
  return { ctx, page, close: async () => { await ctx.close(); rmSync(dir, { recursive: true, force: true }); } };
}

// A key held as long as a person holds it: the game reads the keyboard's state once per logic step.
async function press(page, key) {
  await page.keyboard.down(key);
  await page.waitForTimeout(120);
  await page.keyboard.up(key);
}

const hidden = (page, id) => page.evaluate((i) => document.getElementById(i).hidden, id);
const state = (page) => page.evaluate(() => window.reg2.state());
const shot = (page, name) => page.screenshot({ path: join(OUT, `${which}-${name}.png`) });

async function importGame(page) {
  await page.goto(SITE);
  const [chooser] = await Promise.all([page.waitForEvent('filechooser'), page.click('#pick-file')]);
  await chooser.setFiles(GTA2);
  await page.waitForFunction(() => !document.getElementById('report').hidden || document.getElementById('import-error').textContent);
  const report = await page.textContent('#report-list');
  check('import: report', !(await hidden(page, 'report')), report);
  const start = Date.now();
  await page.click('#do-import');
  await page.waitForFunction(() => !document.getElementById('ready').hidden, null, { timeout: 900000 });
  check('import: stored', true, `${((Date.now() - start) / 1000).toFixed(1)} s`);
}

// From the page to a game in Downtown, with the keyboard: skip the intro, the style (first start), PLAY, START.
async function startGame(page, firstStart) {
  await page.click('#play');
  await page.waitForFunction(() => window.reg2 && window.reg2.module && window.reg2.state() & 1, null, { timeout: 60000 });
  await page.waitForTimeout(3000);
  const keys = firstStart ? 4 : 3;
  for (let i = 0; i < keys; i++) {
    await press(page, 'Enter');
    await page.waitForTimeout(2500);
    if (process.env.STEPS) await shot(page, 'start-' + i);
  }
  await page.waitForFunction(() => window.reg2.state() & 2, null, { timeout: 60000 });
  await page.waitForTimeout(3000);
}

async function hold(page, key, ms) {
  await page.keyboard.down(key);
  await page.waitForTimeout(ms);
  await page.keyboard.up(key);
}

const scenarios = {
  async import() {
    const { page, close } = await open();
    await importGame(page);
    await page.reload();
    await page.waitForTimeout(2000);
    check('import: ready after reload', !(await hidden(page, 'ready')));
    await close();
  },

  async play() {
    const { page, close } = await open();
    await importGame(page);
    await startGame(page, true);
    check('play: in a game', (await state(page)) & 2);
    await hold(page, 'ArrowUp', 2000);
    await shot(page, 'play');
    await close();
  },

  // SAVE AND QUIT in the pause menu, a reload, then LOAD GAME.
  async save() {
    const { page, close } = await open();
    await importGame(page);
    await startGame(page, true);
    await hold(page, 'ArrowLeft', 1500);
    await press(page, 'Escape');
    await page.waitForTimeout(1500);
    check('save: pause menu', (await state(page)) & 4);
    await press(page, 'ArrowUp');
    await press(page, 'Enter');
    await page.waitForTimeout(1000);
    await press(page, 'Enter');
    await page.waitForTimeout(4000);
    const files = await page.evaluate(() => import('./storage.js').then((s) => s.userFiles()));
    check('save: autosave stored', files.some((f) => /_auto_/.test(f)), files.join(' '));
    await page.goto(SITE);
    await page.click('#play');
    await page.waitForFunction(() => window.reg2 && window.reg2.module);
    await page.waitForTimeout(6000);
    for (const key of ['Enter', 'Enter', 'ArrowDown', 'ArrowDown', 'Enter', 'Enter']) {
      await press(page, key);
      await page.waitForTimeout(key === 'Enter' ? 2000 : 400);
    }
    await page.waitForFunction(() => window.reg2.state() & 2, null, { timeout: 60000 });
    check('save: loaded after reload', true);
    await shot(page, 'save-loaded');
    await close();
  },

  async contextloss() {
    const { page, close } = await open();
    await importGame(page);
    await page.click('#play');
    await page.waitForFunction(() => window.reg2 && window.reg2.module);
    await page.waitForTimeout(6000);
    await page.evaluate(() => document.getElementById('canvas').getContext('webgl2').getExtension('WEBGL_lose_context').loseContext());
    await page.waitForTimeout(1000);
    check('contextloss: message', !(await hidden(page, 'crash')), await page.textContent('#crash-title'));
    await close();
  },

  async offline() {
    const { ctx, page, close } = await open();
    await importGame(page);
    await page.waitForFunction(() => navigator.serviceWorker.controller, null, { timeout: 30000 }).catch(() => {});
    await ctx.setOffline(true);
    await page.goto(SITE);
    check('offline: page', !(await hidden(page, 'ready')));
    await page.click('#play');
    await page.waitForFunction(() => window.reg2 && window.reg2.module && window.reg2.state() & 1, null, { timeout: 60000 });
    check('offline: game started', true);
    await close();
  },

  // A second tab offers to take over; taking over stops the first one.
  async tabs() {
    const { ctx, page, close } = await open();
    await importGame(page);
    const second = await ctx.newPage();
    await second.goto(SITE);
    await second.waitForTimeout(1500);
    check('tabs: second tab told', !(await hidden(second, 'other-tab')));
    await second.click('#take-over');
    await second.waitForTimeout(1500);
    check('tabs: second tab ready', !(await hidden(second, 'ready')));
    check('tabs: first tab stopped', !(await hidden(page, 'other-tab')));
    await close();
  },

  // The tab hidden in a game: the pause menu opens and the sound stops.
  async hide() {
    const { page, close } = await open();
    await importGame(page);
    await startGame(page, true);
    await page.evaluate(() => {
      Object.defineProperty(document, 'hidden', { configurable: true, get: () => true });
      document.dispatchEvent(new Event('visibilitychange'));
    });
    await page.waitForTimeout(500);
    const audio = await page.evaluate(() => window.reg2.audioContext()?.state);
    await page.evaluate(() => {
      Object.defineProperty(document, 'hidden', { configurable: true, get: () => false });
      document.dispatchEvent(new Event('visibilitychange'));
    });
    await page.waitForTimeout(1000);
    check('hide: pause menu', (await state(page)) & 4);
    check('hide: sound suspended', audio !== 'running', audio);
    await close();
  },

  // A phone in landscape: the menus by touch, the stick and a button.
  async touch() {
    const { ctx, page, close } = await open(true);
    await importGame(page);
    await page.click('#play');
    await page.waitForFunction(() => window.reg2 && window.reg2.module);
    await page.waitForTimeout(6000);
    check('touch: controls shown', !(await hidden(page, 'touch')));
    const back = await page.locator('#touch .tbtn.menu-only').boundingBox();
    await page.touchscreen.tap(back.x + back.width / 2, back.y + back.height / 2);
    await page.waitForTimeout(2500);
    for (let i = 0; i < 3; i++) {
      await press(page, 'Enter');
      await page.waitForTimeout(2500);
    }
    await page.waitForFunction(() => window.reg2.state() & 2, null, { timeout: 60000 });
    await page.waitForTimeout(3000);
    const cdp = await ctx.newCDPSession(page);
    await cdp.send('Input.dispatchTouchEvent', { type: 'touchStart', touchPoints: [{ x: 150, y: 250 }] });
    for (let i = 1; i <= 10; i++) {
      await cdp.send('Input.dispatchTouchEvent', { type: 'touchMove', touchPoints: [{ x: 150, y: 250 - 7 * i }] });
      await page.waitForTimeout(30);
    }
    await page.waitForTimeout(2000);
    check('touch: in-game controls', (await page.evaluate(() => document.getElementById('touch').dataset.mode)) === 'foot');
    await shot(page, 'touch');
    await cdp.send('Input.dispatchTouchEvent', { type: 'touchEnd', touchPoints: [] });
    await close();
  },
};

if (!GTA2) {
  console.log('GTA2=<a GTA2 installer, disc image or zip> is needed');
  process.exit(2);
}
for (const name of wanted.length ? wanted : Object.keys(scenarios)) {
  if (name === 'touch' && which !== 'chromium') continue;
  console.log(`-- ${which} ${name}`);
  try {
    await scenarios[name]();
  } catch (e) {
    check(name, false, e.message.split('\n')[0]);
  }
}
console.log(failures.length ? `${failures.length} failed: ${failures.join(', ')}` : 'all passed');
process.exit(failures.length ? 1 : 0);
