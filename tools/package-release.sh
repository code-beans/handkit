#!/bin/sh
# Zips the built demo, starter, input self-test and device test package for a release.
# Run after building the handkit-demo-pak, handkit-minimal-pak and handkit-test-pak targets.
#   tools/package-release.sh VERSION OUTPUT_DIR
set -eu
if [ $# -ne 2 ]; then
    echo "usage: $0 VERSION OUTPUT_DIR" >&2
    exit 2
fi
version=$1
root=$(cd "$(dirname "$0")/.." && pwd)
dist="$root/build/Release/dist"
mkdir -p "$2"
out=$(cd "$2" && pwd)
staging=$(mktemp -d)
trap 'rm -rf "$staging"' EXIT

printf '%s\n' "$version" >"$dist/Handkit Test.pak/VERSION"

# zip keeps the executable bits on app, launch.sh and the scripts.
(cd "$dist" && zip -qr "$out/sdl3-demo-$version.zip" "SDL3 Demo.pak")
(cd "$dist" && zip -qr "$out/sdk-minimal-$version.zip" "SDK Minimal.pak")
tar -czf "$out/sdk-input-check-$version-linux-arm64.tar.gz" -C "$root/build/Release" sdk-input-check

# NextUI and MinUI: the .pak goes into Tools/<platform>.
mkdir -p "$staging/nextui/Tools/tg5040"
cp -R "$dist/Handkit Test.pak" "$staging/nextui/Tools/tg5040/"
cp "$root/tests/device/README.txt" "$staging/nextui/"
(cd "$staging/nextui" && zip -qr "$out/handkit-test-nextui-$version.zip" .)

# Ports menus (knulli, muOS and others): a launcher script and the package folder.
mkdir -p "$staging/ports"
cp -R "$dist/Handkit Test.pak" "$staging/ports/handkit-test"
rm "$staging/ports/handkit-test/launch.sh"
cp "$root/tests/device/Handkit Test.sh" "$root/tests/device/README.txt" "$staging/ports/"
(cd "$staging/ports" && zip -qr "$out/handkit-test-ports-$version.zip" .)

cd "$out"
if command -v sha256sum >/dev/null; then
    sha256sum -- *.zip *.tar.gz >SHA256SUMS
else
    shasum -a 256 -- *.zip *.tar.gz >SHA256SUMS
fi
ls -l "$out"
