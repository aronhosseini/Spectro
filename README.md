# Spectro — by [Aron H](https://www.instagram.com/aronh.official)
Spectrogram / spectral analyzer plugin (VST3, AU, Standalone). Resizable window (fixed aspect, 640→2048 px wide).

## Install
**macOS (recommended – no Gatekeeper warning)**
```bash
curl -fsSL https://raw.githubusercontent.com/YOUR_GITHUB_USER/spectro/main/installer/mac/install.sh | bash
# all users:  ... | bash -s -- --system
```
**macOS (Homebrew)**
```bash
brew tap YOUR_GITHUB_USER/spectro && brew install --cask spectro
```
**Windows:** download `Spectro-Windows-Setup.exe` from Releases.

If you downloaded the `.pkg`/`.zip` in a browser and macOS blocks it: `xattr -dr com.apple.quarantine /path/to/Spectro.*`

## Build
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --config Release
```
Release: `git tag v1.0.0 && git push --tags` → GitHub Actions builds + publishes both installers.
Replace `YOUR_GITHUB_USER` everywhere (install.sh, cask, README).
