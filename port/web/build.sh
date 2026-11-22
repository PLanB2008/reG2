#!/bin/sh
# build.sh <deps prefix> <build dir> <site dir>: the web build (emsdk's environment needed) and the site around it,
# ready to serve from <site dir>. The deps prefix is deps.sh's.
set -eu
P=$1; B=$2; OUT=$3
HERE=$(cd "$(dirname "$0")" && pwd)
PORT=$(cd "$HERE/.." && pwd)
ROOT=$(cd "$PORT/.." && pwd)

emcmake cmake -S "$PORT" -B "$B" -G Ninja -DCMAKE_BUILD_TYPE=Release -DGTA2_SETUP_DEPS_DIR="$P" \
    -DCMAKE_PREFIX_PATH="$P" -DCMAKE_FIND_ROOT_PATH="$P" ${GTA2_VERSION:+-DGTA2_VERSION=$GTA2_VERSION}
ninja -C "$B" gta2 reg2_import

rm -rf "$OUT"
mkdir -p "$OUT/icons"
cp -R "$HERE/site/." "$OUT/"
cp "$B/reg2.js" "$B/reg2.wasm" "$B/reg2_import.js" "$B/reg2_import.wasm" "$OUT/"
cp "$ROOT/docs/images/reg2_logo_black.svg" "$OUT/logo-dark.svg"
# The site's design system, beside the page's fonts so that the service worker keeps it for offline starts.
cp "$HERE/ds.css" "$OUT/"
cp "$PORT/resources/reg2_256.png" "$PORT/resources/reg2_512.png" "$OUT/icons/"

# The licences of what the site serves.
{
    echo "reG2's web build. The port's code (port/) is under the MIT licence; the game code comes from gta2_re."
    for f in "$PORT/LICENSE" "$P/licenses/"* "$OUT/vendor/"*.LICENSE "$OUT/fonts/"*.txt; do
        [ -f "$f" ] || continue
        printf '\n==== %s\n\n' "$(basename "$f")"
        cat "$f"
    done
} > "$OUT/licenses.txt"
rm -f "$OUT/vendor/"*.LICENSE "$OUT/fonts/"*.txt

# The service worker caches this build's files under a version made from their contents.
VERSION=$(cd "$OUT" && find . -type f ! -name sw.js | sort | xargs cat | sha256sum | cut -c1-12)
FILES=$(cd "$OUT" && find . -type f ! -name sw.js | sort | sed 's|^\./||' | awk 'BEGIN{printf "["} {printf "%s\"%s\"", (NR>1?",":""), $0} END{printf "]"}')
sed -i "s|@VERSION@|$VERSION|; s|@FILES@|$FILES|" "$OUT/sw.js"
echo "site: $OUT (version $VERSION)"
