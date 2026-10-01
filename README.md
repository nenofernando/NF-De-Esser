# NF De-Esser

Simple split-band de-esser by **NF Audio Tools** (Nenno Fernando) for vocals, drums and mixes. VST3, AU (macOS) and AAX (Pro Tools).

Built like the classic de-essers (Waves DeEsser, Avid De-Esser III), in the NF look with a **black brushed-steel** chassis (the owner's choice for this plug-in).
- **MODE:** **TARGET** (Split: the audio is split at Frequency and only the HIGH part is turned down, the low part is never touched) or **FULL** (the whole signal is turned down).
- **SIDE-CHAIN:** **HIGH** (default: the detector looks at everything above Frequency, good for voices with several kinds of "s") or **BAND** (a narrow band around Frequency, isolates one kind of "s").
- **RANGE** (0..20 dB: the most it will ever turn down) and **FREQUENCY** (500 Hz - 16 kHz): round knobs with a value box.
- **MONITOR:** **AUDIO** or **LISTEN**, the side-chain monitor: it always plays what the side-chain hears (to find the "s"), whatever the Threshold is.
- **THRESHOLD** (-40..0 dB) is the only fader, with an input meter beside it (the energy of the side-chain, same scale); then the **REDUCTION** meter and L / R output level meters.
  There is no output gain control. The Threshold scale is calibrated so an "s" reads in the upper-middle of the scale.
- Power and 13 factory presets (voices male / female, "s" and "sh", wide, rock, podcast, acoustic guitar, hi-hat, overheads, mix bus, master) plus save/load.
- Look-ahead of 1.5 ms (reported to the host as latency, also with Power off) so the reduction is already in place when an "s" starts.

![preview](Docs/preview.png)

## Build (local, no CI)
- macOS DMG (VST3 + AU + PACE-signed AAX): `bash Installer/macos/build_dmg.sh`
- Windows installer (VST3 + PACE-signed AAX): `.\Installer\Windows\build_windows_installer.ps1`

See `CLAUDE.md` for the rules (version, AAX SDK, signing).

## Tests
```
g++ -std=c++17 -Wall -Wextra Tests/DeEsserTests.cpp -o dsp_tests && ./dsp_tests
```

This plug-in is **square** (owner's design): it opens at 540 x 540 (resizable, always 1:1) and remembers the size you choose with the resize handle; click the NF logo to go back to 540 x 540. A small preset tab (prev / name / next) sits at the top right; the 3-line button holds About.
