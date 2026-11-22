// On-screen controls for touch screens. They drive a virtual gamepad in the game (port_WebPad in
// web/src/web_input.cpp), so the port's pad support does the rest: the left stick walks or steers, RT and LT
// accelerate and brake, the buttons are the pad's default bindings (port/compat/src/pad.cpp's kDefaultPad).
// The stick is nipplejs; menus are played by tapping them, which the game sees as the mouse.
import './vendor/nipplejs.js';

// SDL_GamepadButton bits and SDL_GamepadAxis indices.
const A = 1 << 0, B = 1 << 1, Y = 1 << 3, START = 1 << 6, LB = 1 << 9, RB = 1 << 10;
const LT = 4, RT = 5;
const MIN_PRESS_MS = 40;

// Places are from the screen's edges (or its centre), in CSS pixels at size 100%; mode: shown on foot, in a
// car, in either (game), or in the menus.
const BUTTONS = [
  { label: 'attack', bit: A, mode: 'foot', right: 28, bottom: 40, size: 1.35 },
  { label: 'jump', bit: B, mode: 'foot', right: 120, bottom: 28 },
  { label: 'enter', bit: Y, mode: 'foot', right: 40, bottom: 140 },
  { label: 'gas', axis: RT, mode: 'car', right: 28, bottom: 40, size: 1.35 },
  { label: 'brake', axis: LT, mode: 'car', right: 124, bottom: 28, size: 1.1 },
  { label: 'handbrake', bit: B, mode: 'car', right: 112, bottom: 120 },
  { label: 'exit', bit: Y, mode: 'car', right: 34, bottom: 150 },
  { label: 'fire', bit: A, mode: 'car', right: 200, bottom: 34, small: true },
  { label: '◀', bit: LB, mode: 'foot', right: 196, bottom: 150, small: true, raw: true },
  { label: '▶', bit: RB, mode: 'foot', right: 136, bottom: 196, small: true, raw: true },
  { label: 'pause', bit: START, mode: 'game', centre: true, top: 8, small: true, wide: true },
  { label: 'back', bit: B, mode: 'menu', left: 12, top: 8, small: true, wide: true },
];

const LABELS = {
  en: { attack: 'Attack', jump: 'Jump', enter: 'Enter', gas: 'Gas', brake: 'Brake', handbrake: 'Hand brake', exit: 'Exit', fire: 'Fire', pause: 'Pause', back: 'Back' },
  fr: { attack: 'Frapper', jump: 'Sauter', enter: 'Monter', gas: 'Gaz', brake: 'Frein', handbrake: 'Frein à main', exit: 'Sortir', fire: 'Tirer', pause: 'Pause', back: 'Retour' },
  de: { attack: 'Angriff', jump: 'Springen', enter: 'Einsteigen', gas: 'Gas', brake: 'Bremse', handbrake: 'Handbremse', exit: 'Aussteigen', fire: 'Feuer', pause: 'Pause', back: 'Zurück' },
  it: { attack: 'Attacca', jump: 'Salta', enter: 'Sali', gas: 'Gas', brake: 'Freno', handbrake: 'Freno a mano', exit: 'Scendi', fire: 'Spara', pause: 'Pausa', back: 'Indietro' },
  es: { attack: 'Atacar', jump: 'Saltar', enter: 'Subir', gas: 'Gas', brake: 'Freno', handbrake: 'Freno de mano', exit: 'Bajar', fire: 'Disparar', pause: 'Pausa', back: 'Atrás' },
};

export class TouchControls {
  constructor(root, module, getState, prefs) {
    this.root = root;
    this.module = module;
    this.getState = getState;
    this.buttons = 0;
    this.axes = [0, 0, 0, 0, 0, 0];
    this.usedTouch = false;
    this.visible = false;
    const lang = (navigator.language || 'en').slice(0, 2);
    const labels = LABELS[lang] || LABELS.en;

    const zone = document.createElement('div');
    zone.className = 'zone stick-zone game-only';
    root.appendChild(zone);
    this.stick = window.nipplejs.create({ zone, mode: 'dynamic', color: 'white', size: 120, threshold: 0.05, fadeTime: 100 });
    this.stick.on('move', (_, data) => {
      const force = Math.min(1, data.force || 0);
      const angle = data.angle ? data.angle.radian : 0;
      this.axes[0] = Math.round(Math.cos(angle) * force * 32767);
      this.axes[1] = Math.round(-Math.sin(angle) * force * 32767);
      this.send();
    });
    this.stick.on('end', () => {
      this.axes[0] = this.axes[1] = 0;
      this.send();
    });

    for (const def of BUTTONS) {
      const el = document.createElement('div');
      el.className = 'tbtn ' + (def.mode === 'menu' ? 'menu-only' : def.mode === 'game' ? 'game-only' : def.mode + '-only game-only') +
        (def.small ? ' small' : '') + (def.wide ? ' wide' : '');
      el.textContent = def.raw ? def.label : labels[def.label];
      el.setAttribute('aria-hidden', 'true');
      for (const side of ['right', 'bottom', 'left', 'top']) {
        if (def[side] !== undefined) el.style[side] = `calc(${def[side]}px * var(--touch-scale, 1) + env(safe-area-inset-${side}))`;
      }
      // Clear of the HUD, which fills the top corners.
      if (def.centre) el.style.left = 'calc(50% + 6px)';
      if (def.size) {
        el.style.width = el.style.height = `calc(64px * ${def.size} * var(--touch-scale, 1))`;
      }
      const pointers = new Set();
      let pressedAt = 0;
      let releaseTimer = 0;
      const update = () => {
        const down = pointers.size > 0;
        // SDL reads the virtual pad's state once a frame: a tap must last across one of its reads.
        clearTimeout(releaseTimer);
        if (down) pressedAt = performance.now();
        else if (performance.now() - pressedAt < MIN_PRESS_MS) {
          releaseTimer = setTimeout(update, MIN_PRESS_MS - (performance.now() - pressedAt));
          return;
        }
        el.classList.toggle('down', down);
        if (def.axis !== undefined) this.axes[def.axis] = down ? 32767 : 0;
        else this.buttons = down ? this.buttons | def.bit : this.buttons & ~def.bit;
        this.send();
        if (down && navigator.vibrate) navigator.vibrate(8);
      };
      el.addEventListener('pointerdown', (e) => {
        e.preventDefault();
        el.setPointerCapture(e.pointerId);
        pointers.add(e.pointerId);
        update();
      });
      for (const type of ['pointerup', 'pointercancel', 'lostpointercapture']) {
        el.addEventListener(type, (e) => {
          if (pointers.delete(e.pointerId)) update();
        });
      }
      root.appendChild(el);
    }

    // Shown while touch is the input in use (or always, or never, as set on the page).
    // Browsers follow a touch with emulated mouse events: those are not a mouse in use.
    let lastTouch = 0;
    addEventListener('touchstart', () => {
      lastTouch = performance.now();
      this.setUsed(true);
    }, { capture: true, passive: true });
    addEventListener('touchend', () => (lastTouch = performance.now()), { capture: true, passive: true });
    addEventListener('keydown', (e) => {
      if (!e.isTrusted) return;
      this.setUsed(false);
    }, true);
    addEventListener('pointermove', (e) => {
      if (e.pointerType === 'mouse' && (e.movementX || e.movementY) && performance.now() - lastTouch > 1000) this.setUsed(false);
    }, true);
    addEventListener('gamepadconnected', () => this.setUsed(false));
    this.usedTouch = matchMedia('(pointer: coarse)').matches;
    this.setPrefs(prefs);
    this.poll();
  }

  setUsed(touch) {
    if (this.usedTouch === touch) return;
    this.usedTouch = touch;
    this.refresh();
  }

  setPrefs(prefs) {
    this.prefs = prefs;
    this.root.style.setProperty('--touch-scale', prefs.touchSize / 100);
    this.root.style.setProperty('--touch-opacity', prefs.touchOpacity / 100);
    this.refresh();
  }

  refresh() {
    const mode = this.prefs.touchMode;
    const visible = mode === 'on' || (mode === 'auto' && this.usedTouch);
    if (visible !== this.visible) {
      this.visible = visible;
      this.root.hidden = !visible;
      // The controls have their own pause button.
      document.body.classList.toggle('touch-on', visible);
      if (!visible) this.release();
    }
  }

  // A physical gamepad in use hides the controls, as a key or the mouse does.
  poll() {
    const state = this.getState();
    const mode = state & 2 ? (state & 4 ? 'menu' : state & 8 ? 'car' : 'foot') : 'menu';
    if (this.root.dataset.mode !== mode) {
      this.root.dataset.mode = mode;
      this.release();
    }
    if (this.visible && navigator.getGamepads) {
      for (const pad of navigator.getGamepads()) {
        if (pad && pad.buttons.some((b) => b.pressed)) {
          this.setUsed(false);
          break;
        }
      }
    }
    requestAnimationFrame(() => this.poll());
  }

  release() {
    this.buttons = 0;
    this.axes.fill(0);
    for (const el of this.root.querySelectorAll('.tbtn.down')) el.classList.remove('down');
    this.send();
  }

  send() {
    // The game gets its virtual pad only once the controls are in use.
    if (!this.visible && !this.attached) return;
    this.attached = true;
    try {
      this.module._port_WebPad(this.buttons, ...this.axes);
    } catch {}
  }
}
