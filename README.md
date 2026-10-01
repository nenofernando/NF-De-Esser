# NF De-Esser

Simple split-band de-esser by **NF Audio Tools** (Nenno Fernando) for vocals, drums and mixes. VST3, AU (macOS) and AAX (Pro Tools).

Controls: **Frequency** (2-12 kHz: where the sibilance lives), **Threshold** (-40..0 dB), **Range** (0..20 dB: the most it will ever turn the
band down), **Output** (-12..+12 dB), a **Listen** button (plays only the band the de-esser hears, to find the "s"), a **Reduction** meter,
Power and 11 factory presets (male / female / rock / pop / podcast / heavy voice, acoustic guitar, hi-hat, overheads, mix bus, master) plus save/load.

Only the band around Frequency is turned down; everything else passes untouched (no latency).

![preview](Docs/preview.png)

## Build (local, no CI)
- macOS DMG (VST3 + AU + PACE-signed AAX): `bash Installer/macos/build_dmg.sh`
- Windows installer (VST3 + PACE-signed AAX): `.\Installer\Windows\build_windows_installer.ps1`

See `CLAUDE.md` for the rules (version, AAX SDK, signing).

## Tests
```
g++ -std=c++17 -Wall -Wextra Tests/DeEsserTests.cpp -o dsp_tests && ./dsp_tests
```

Opens at 810 x 270 and remembers the size you choose with the resize handle; double-click the NF logo to go back to the default. A small preset tab (prev / name / next) sits at the top right; the 3-line button holds About.
