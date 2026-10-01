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
- **AAX must be in the installers and PACE-signed** (`wraptool`). Wrap "NF De-Esser - Signing Only" (PACE product `NFDEESSER001`), Wrap GUID `A3705280-BD40-11F1-B096-005056920FF7` (the default of both installer scripts). AAX SDK is expected in `~/Documents/AAX_SDK`
  (`AAX_SDK_PATH` overrides). `SKIP_AAX=1` is the only way to build without it. Never re-run `codesign` on a
  signed `.aaxplugin`. Never commit the AAX SDK, passwords or certificates.
- Identifiers: bundle `com.nfaudiotools.nfdeesser`, manufacturer `Nfat`, plug-in code `Nfde`.
- Presets: `.nfdeesserpreset` in `Documents/NF Audio Tools/NF De-Esser/Presets`; 13 factory presets in `Source/FactoryPresets.h`.

## Design notes
- UI is a fixed 1200x400 layout scaled uniformly; keep the approved look (do not add controls unasked). Owner's design (modelled on a classic de-esser layout he showed): left column MODE (TARGET / FULL button; the owner chose these names, do not call them Split/Wide) / SIDE-CHAIN (HIGH / BAND button) / RANGE (round knob + value box) / FREQUENCY (round knob + value box) / MONITOR (Audio, Listen); Threshold is the ONLY fader (component id "fader", silver ribbed cap with a black centre line); it has an input meter beside it (detector level, -40..0 dB, same range as the fader), the REDUCTION meter (do not call it ATTEN: too close to another maker's wording), then L/R output peak meters (-30..0 dB). The owner does NOT want an Output gain control (only Frequency, Range and Threshold). Fader scale marks are drawn from `Slider::getPositionOfValue` so they always line up; the meters span the same travel as the thumb (`kThumbMargin`).
- DSP is JUCE-free in `Source/DSP/DeEsser.h` and covered by `Tests/DeEsserTests.cpp`.
- Architecture = the classic de-essers (Waves DeEsser manual and Avid De-Esser III manual, both supplied by the owner): a SIDE-CHAIN (filter, energy
  detector, compressor) drives an AUDIO path.
  - Side-chain filter: HIGH-PASS (4th order Linkwitz-Riley at Frequency, default, "looks at" everything above it) or BAND-PASS (narrow 4th order, Q 1.8 per stage).
    Stereo-linked peak envelope (0.4 ms attack / 18 ms release), calibrated scale (`kDetectorGainDb` +16), nearly hard knee (2 dB), `kSlope` 0.9 (10:1), never above Range.
  - Audio, TARGET (= Waves "Split" / Avid "HF Only"): Linkwitz-Riley 4th-order crossover at Frequency x `kCrossRatio` (0.65): `y = low + g * high`. The low part is
    never touched. 0.65 was chosen by measuring on a real vocal: the real reduction of the "s" then matches what the meter shows. FULL (= "Wideband"): `y = x * g`.
    (An earlier bell-shaped version only removed about 0.7 dB of a real "s" while the meter showed 8 dB: the owner heard no difference when moving Threshold.)
  - LISTEN is the SIDE-CHAIN MONITOR (what the side-chain hears: high-pass = everything above Frequency, band-pass = the band), `tanh(x * gain)`, ALWAYS audible,
    independent of Threshold (a version that played only what is removed went silent and was wrong: in the Waves/Avid de-essers, sweeping Frequency into the mids keeps
    playing that region).
  Power off = untouched input. No latency, no oversampling.
- Parameters: `freq` 500 Hz - 16 kHz (centre 3 kHz; the Avid De-Esser III spans the same), `threshold`, `range`, `listen`, `full` (false = TARGET/Split, only the highs; true = FULL, whole signal; the DSP field is still called `wide`), `sidechainHigh` (true = HIGH-PASS side-chain, false = BAND-PASS), `power`. The processor publishes `gainReductionDb`, `detectorLevelDb` and `outputLevelDb[2]` for the meters.
- No licence system yet (NF Q3 has one); decide before selling.

- UI: NF De-Esser is the one SQUARE plug-in of the family (owner's design): 800 x 800 base layout, default window 540 x 540 (the same 0.675 text scale as the 810 x 270 plug-ins), aspect 1:1, limits 405..1200. Its chassis is `Assets/PNG_READY_800x800/01_chassis_800x800.png`: BLACK BRUSHED STEEL (owner's choice; the other NF plug-ins are green), made from the family chassis texture by mapping its luminance to a dark steel ramp (frame slightly darkened). UI colours that were green-tinted (meter segments, backgrounds) are neutral dark grey. The size chosen with the resize handle is stored in the plug-in state (`uiWidth`) and restored when the window is reopened; a click on the NF logo (single or double) returns to 540 x 540. Preset tab (prev / name / next) at the top right; the 3-line button holds About.

## Calibration and speed (owner compared with Waves and Avid de-essers)
- Threshold scale is CALIBRATED: the detector reading has `kDetectorGainDb` (+16 dB) added. On a real vocal an "s" read -18..-23 dB raw (so the useful
  Threshold zone sat at the very bottom of -40..0); now it reads about -5..0 and the vowels about -22..-27, so the working Threshold is -16..-20.
  Presets are on this scale.
- LOOK-AHEAD `kLookAheadMs` 1.5 ms: the detector sees the signal before the audio is heard, so the reduction is in place when an "s" starts. The delay is
  reported as latency (`setLatencySamples`) and Power off uses `bypassSample` with the same delay.
- Reference: Avid De-Esser III manual (owner supplied pages): Frequency 500 Hz - 16 kHz, Range -40..0 dB, "HF Only" (reduce only above Frequency) vs whole
  signal, Listen = monitor the sibilant side-chain, gain-reduction meter light orange / dark orange when the Range maximum is reached.
