#!/usr/bin/env bash
# Spectro by Aron H — macOS installer (Gatekeeper-friendly)
#
#   curl -fsSL https://raw.githubusercontent.com/YOUR_GITHUB_USER/spectro/main/installer/mac/install.sh | bash
#   ...add "-s -- --system" after "bash" to install for all users (asks for sudo).
#
# Why this works: files fetched with curl never get the com.apple.quarantine flag,
# and we also strip it explicitly + ad-hoc sign, so macOS does not block the plugin.
set -euo pipefail

REPO="${SPECTRO_REPO:-YOUR_GITHUB_USER/spectro}"
VERSION="${SPECTRO_VERSION:-latest}"
SCOPE="user"; [[ "${1:-}" == "--system" ]] && SCOPE="system"

if [[ "$(uname)" != "Darwin" ]]; then echo "This installer is for macOS only." >&2; exit 1; fi

if [[ "$VERSION" == "latest" ]]; then
  URL="https://github.com/${REPO}/releases/latest/download/Spectro-macOS.zip"
else
  URL="https://github.com/${REPO}/releases/download/${VERSION}/Spectro-macOS.zip"
fi

if [[ "$SCOPE" == "system" ]]; then BASE="/Library/Audio/Plug-Ins"; SUDO="sudo"; else BASE="$HOME/Library/Audio/Plug-Ins"; SUDO=""; fi
VST3_DIR="$BASE/VST3"; AU_DIR="$BASE/Components"

TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
echo "▸ Downloading Spectro…"
curl -fL --progress-bar "$URL" -o "$TMP/Spectro.zip"
ditto -x -k "$TMP/Spectro.zip" "$TMP/out"

echo "▸ Installing to $BASE"
$SUDO mkdir -p "$VST3_DIR" "$AU_DIR"
for item in "Spectro.vst3:$VST3_DIR" "Spectro.component:$AU_DIR"; do
  src="$TMP/out/${item%%:*}"; dst="${item##*:}"
  [[ -e "$src" ]] || continue
  $SUDO rm -rf "$dst/${item%%:*}"
  $SUDO ditto "$src" "$dst/${item%%:*}"
  # remove quarantine + ad-hoc sign (required on Apple Silicon)
  $SUDO xattr -dr com.apple.quarantine "$dst/${item%%:*}" 2>/dev/null || true
  $SUDO codesign --force --deep --sign - "$dst/${item%%:*}" >/dev/null 2>&1 || true
done

if [[ -d "$TMP/out/Spectro.app" ]]; then
  APPDIR="/Applications"; [[ "$SCOPE" == "user" ]] && APPDIR="$HOME/Applications" && mkdir -p "$APPDIR"
  $SUDO rm -rf "$APPDIR/Spectro.app"; $SUDO ditto "$TMP/out/Spectro.app" "$APPDIR/Spectro.app"
  $SUDO xattr -dr com.apple.quarantine "$APPDIR/Spectro.app" 2>/dev/null || true
  $SUDO codesign --force --deep --sign - "$APPDIR/Spectro.app" >/dev/null 2>&1 || true
fi

# make AU visible to hosts straight away
killall -9 AudioComponentRegistrar 2>/dev/null || true
echo "✓ Spectro installed. Rescan plugins in your DAW."
