# reG2 landing page

The page at the root of the GitHub Pages site. Plain HTML and CSS, no build step and no JavaScript.
The Web workflow publishes it like this:

| Site path | Source |
|---|---|
| `/` | `port/web/home/` (this folder) |
| `/ds.css` | `port/web/ds.css` |
| `/play/` | the game page, `port/web/site/` plus the WebAssembly build |

`index.html` links `ds.css` as a sibling, so open it through that layout, not straight from this folder.

## Preview

```sh
P=/tmp/reg2-site
rm -rf "$P" && mkdir -p "$P/play"
cp -R port/web/home/. "$P/" && cp port/web/ds.css "$P/"
cp -R port/web/site/. "$P/play/"   # optional: the game page, or leave /play/ empty
python3 -m http.server -d "$P" 8000
```

Then open <http://localhost:8000/>. The site is dark only.

## Files

- `img/`: copies of `docs/images/*` (the reG2 logo for dark backgrounds, screenshots) and `port/resources/reg2_256.png`, so the
  page works when only this folder is published. Git stores identical files once, so the copies cost nothing
  in the repository; replace them when the originals change.
- `fonts/`: Saira (variable, latin subset, roman and italic) and Share Tech Mono, both under the SIL Open
  Font License (licences next to them), from Google Fonts. `ds.css`
  loads them from `fonts/` beside itself, which is this folder once published.
- `home.css`: this page's layout only.

## Design system (`ds.css`)

Shared by this page and the game page, dark only, in the spirit of GTA2's frontend: colour blocks with angled
cuts, slanted display type, gang colours. Use the tokens instead of literal values:

- Colours: `--bg`, `--bg-2` (raised surfaces), `--line`, `--text`, `--muted`, `--red` (the main action),
  `--red-ink` (text on red), `--amber` (warnings), `--ok`, `--focus`.
- Gang colours `--red`, `--amber`, `--cyan`, `--lime`, `--pink`. A section picks one with `.acc-red`,
  `.acc-amber`, `.acc-cyan`, `.acc-lime` or `.acc-pink`, which sets `--accent`; labels, bullets, link
  underlines, rules and image shadows inside it follow `--accent`. Text on an accent block uses `--accent-ink`.
- `--lcd` and `--lcd-ink`: the pager (`.pager`), used for the beta warning.
- Type: `--font-display` (Saira black italic at `--display-stretch`, 125% wide, for uppercase headings),
  `--font-body` (Saira at normal width) and `--font-hud` (Share Tech Mono, for tags, captions, the pager and
  `.hud` readouts); sizes `--step--1` to `--step-5`.
- Shape: `--space-1` to `--space-7`, `--rule` (thick rules), `--radius` (square corners), `--cut` (angled
  corners of `.btn.primary`), `--slant` (skew of `.slant`), `--measure`, `--page-max`, `--gutter`.
- Components: `.btn` (`.primary`, `.big`, `.danger`), `.row`, `.panel` (`.warn`), `.label` (angled title
  block, `<h2 class="label">Title</h2>`), `.tag` (sticker), `.pager`, `.hud`, `.scanlines` (faint CRT lines over a positioned box), `.slant`, `.bar`,
  `.stripe` (on `body`), `.nocase` (keeps macOS or gta2_re in uppercase headings), `.foot`, `.skip`, `.lede`,
  `.small`.

One red main action per screen, one accent per section, amber for warnings. No Rockstar or GTA2 logos, fonts or
trademarks as branding.
