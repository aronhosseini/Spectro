# Homebrew tap: put this file in a repo named  homebrew-spectro  (folder Casks/spectro.rb)
#   brew install --cask YOUR_GITHUB_USER/spectro/spectro
cask "spectro" do
  version "1.0.0"
  sha256 "REPLACE_WITH_SHA256_FROM_RELEASE"   # printed by the release workflow

  url "https://github.com/YOUR_GITHUB_USER/spectro/releases/download/v#{version}/Spectro-macOS.zip"
  name "Spectro"
  desc "Spectrogram and spectral analyzer plugin by Aron H"
  homepage "https://www.instagram.com/aronh.official"

  vst3_plugin "Spectro.vst3"
  audio_unit  "Spectro.component"

  # Unnotarized build: strip quarantine so macOS doesn't block it
  postflight do
    [
      "#{Dir.home}/Library/Audio/Plug-Ins/VST3/Spectro.vst3",
      "#{Dir.home}/Library/Audio/Plug-Ins/Components/Spectro.component",
    ].each do |p|
      system_command "/usr/bin/xattr", args: ["-dr", "com.apple.quarantine", p], must_succeed: false
      system_command "/usr/bin/codesign", args: ["--force", "--deep", "--sign", "-", p], must_succeed: false
    end
  end

  uninstall delete: [
    "~/Library/Audio/Plug-Ins/VST3/Spectro.vst3",
    "~/Library/Audio/Plug-Ins/Components/Spectro.component",
  ]
end
