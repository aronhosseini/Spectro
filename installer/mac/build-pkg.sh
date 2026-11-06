#!/usr/bin/env bash
# Usage: installer/mac/build-pkg.sh <artefacts-dir> <out-dir> <version>
# <artefacts-dir> contains Spectro.vst3, Spectro.component, Spectro.app (already ad-hoc signed).
set -euo pipefail
ART="$1"; OUT="$2"; VER="${3:-1.0.0}"
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(mktemp -d)"; trap 'rm -rf "$ROOT"' EXIT
mkdir -p "$ROOT/Library/Audio/Plug-Ins/VST3" "$ROOT/Library/Audio/Plug-Ins/Components" "$ROOT/Applications" "$OUT"
ditto "$ART/Spectro.vst3"      "$ROOT/Library/Audio/Plug-Ins/VST3/Spectro.vst3"
ditto "$ART/Spectro.component" "$ROOT/Library/Audio/Plug-Ins/Components/Spectro.component"
ditto "$ART/Spectro.app"       "$ROOT/Applications/Spectro.app"
pkgbuild --root "$ROOT" --identifier com.aronh.spectro --version "$VER" \
         --scripts "$HERE/scripts" --install-location / "$OUT/Spectro-macOS.pkg"
echo "Built $OUT/Spectro-macOS.pkg"
