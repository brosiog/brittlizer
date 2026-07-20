# BrittLizer — macOS Build & Test Guide

> **Target:** Logic Pro X (AU) + Any DAW (VST3) on Apple Silicon (arm64)
> **Stack:** JUCE 8.0.12 + CMake 3.24+ + Xcode 15+ + macOS 14+ SDK
> **Repo source:** Kanban workspace `t_b5cc5f97` / `t_9bb30f36`

---

## 1. Prerequisites

### Required tools on your Mac
- **Xcode 15+** — from Mac App Store or developer.apple.com
- **CMake 3.24+** — `brew install cmake`
- **Git** — `brew install git`

### Verify your environment
```bash
# Check Apple Silicon architecture
uname -m
# → arm64

# Check Xcode version / command line tools
xcode-select -p
xcrun --show-sdk-path
xcodebuild -version

# Check CMake
cmake --version

# Check for JUCE build cache (FETCHCONTENT downloads JUCE 8.0.12)
```

---

## 2. Build

### Option A: Use the build script

```bash
cd /path/to/BrittLizer
./scripts/build-macos.sh Release
```

This generates an Xcode project and builds Release configuration.

### Option B: Manual CMake build

```bash
cd /path/to/BrittLizer
cmake -B Build -G Xcode \
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
cmake --build Build --config Release
```

### Build outputs (Release)

| Format | Path |
|--------|------|
| AU     | `Build/Release/BrittLizer.component` |
| VST3   | `Build/Release/BrittLizer.vst3` |
| Standalone | `Build/Release/BrittLizer.app` |

### Verify Universal Binary

```bash
lipo -info Build/Release/BrittLizer.component/Contents/MacOS/BrittLizer
# Should show: Architectures in the fat file: x86_64 arm64
```

---

## 3. Logic Pro X Installation

### Automatic install
The CMake config has `COPY_PLUGIN_AFTER_BUILD=TRUE`, which copies the AU to:
```
~/Library/Audio/Plug-Ins/Components/BrittLizer.component
```
and the VST3 to:
```
~/Library/Audio/Plug-Ins/VST3/BrittLizer.vst3
```

If auto-copy didn't work, copy manually:
```bash
cp -R Build/Release/BrittLizer.component ~/Library/Audio/Plug-Ins/Components/
cp -R Build/Release/BrittLizer.vst3 ~/Library/Audio/Plug-Ins/VST3/
```

### Rescan in Logic Pro X
1. Open Logic Pro X
2. Go to **Logic Pro > Settings > Plug-in Manager**
3. Click **Rescan Selection** for the AU or VST3 category
4. Or quit and relaunch Logic (it rescans AU plugins on launch)

---

## 4. AU Validation (command-line)

### Full AU validation
```bash
auval -strict -a | grep -i brittlizer
```

### Targeted validation
```bash
# Stereo effect validation
auval -strict -v aufx BrLz BrAo -c 2

# Mono effect validation
auval -strict -v aufx BrLz BrAo -c 1
```

### Stress testing with pluginval
Install [pluginval](https://github.com/Tracktion/pluginval) for deep testing:
```bash
# Install via brew
brew install pluginval

# Basic validation
pluginval --strictness 5 --validate ~/Library/Audio/Plug-Ins/Components/BrittLizer.component

# Stress test (10 iterations)
pluginval --strictness 10 --iterations 10 --validate ~/Library/Audio/Plug-Ins/VST3/BrittLizer.vst3
```

### Common auval issues and fixes
| Error | Likely Cause | Fix |
|-------|-------------|-----|
| `error: component not found` | Wrong path or codes | Verify manufacturer code `BrAo` and plugin code `BrLz` |
| `error: validation result: failed` | Sandbox/resource issue | Check console.app for crash logs |
| `error: (-1)` | Generic failure | Run with `-v` flag for verbose output |
| `audio unit does not pass basic validation` | Missing required AU properties | Add `getSupportedProperties()` override |

---

## 5. Logic Pro X Test Plan

### 5.1 Basic Functionality

- [ ] **Instantiation** — Insert BrittLizer on an audio track. Plugin window appears without crash.
- [ ] **Parameter changes** — Adjust every parameter while audio plays. No crackles, pops, or hangs.
- [ ] **Bypass toggle** — Toggle each module's bypass (Bit Crusher On/Off, Decimator On/Off, etc.). Signal changes cleanly.
- [ ] **Master bypass** — Use Logic's built-in bypass button on the plugin slot. Clean mute/unmute.
- [ ] **Preset loading** — Save a Logic channel strip preset, reload it. Parameters restore correctly.
- [ ] **Undo/Redo** — Change parameters, undo, redo. State should be consistent.

### 5.2 Automation

- [ ] **Parameter automation** — Automate each float parameter in Logic's automation lane.
- [ ] **Read/Write/Latch/Touch modes** — Test all automation modes on Mix, Gain, Drive params.
- [ ] **No automation clicks** — Smoothed values (20ms ramps) should prevent zipper noise.

### 5.3 Audio Quality

- [ ] **Bit Crusher (1-32 bit)** — At 32-bit, signal passes unchanged. At 4-8 bit, audible quantization.
- [ ] **Decimator (100-48000 Hz)** — At 44100 Hz, bypass. At 8000 Hz, lo-fi telephone effect. At 100 Hz, extreme degradation.
- [ ] **Distortion modes** — Hard Clip (harsh), Foldback (metallic), Asymmetric (broken). Verify different characters.
- [ ] **Noise modes** — TPDF dither, Pink noise (hiss), Brown noise (rumble), White noise, Crackle (pops).
- [ ] **SVF Filter** — Low-pass, Band-pass, High-pass with sweepable frequency and Q.
- [ ] **Dry/Wet mix** — 0% = dry, 100% = wet, 50% = clean blend.
- [ ] **Output gain** — -24 dB to +24 dB range.

### 5.4 Stability

- [ ] **Sample rate changes** — Change Logic project sample rate (44.1k → 48k → 96k → 192k). No crashes.
- [ ] **Buffer size changes** — Change I/O buffer size (32 → 64 → 128 → 256 → 512 → 1024). No xruns.
- [ ] **Long playback** — Play 30 minutes of audio through the plugin. No gradual latency buildup.
- [ ] **Render to audio** — Bounce/freeze track with plugin. Output matches real-time playback.
- [ ] **Session load/save** — Save project, close, reopen. Plugin state preserved and restored.

### 5.5 CPU Performance

- [ ] **Baseline** — Note CPU % with all modules bypassed.
- [ ] **Each module** — Enable one module at a time, note CPU impact.
- [ ] **Oversampling** — Distortion with 4x oversampling is the heaviest path. Monitor CPU usage.
- [ ] **Full chain** — All 5 modules enabled. Acceptable for real-time use on M-series chips.

### 5.6 Apple Silicon Native

- [ ] **Architecture check** — `lipo -info` shows `arm64` in the binary. Activity Monitor shows "Apple" (not "Intel") in the Architecture column for Logic Pro.
- [ ] **No Rosetta** — Disable Rosetta for Logic in Finder > Get Info. Plugin should still load and function.
- [ ] **M1/M2/M3/M4 compatibility** — Test on at least one Apple Silicon generation.

---

## 6. Code Signing & Notarization (for distribution)

### Prerequisites
- Apple Developer account ($99/year)
- Developer ID Application certificate

### Sign
```bash
# Sign the AU component
codesign --force --sign "Developer ID Application: Your Name" \
    --timestamp --options runtime \
    BrittLizer.component

# Sign the VST3 bundle
codesign --force --sign "Developer ID Application: Your Name" \
    --timestamp --options runtime \
    BrittLizer.vst3
```

### Notarize
```bash
# Package into .zip
ditto -c -k --sequesterRsrc --keepParent BrittLizer.component BrittLizer.zip

# Submit for notarization
xcrun notarytool submit BrittLizer.zip \
    --apple-id your@email.com \
    --team-id YOUR_TEAM_ID \
    --password @keychain:AC_PASSWORD \
    --wait

# Staple ticket to the bundle
xcrun stapler staple BrittLizer.component
```

---

## 7. Troubleshooting

### "JUCE library not found"
First CMake build downloads JUCE 8.0.12 via FetchContent (~200MB). Ensure internet access. Check `Libs/` directory.

### Xcode generator warnings about `XCODE_ATTRIBUTE_*`
These are macOS-only CMake properties. The properties are set conditionally in the CMakeLists.txt and don't affect Linux builds. On macOS these warnings are informational only.

### AU not appearing in Logic Pro X
Run `auval -strict -v aufx BrLz BrAo -c 2` to validate. Check `~/Library/Audio/Plug-Ins/Components/` exists.

### Crackles/pops in distortion with oversampling
Try disabling oversampling (set to "Off"). 4x oversampling is CPU-intensive. Start with 2x.

### Automated DSP Tests (cross-platform)
Python DSP tests validate the core algorithms without JUCE or macOS:
```bash
cd /path/to/BrittLizer
python3 -m pytest test/test_dsp.py -v
```
38 tests covering Bit Crusher, Decimator, Distortion, Noise, Output, and Pipeline.

---

## 8. Known Issues (pre-release)

- **UI is a stub** — Current editor shows only the plugin title ("BRITTLIZER") on a dark background. Full per-module UI strips (sliders, combos, toggles as designed in BrittLizer-Design.md section 4) need implementation.
- **Latency not reported** — `getLatencySamples()` is not overridden. The Distortion module with oversampling adds ~12-24 samples of latency (from half-band FIR filters). This is typically acceptable for Logic Pro X's latency compensation but should be explicitly reported.
- **No CI pipeline** — No GitHub Actions configured. The carmy-saturation project has a working CI template that can be adapted.
- **No code signing** — Not signed or notarized. Distribution-ready builds need signing.
- **No Linux VST3 support** — CMakeLists.txt targets only macOS formats. Add `LINUX` to FORMATS for Linux VST3 builds.
- **No preset system** — Factory/user presets not implemented. JUCE's `getCurrentProgram()`/`setCurrentProgram()` can be used.

---

*Generated by CarmyGPT on 2026-07-20 — for execution on macOS with Apple Silicon.*
