#!/bin/bash
# Package the full personal edition with the official shared Qt kit.
set -euo pipefail
cd "$(dirname "$0")/.."
arch="${1:?Usage: package-personal-macos.sh arm64|x64}"
unset SNOW_SHOT_PERSONAL_ARM64_PACKAGE
case "$arch" in
    arm64) expected_arch=arm64; export SNOW_SHOT_PERSONAL_ARM64_PACKAGE=1 ;;
    x64) expected_arch=x86_64 ;;
    *) echo 'Unsupported architecture' >&2; exit 1 ;;
esac
[[ "$(uname -s)" == Darwin && "$(uname -m)" == "$expected_arch" ]] || {
    echo 'Package and validate on the matching native macOS host.' >&2; exit 1;
}
build_dir="$PWD/build/snow-shot-macos-$arch-performance"
cache="$build_dir/CMakeCache.txt"
grep -Fqx 'CMAKE_BUILD_TYPE:STRING=Release' "$cache"
grep -Fqx 'SNOW_APPS_BUILD_SNOW_SHOT_MINI:BOOL=OFF' "$cache"
grep -Fqx 'SNOW_APPS_PACKAGE_SNOW_SHOT:BOOL=ON' "$cache"
cmake -P "$build_dir/snow_shot/GenerateSnowShotDiagnosticsSymbols-Release.cmake"
cpack --config "$build_dir/CPackConfig.cmake" -C Release
packages=("$build_dir"/xiaomage-screenshot-*-macos-"$expected_arch".dmg)
[[ ${#packages[@]} == 1 && -f "${packages[0]}" ]] || {
    echo 'Expected exactly one personal DMG.' >&2; exit 1;
}
dmg="${packages[0]}"
mount_dir="$(mktemp -d "${TMPDIR:-/tmp}/xiaomage-package.XXXXXX")"
mounted=0
cleanup() {
    if [[ "$mounted" == 1 ]]; then hdiutil detach "$mount_dir"; fi
    rmdir "$mount_dir"
}
trap cleanup EXIT
hdiutil verify "$dmg"
hdiutil attach "$dmg" -readonly -nobrowse -mountpoint "$mount_dir"
mounted=1
app="$mount_dir/小马哥专用截图工具.app"
[[ -f "$app/Contents/Info.plist" ]]
codesign --verify --deep --strict "$app"
lipo -verify_arch "$expected_arch" "$app/Contents/MacOS/snow_shot"
"$app/Contents/MacOS/snow_shot" --startup-probe
"$app/Contents/MacOS/snow_shot" --distribution-probe
[[ -d "$app/Contents/Resources/snow-shot/licenses/third-party/qt" ]]
[[ -d "$app/Contents/Resources/snow-shot/licenses/third-party/vcpkg" ]]
[[ -d "$app/Contents/Resources/snow-shot/licenses/third-party/cargo" ]]
mkdir -p dist/personal-macos
cp "$dmg" dist/personal-macos/
cp "$build_dir/macos-ocr-verification.json" "dist/personal-macos/ocr-verification-$arch.json"
python3 - "$arch" "dist/personal-macos/build-info-$arch.json" "$app" "$dmg" "$build_dir" <<'PY'
import json
import os
import platform
import subprocess
import sys
from pathlib import Path

def output(*args):
    return subprocess.check_output(args, text=True).strip()

information = {
    "product": "小马哥专用截图工具",
    "architecture": sys.argv[1],
    "commit": output("git", "rev-parse", "HEAD"),
    "macOSMinimum": "15.0",
    "buildHost": platform.platform(),
    "qt": "6.12.0 (shared, bundled)",
    "rust": output("rustc", "--version"),
    "signing": "ad-hoc; not Apple notarized",
    "edition": "full",
    "startupProbe": "passed",
    "bundleSignature": "verified",
    "dmgBytes": Path(sys.argv[4]).stat().st_size,
}
app = Path(sys.argv[3])
app_bytes = 0
for root, directories, files in os.walk(app, followlinks=False):
    root = Path(root)
    directories[:] = [name for name in directories if not (root / name).is_symlink()]
    app_bytes += sum((root / name).stat().st_size for name in files
                     if not (root / name).is_symlink() and (root / name).is_file())
information['installedAppBytes'] = app_bytes
if sys.argv[1] == 'arm64':
    information['packageOptimization'] = json.loads(
        (Path(sys.argv[5]) / 'personal-macos-package-size.json').read_text())
Path(sys.argv[2]).write_text(json.dumps(information, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
PY
(
    cd dist/personal-macos
    shasum -a 256 "$(basename "$dmg")" > "$(basename "$dmg").sha256"
)
