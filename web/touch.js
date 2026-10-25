// civ-wasm: touch controls for phones and tablets (index.html).
//
// A layer over the canvas takes every touch, so the game sees clicks only
// when a gesture is over:
//   tap        a left click (select a unit, enter a city, recentre; next to
//              the active unit it moves there, CONTROL.C)
//   long press on the map a plain click (select or recentre, never a move);
//              on other screens the right button (the game's help)
//   drag       on the map, scrolls it 4 tiles at a time (Shift+arrow keys);
//              on other screens the left button held, as a mouse drag
// The buttons beside the picture send the keys the game already knows, so
// every order is taken at the game's own wait for input.
'use strict';

const Touch = (() => {
  const $ = id => document.getElementById(id);
  const call = (name, ...a) => { try { Module['_' + name](...a); } catch {} };
  const key = code => call('civwasm_key', code);

  // BIOS keystrokes (scan << 8 | ASCII) the buttons send.
  const K = {
    enter: 0x1C0D, esc: 0x011B, space: 0x3920, tab: 0x0F09, bksp: 0x0E08,
    up: 0x4800, down: 0x5000, left: 0x4B00, right: 0x4D00,
    upleft: 0x4700, upright: 0x4900, downleft: 0x4F00, downright: 0x5100,
    altG: 0x2200, altO: 0x1800, altA: 0x1E00, altW: 0x1100, altC: 0x2E00,
  };
  const F = n => (0x3A + n) << 8;           // F1..F10
  // Typed characters, by position on a US keyboard (keyboard.c).
  const ROWS = [['1234567890-=', '!@#$%^&*()_+', 0x02], ['qwertyuiop[]', 'QWERTYUIOP{}', 0x10],
                ["asdfghjkl;'`", 'ASDFGHJKL:"~', 0x1E], ['zxcvbnm,./', 'ZXCVBNM<>?', 0x2C]];
  function charKey(c) {
    if (c === ' ') return K.space;
    if (c === '\\' || c === '|') return 0x2B00 | c.charCodeAt(0);
    for (const [plain, shifted, scan] of ROWS) {
      const i = Math.max(plain.indexOf(c), shifted.indexOf(c));
      if (i >= 0) return (scan + i) << 8 | c.charCodeAt(0);
    }
    return c.charCodeAt(0) < 128 ? c.charCodeAt(0) : 0;
  }
  const letter = c => charKey(c);

  // The map's orders, as on the DOS Orders menu.
  const ORDERS = [
    ['Skip', 'Next unit, or end the turn (Space)', () => key(K.space)],
    ['Wait', 'Wait: come back to this unit later (w)', () => key(letter('w'))],
    ['Sentry', 'Sentry: sleep until an enemy comes near (s)', () => key(letter('s'))],
    ['Fortify', 'Fortify (f)', () => key(letter('f'))],
    ['GoTo', 'GoTo: then tap where to go (g)', () => { key(letter('g')); goto(true); }, 'goto'],
    ['City', 'Found a new city, or join one (b)', () => key(letter('b'))],
    ['Road', 'Build a road or railroad (r)', () => key(letter('r'))],
    ['Irrig.', 'Irrigate (i)', () => key(letter('i'))],
    ['Mine', 'Build a mine (m)', () => key(letter('m'))],
    ['Home', 'Make the nearest city its home (h)', () => key(letter('h'))],
    ['Centre', 'Centre the map on the unit (c)', () => key(letter('c'))],
    ['Orders…', 'The game\'s Orders menu, all orders for this unit', () => key(K.altO)],
  ];
  // Screens that are not the map: dialogs, the city screen, reports.
  const DIALOG = [
    ['OK', 'Enter', () => key(K.enter)],
    ['Back', 'Escape', () => key(K.esc)],
    ['▲', 'Up', () => key(K.up)],
    ['▼', 'Down', () => key(K.down)],
    ['Type…', 'Show the keyboard, to type a name', () => keyboard()],
  ];
  // The menu sheet: the DOS menu bar's entries.
  const MENU = [
    ['Game', [
      ['Tax rate', () => key(letter('+'))],
      ['Luxuries rate', () => key(letter('-'))],
      ['Find city', () => key(letter('?'))],
      ['Save game', () => key(letter('S'))],
      ['Game menu… (options, revolution, retire)', () => key(K.altG)],
      ['Type… (the keyboard)', () => keyboard()],
    ]],
    ['Advisors', [
      ['City status', () => key(F(1))], ['Military', () => key(F(2))],
      ['Intelligence', () => key(F(3))], ['Attitude', () => key(F(4))],
      ['Trade', () => key(F(5))], ['Science', () => key(F(6))],
    ]],
    ['World', [
      ['Wonders of the world', () => key(F(7))], ['Top 5 cities', () => key(F(8))],
      ['Civilization score', () => key(F(9))], ['World map', () => key(F(10))],
      ['World menu… (demographics, spaceships)', () => key(K.altW)],
    ]],
    ['Civilopedia', [['Civilopedia…', () => key(K.altC)]]],
  ];

  let on = false, wide = true, gotoArmed = false;

  function button(label, title, action, cls) {
    const b = document.createElement('button');
    b.textContent = label;
    b.title = title;
    if (cls) b.className = cls;
    // Act on pointerdown: a phone's click comes late, and the game should
    // not lose a tap to a scroll of the page.
    b.addEventListener('pointerdown', e => { e.preventDefault(); action(); });
    b.addEventListener('click', e => e.preventDefault());
    return b;
  }

  function goto(armed) {
    gotoArmed = armed;
    document.querySelector('#t-orders .goto')?.classList.toggle('armed', armed);
  }

  // ---------------------------------------------------------- layout

  function build() {
    const side = $('t-side'), orders = $('t-orders');
    side.replaceChildren(
      button('☰', 'Menu: tax, advisors, world, Civilopedia, settings', () => sheet(true)),
      button('+', 'Zoom in', () => zoom(1)),
      button('−', 'Zoom out, more map', () => zoom(-1)),
      button('⏎', 'Enter: end of turn, or OK', () => key(K.enter)),
      button('⌨', 'Keyboard, to type a name', () => keyboard()),
      ...(fullscreenAllowed() ? [button('⛶', 'Fullscreen', () => fullscreen())] : []));
    orders.replaceChildren(...ORDERS.map(o => button(...o)));
    $('t-dialog').replaceChildren(...DIALOG.map(o => button(...o)));
    const list = $('t-menu-list');
    list.replaceChildren();
    for (const [title, items] of MENU) {
      const h = document.createElement('h3');
      h.textContent = title;
      list.append(h);
      for (const [label, action] of items)
        list.append(button(label, label, () => { sheet(false); action(); }));
    }
  }

  const fullscreenAllowed = () => !!document.documentElement.requestFullscreen;
  function fullscreen() {
    if (document.fullscreenElement) return document.exitFullscreen();
    document.documentElement.requestFullscreen({ navigationUI: 'hide' })
      .then(() => screen.orientation?.lock?.('portrait').catch(() => {}))
      .catch(() => {});
  }

  function sheet(open) {
    $('t-menu').hidden = !open;
    if (open) {
      $('t-enemy').textContent = ENEMY_LABEL[$('enemy-moves').value];
      $('t-ios').hidden = fullscreenAllowed() || matchMedia('(display-mode: fullscreen), (display-mode: standalone)').matches;
    }
  }

  // The map or a 320x200 screen: the buttons to suit.
  function poll() {
    if (!on) return;
    const w = !!Module._civwasm_wide_view?.();
    if (w !== wide) {
      wide = w;
      document.body.classList.toggle('t-flat', !wide);
    }
  }

  // ---------------------------------------------------------- gestures

  const LONG = 450, SLOP = 12;
  let touch = null;          // the gesture under way

  // A canvas CSS pixel, as SDL's window coordinates.
  function at(e) {
    const r = $('canvas').getBoundingClientRect();
    return [e.clientX - r.left, e.clientY - r.top];
  }
  // The CSS size of one map tile: the game's screen fitted at 4:3 into the canvas.
  function tileSize() {
    const c = $('canvas').getBoundingClientRect(), s = screenSize() || [320, 200];
    const w = 80 + 16 * Math.min(80, Math.floor((s[0] - 80) / 16)), h = 8 + 16 * Math.min(50, Math.floor((s[1] - 8) / 16));
    const scale = Math.min(c.width / w, c.height / (h * 1.2));
    return [16 * scale, 19.2 * scale];
  }

  function click(x, y, buttons) {
    call('civwasm_mouse', x, y, 0);
    call('civwasm_mouse', x, y, buttons);
    setTimeout(() => call('civwasm_mouse', x, y, 0), 70);
  }

  function down(e) {
    if (touch) return;                       // one finger at a time
    e.preventDefault();
    $('t-layer').setPointerCapture(e.pointerId);
    const [x, y] = at(e);
    touch = { id: e.pointerId, x0: e.clientX, y0: e.clientY, x, y, dx: 0, dy: 0, moved: false, held: false, long: false };
    if (!wide) {
      // A mouse press, as it happens: the game's lists and sliders follow it.
      call('civwasm_mouse', x, y, 0);
      call('civwasm_mouse', x, y, 1);
      touch.held = true;
    }
    touch.timer = setTimeout(() => {
      if (!touch || touch.moved) return;
      touch.long = true;
      ring(touch.x0, touch.y0);
      navigator.vibrate?.(15);
      if (touch.held) {                      // the press becomes the right button
        call('civwasm_mouse', touch.x, touch.y, 0);
        touch.held = false;
        click(touch.x, touch.y, 2);
      } else
        click(touch.x, touch.y, 1 | 0x100);
    }, LONG);
  }

  function move(e) {
    if (!touch || e.pointerId !== touch.id) return;
    e.preventDefault();
    const [x, y] = at(e);
    touch.x = x;
    touch.y = y;
    if (Math.hypot(e.clientX - touch.x0, e.clientY - touch.y0) > SLOP) touch.moved = true;
    if (touch.held) return call('civwasm_mouse', x, y, 1);
    if (!touch.moved || touch.long) return;
    // Scroll the map a step (4 tiles) for each 4 tiles dragged: the map
    // follows the finger.
    const [tw, th] = tileSize();
    const sx = Math.trunc((e.clientX - touch.x0 - touch.dx * 4 * tw) / (4 * tw));
    const sy = Math.trunc((e.clientY - touch.y0 - touch.dy * 4 * th) / (4 * th));
    if (sx || sy) {
      const h = Math.sign(sx), v = Math.sign(sy);
      // Dragging right shows what is to the left: scroll west.
      const code = { '-1,-1': 0x5133, '0,-1': 0x5032, '1,-1': 0x4F31, '-1,0': 0x4D36,
                     '1,0': 0x4B34, '-1,1': 0x4939, '0,1': 0x4838, '1,1': 0x4737 }[`${h},${v}`];
      key(code);
      touch.dx += h;
      touch.dy += v;
    }
  }

  function up(e) {
    if (!touch || e.pointerId !== touch.id) return;
    e.preventDefault();
    clearTimeout(touch.timer);
    const [x, y] = at(e);
    if (touch.held) call('civwasm_mouse', x, y, 0);
    else if (!touch.moved && !touch.long && e.type === 'pointerup') {
      click(touch.x, touch.y, 1);
      if (gotoArmed) goto(false);
    }
    touch = null;
  }

  function ring(x, y) {
    const r = $('t-ring');
    r.style.left = x + 'px';
    r.style.top = y + 'px';
    r.classList.remove('show');
    void r.offsetWidth;
    r.classList.add('show');
  }

  // ---------------------------------------------------------- typing

  // Phones show their keyboard for a text field, not a canvas: a hidden
  // field takes the typing and sends it on as keystrokes.
  function keyboard() {
    const f = $('t-type');
    f.value = '';
    f.focus();
  }

  // ---------------------------------------------------------- start

  // Touch controls: the start page's setting, or ?ui=touch|mouse.
  function wanted() {
    const ui = new URLSearchParams(location.search).get('ui');
    if (ui) return ui === 'touch';
    return $('controls').value === 'touch';
  }

  function start() {
    on = wanted();
    if (!on) return [];
    document.body.classList.add('touch');
    build();
    const layer = $('t-layer');
    layer.addEventListener('pointerdown', down);
    layer.addEventListener('pointermove', move);
    layer.addEventListener('pointerup', up);
    layer.addEventListener('pointercancel', up);
    $('t-close').addEventListener('click', () => sheet(false));
    $('t-menu').addEventListener('pointerdown', e => { if (e.target === $('t-menu')) sheet(false); });
    $('t-zoom-in').addEventListener('click', () => zoom(1));
    $('t-zoom-out').addEventListener('click', () => zoom(-1));
    $('t-enemy').addEventListener('click', () => { $('enemy').click(); $('t-enemy').textContent = ENEMY_LABEL[$('enemy-moves').value]; });
    $('t-leave').addEventListener('click', () => { dispatchEvent(new Event('pagehide')); setTimeout(() => location.reload(), 300); });
    const f = $('t-type');
    // Its keys are sent below; SDL's own listeners must not get them twice.
    for (const t of ['keydown', 'keyup', 'keypress']) f.addEventListener(t, e => e.stopPropagation());
    f.addEventListener('input', () => { for (const c of f.value) { const k = charKey(c); if (k) key(k); } f.value = ''; });
    f.addEventListener('keydown', e => {
      const k = { Enter: K.enter, Backspace: K.bksp, Escape: K.esc }[e.key];
      if (k) { e.preventDefault(); key(k); if (e.key !== 'Backspace') f.blur(); }
    });
    setInterval(poll, 200);
    return ['--touch'];
  }

  return { start, on: () => on };
})();
