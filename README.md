# BrittLizer

**The Anti-Sausage Fattener** — degrade, dither, and distort your audio.

BrittLizer is a multi-stage degradation AU/VST3 plugin for macOS (Apple Silicon + Intel). It does the opposite of Dada Life's Sausage Fattener: instead of fattening and gluing, it thins, brittlizes, and destroys.

## Quick Start

1. Open in Xcode: `cmake -B Build -G Xcode && open Build/BrittLizer.xcodeproj`
2. Or build: `./scripts/build-macos.sh Release`
3. Install: `cp -R Build/Release/BrittLizer.component ~/Library/Audio/Plug-Ins/Components/`
4. Validate: `auval -strict -v aufx BrLz BrAo -c 2`

## DSP Pipeline

```
Input → Bit Crusher (1-32 bit) → Decimator (100-48000 Hz)
     → Distortion (Hard Clip / Foldback / Asymmetric)
     → Noise/Dither (TPDF / Pink / Brown / White / Crackle)
     → Output (SVF Filter + Dry/Wet Mix + Gain)
```

All 5 modules independently bypassable. 23 parameters total.

## Test (cross-platform, no JUCE needed)

```bash
python3 -m pytest test/test_dsp.py -v   # 38 tests
```

## Project Structure

| Path | Content |
|------|---------|
| `docs/DEVGUIDE.md` | Full development guide & user manual |
| `docs/BrittLizer-Design.md` | Original design document |
| `docs/macos-build-test-guide.md` | macOS build & Logic Pro X test plan |
| `Source/DSP/` | 5 header-only DSP modules |
| `Source/PluginProcessor.*` | AudioProcessor with full DSP pipeline |
| `Source/PluginEditor.*` | Dark-theme UI with 5 module strips |

## Known Issues

See `docs/DEVGUIDE.md` §8.

---

*Builds on macOS only. Requires Xcode 15+, CMake 3.24+, and JUCE 8.0.12 (auto-fetched).*
