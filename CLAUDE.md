# NF De-Esser -- notes for Claude

Standalone repo for the **NF De-Esser** plug-in (NF Audio Tools). Built from the NF Glue code base (same look, same preset tab / window rules); the
look-and-feel and artwork are its own copies (`Source/UI/NFDeEsserLookAndFeel.*`, `Assets/`).

## Mandatory: version consistency
Whenever the version changes it MUST be right everywhere. **Single source of truth: `CMakeLists.txt` line 2**
(`project(NFDeEsser VERSION X.Y.Z ...)`). Everything else derives from it automatically:
- plugin UI footer "V1.0.0" (bottom-left) reads `JucePlugin_VersionString`;
- `Installer/macos/build_dmg.sh` and `Installer/Windows/build_windows_installer.ps1` read it from `CMakeLists.txt`
  (the Windows script passes it to Inno Setup as `/DMyAppVersion`; `NFDeEsser.iss` refuses to build without it).
After a bump, grep for the old version to catch anything missed:
`grep -rn "OLD_VERSION" --include="*.cpp" --include="*.h" --include="*.iss" --include="*.sh" --include="*.ps1" --include="*.txt" .`

## Build & release rules (owner's decisions)
- **No GitHub Actions / no uploads.** Installers are built locally: DMG on the owner's Mac, `.exe` on the partner's
  Windows PC. Never commit `.dmg`, `.exe`, `.pkg` or plug-in bundles (git-ignored).
- **macOS DMG = a DMG that contains a standard installer `.pkg`** (`Installer/macos/build_dmg.sh`): English
  Welcome / Read Me / Conclusion, a Customize step to pick VST3, AU and AAX, "NF Audio Tools by Nenno Fernando".
  **No Apple Developer account for now**: VST3/AU are ad-hoc signed and the `.pkg` is unsigned (Gatekeeper: right-click > Open
  on other Macs). macOS cannot auto-launch an installer from a DMG, so the DMG opens a clean Finder window with only
  "Install NF De-Esser X.Y.Z.pkg". PACE account for signing: `nenofernando` (`WRAP_ACCOUNT`); `wraptool sign` also needs `--signid` (the wrap has "Digitally sign binary"), the script passes `WRAP_SIGNID` (auto: the owner's local keychain certificate "NF Audio Tools AAX Local Signing" if present, else `-` ad-hoc); the AAX SDK is auto-detected in `~/Documents`.
- **AAX must be in the installers and PACE-signed** (`wraptool`). The NF De-Esser wrap GUID is NOT known yet: pass `WRAP_GUID=...`
  (no default; the scripts stop early without it unless `SKIP_AAX=1`). AAX SDK is expected in `~/Documents/AAX_SDK`
  (`AAX_SDK_PATH` overrides). `SKIP_AAX=1` is the only way to build without it. Never re-run `codesign` on a
  signed `.aaxplugin`. Never commit the AAX SDK, passwords or certificates.
- Identifiers: bundle `com.nfaudiotools.nfdeesser`, manufacturer `Nfat`, plug-in code `Nfde`.
- Presets: `.nfdeesserpreset` in `Documents/NF Audio Tools/NF De-Esser/Presets`; 11 factory presets in `Source/FactoryPresets.h`.

## Design notes
- UI is a fixed 1200x400 layout scaled uniformly; keep the approved look (do not add controls unasked). Owner's design (modelled on a classic de-esser layout he showed): left column AUDIO (Split/Wide) / FREQUENCY (value box) / MONITOR (Audio, Listen); Threshold, Range and Output are VERTICAL FADERS (component id "fader", console-style cream cap); Threshold has an input meter beside it (detector level, -40..0 dB, same range as the fader), ATTEN is the reduction meter, Output has L/R peak meters (-30..0 dB). Fader scale marks are drawn from `Slider::getPositionOfValue` so they always line up; the meters span the same travel as the thumb (`kThumbMargin`).
- DSP is JUCE-free in `Source/DSP/DeEsser.h` and covered by `Tests/DeEsserTests.cpp`.
- Split-band: a band-pass (Q 1.4) at Frequency is both the detector (stereo-linked peak envelope, 0.4 ms attack / 40 ms release) and the
  part that is turned down: `y = x + (g-1) * band`. Soft knee 6 dB, effective ratio 4:1, reduction never above Range. Listen outputs the band.
  Power off = untouched input. No latency, no oversampling.
- Parameters: `freq` 2-12 kHz (centre 5 kHz), `threshold`, `range`, `outputGain` -12..+12, `listen`, `wide` (false = Split, true = Wide), `power`. The processor publishes `gainReductionDb`, `detectorLevelDb` and `outputLevelDb[2]` for the meters.
- No licence system yet (NF Q3 has one); decide before selling.

- UI: NF De-Esser is the one SQUARE plug-in of the family (owner's design): 800 x 800 base layout, default window 540 x 540 (the same 0.675 text scale as the 810 x 270 plug-ins), aspect 1:1, limits 405..1200. Its chassis is `Assets/PNG_READY_800x800/01_chassis_800x800.png`, built from the family chassis texture. The size chosen with the resize handle is stored in the plug-in state (`uiWidth`) and restored when the window is reopened; double-click on the NF logo returns to 540 x 540. Preset tab (prev / name / next) at the top right; the 3-line button holds About.
