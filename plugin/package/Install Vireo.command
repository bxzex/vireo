#!/bin/bash
# Installs Vireo for the current user. Double-click this file.
# The plugins are not notarised by Apple, so macOS quarantines them when they are
# downloaded. This copies them into your plug-in folders and lifts that quarantine.
cd "$(dirname "$0")"
set -e
mkdir -p ~/Library/Audio/Plug-Ins/Components ~/Library/Audio/Plug-Ins/VST3
rm -rf ~/Library/Audio/Plug-Ins/Components/Vireo.component ~/Library/Audio/Plug-Ins/VST3/Vireo.vst3
cp -R Vireo.component ~/Library/Audio/Plug-Ins/Components/
cp -R Vireo.vst3 ~/Library/Audio/Plug-Ins/VST3/
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/Vireo.component ~/Library/Audio/Plug-Ins/VST3/Vireo.vst3 2>/dev/null || true
if [ -d "Vireo.app" ]; then rm -rf /Applications/Vireo.app 2>/dev/null; cp -R Vireo.app /Applications/ 2>/dev/null && xattr -dr com.apple.quarantine /Applications/Vireo.app 2>/dev/null || true; fi
killall -9 AudioComponentRegistrar 2>/dev/null || true
echo
echo "Vireo is installed."
echo "  AU:   ~/Library/Audio/Plug-Ins/Components/Vireo.component  (Logic Pro, GarageBand, Ableton)"
echo "  VST3: ~/Library/Audio/Plug-Ins/VST3/Vireo.vst3             (Ableton, FL Studio, Bitwig, Reaper)"
echo "  App:  /Applications/Vireo.app"
echo "Rescan plug-ins in your DAW if it was already open."
