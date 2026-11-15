#!/usr/bin/env bash
set -euo pipefail
for d in "$HOME/Library/Audio/Plug-Ins" "/Library/Audio/Plug-Ins"; do
  for f in "VST3/Spectro.vst3" "Components/Spectro.component"; do
    [ -e "$d/$f" ] && { echo "Removing $d/$f"; rm -rf "$d/$f" 2>/dev/null || sudo rm -rf "$d/$f"; }
  done
done
for a in "$HOME/Applications/Spectro.app" "/Applications/Spectro.app"; do
  [ -e "$a" ] && { echo "Removing $a"; rm -rf "$a" 2>/dev/null || sudo rm -rf "$a"; }
done
echo "Spectro removed."
