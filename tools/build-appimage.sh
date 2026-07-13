#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${BUILD_DIR:-$root/build-appimage}"
appdir="${SAY_COUNT_APPDIR:-$root/dist/SayCount.AppDir}"
output="${SAY_COUNT_OUTPUT:-$root/dist/Say_Count-x86_64.AppImage}"

cmake -S "$root" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build "$build_dir" --target say-count -j"${JOBS:-2}"
rm -rf "$appdir"
DESTDIR="$appdir" cmake --install "$build_dir" --prefix /usr

if [[ "${1:-}" == "--appdir-only" ]]; then
    printf 'Staged %s\n' "$appdir"
    exit 0
fi

linuxdeploy_bin="${LINUXDEPLOY:-$(command -v linuxdeploy || true)}"
appimagetool_bin="${APPIMAGETOOL:-$(command -v appimagetool || true)}"
if [[ -z "$linuxdeploy_bin" || -z "$appimagetool_bin" ]]; then
    echo "linuxdeploy and appimagetool are required (or set LINUXDEPLOY and APPIMAGETOOL)." >&2
    exit 2
fi

"$linuxdeploy_bin" --appdir "$appdir" \
    --desktop-file "$root/resources/say-count.desktop" \
    --icon-file "$root/resources/say-count.svg"
ARCH=x86_64 "$appimagetool_bin" "$appdir" "$output"
printf 'Created %s\n' "$output"
