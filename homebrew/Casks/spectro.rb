# Put this file in a tap repo named  aronh-official/homebrew-spectro  under Casks/spectro.rb
#   brew install --cask aronh-official/spectro/spectro
cask "spectro" do
  version :latest
  sha256 :no_check

  url "https://github.com/aronh-official/spectro/releases/latest/download/Spectro-macOS.zip"
  name "Spectro"
  desc "Spectrogram and spectral analyzer plugin by Aron H"
  homepage "https://www.instagram.com/aronh.official"

  vst3_plugin "Spectro.vst3"
  audio_unit_plugin "Spectro.component"
  app "Spectro.app"

  # The plugin is only ad-hoc signed (not notarized), so remove the quarantine flag after install.
  postflight do
    [
      "#{Dir.home}/Library/Audio/Plug-Ins/VST3/Spectro.vst3",
      "#{Dir.home}/Library/Audio/Plug-Ins/Components/Spectro.component",
      "#{appdir}/Spectro.app",
    ].each do |path|
      next unless File.exist?(path)

      system_command "/usr/bin/xattr",
                     args: ["-dr", "com.apple.quarantine", path],
                     must_succeed: false
    end
  end

  uninstall quit: "com.aronh.spectro"
end
