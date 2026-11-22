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

Then open <http://localhost:8000/>. Light and dark follow the system setting.

## Files

- `img/`: copies of `docs/images/*` (the reG2 logos, screenshots) and `port/resources/reg2_256.png`, so the
  page works when only this folder is published. Git stores identical files once, so the copies cost nothing
  in the repository; replace them when the originals change.
- `fonts/`: Big Shoulders and Barlow, both under the SIL Open Font License (licences next to them). `ds.css`
  loads them from `fonts/` beside itself, which is this folder once published.
- `home.css`: this page's layout only.

## Design system (`ds.css`)

Shared by this page and the game page. Use the tokens instead of literal values:

- Colours: `--bg`, `--bg-2` (raised surfaces), `--line`, `--text`, `--muted`, `--red` (the accent, links and
  the main action), `--red-ink` (text on red), `--amber` (warnings, the beta notice), `--ok`, `--focus`.
  Dark is the default; the light values apply under `prefers-color-scheme: light`.
- Type: `--font-display` (Big Shoulders, uppercase headings only) and `--font-body` (Barlow); sizes
  `--step--1` to `--step-5`.
- Spacing `--space-1` to `--space-7`, `--rule` (the thick left rules), `--radius` (square corners),
  `--measure` (line length), `--page-max`, `--gutter`.
- Components: `.btn` (`.primary`, `.big`, `.danger`), `.row`, `.panel` (`.warn`), `.notice`, `.foot`,
  `.skip`, `.lede`, `.small`, `.bar` (red bar under a heading), `.stripe` (on `body`: the street-light
  stripe), `.nocase` (keeps the case of names like macOS in uppercase headings).

One red action per screen, amber only for warnings. No Rockstar logos, fonts or trademarks as branding.
