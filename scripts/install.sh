#!/usr/bin/env bash
# Spectro installer for macOS (Gatekeeper-friendly).
#   curl -fsSL https://raw.githubusercontent.com/aronh-official/spectro/main/scripts/install.sh | bash
# Options (env vars): SPECTRO_REPO=owner/repo  SPECTRO_VERSION=v1.0.0  SPECTRO_SYSTEM=1 (install for all users, needs sudo)
set -euo pipefail

REPO="${SPECTRO_REPO:-aronh-official/spectro}"
VERSION="${SPECTRO_VERSION:-latest}"
ASSET="Spectro-macOS.zip"

[ "$(uname -s)" = "Darwin" ] || { echo "This installer is for macOS only." >&2; exit 1; }

if [ "$VERSION" = "latest" ]; then
  BASE="https://github.com/$REPO/releases/latest/download"
else
  BASE="https://github.com/$REPO/releases/download/$VERSION"
fi

if [ "${SPECTRO_SYSTEM:-0}" = "1" ]; then
  PLUG="/Library/Audio/Plug-Ins"; APPS="/Applications"; SUDO="sudo"
else
  PLUG="$HOME/Library/Audio/Plug-Ins"; APPS="$HOME/Applications"; SUDO=""
fi

TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
echo "==> Downloading Spectro ($VERSION)"
# curl does not set the quarantine flag, so files fetched this way are not blocked by Gatekeeper.
curl -fsSL "$BASE/$ASSET" -o "$TMP/$ASSET"
if curl -fsSL "$BASE/$ASSET.sha256" -o "$TMP/$ASSET.sha256" 2>/dev/null; then
  echo "==> Verifying checksum"
  ( cd "$TMP" && shasum -a 256 -c "$ASSET.sha256" )
fi

echo "==> Unpacking"
ditto -x -k "$TMP/$ASSET" "$TMP/pkg"

install_bundle () {  # $1 = bundle name, $2 = destination dir
  [ -e "$TMP/pkg/$1" ] || return 0
  $SUDO mkdir -p "$2"
  $SUDO rm -rf "$2/$1"
  $SUDO ditto "$TMP/pkg/$1" "$2/$1"
  # remove quarantine + re-apply an ad-hoc signature so the (arm64) binary is valid after the copy
  $SUDO xattr -dr com.apple.quarantine "$2/$1" 2>/dev/null || true
  $SUDO codesign --force --deep --sign - "$2/$1" >/dev/null 2>&1 || true
  echo "    installed $2/$1"
}

echo "==> Installing"
install_bundle "Spectro.vst3"      "$PLUG/VST3"
install_bundle "Spectro.component" "$PLUG/Components"
install_bundle "Spectro.app"       "$APPS"

echo
echo "Done. Rescan plugins in your DAW. (AU: you may need to run 'killall -9 AudioComponentRegistrar')"
