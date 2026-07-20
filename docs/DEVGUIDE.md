# BrittLizer — Development Guide & User Manual

> **Version:** 1.0.0 · **Format:** AU / VST3 / Standalone
> **DSP Engine:** JUCE 8.0.12 · **Language:** C++17 · **Build:** CMake 3.24+
> **Author:** BrittLizer (multi-worker Kanban build) · **Target:** Apple Silicon (arm64) + x86_64 Universal 2

---

## 1. What Is BrittLizer?

BrittLizer is the **inverse of Dada Life's Sausage Fattener**. Where Sausage Fattener fattens, saturates, compresses, and glues, BrittLizer **thins, degrades, and brittlizes** — turning clean, polished audio into something harsh, lo-fi, or destructively textured.

It's a **multi-stage degradation processor** with five independent bypassable modules:

```
Input → Bit Crusher → Decimator → Distortion → Noise/Dither → Output (Filter + Mix + Gain)
```

---

## 2. Project Structure

```
brittlizer/
├── CMakeLists.txt                 # Root build (JUCE 8 FetchContent, AU+VST3+Standalone)
├── Source/
│   ├── CMakeLists.txt             # Source file list
│   ├── Params.h                   # 23 parameter IDs, ranges, defaults, display names
│   ├── PluginProcessor.h          # AudioProcessor with APVTS + 5 DSP module members
│   ├── PluginProcessor.cpp        # Full DSP pipeline in processBlock
│   ├── PluginEditor.h             # Full UI: 5 module strips with controls
│   ├── PluginEditor.cpp           # UI layout: knobs, sliders, combos, toggles
│   └── DSP/
│       ├── BitCrusher.h           # 1–32 bit uniform quantization
│       ├── Decimator.h            # Sample-rate reduction (ZOH + anti-alias IIR)
│       ├── Distortion.h           # Hard clip / Foldback / Asymmetric + 2x/4x oversampling
│       ├── NoiseGenerator.h       # TPDF, Pink, Brown, White, Crackle
│       └── OutputStage.h          # SVF LP/BP/HP filter, dry/wet mix, output gain
├── test/
│   └── test_dsp.py                # 38 Python DSP tests (cross-platform, no JUCE needed)
├── scripts/
│   └── build-macos.sh             # macOS build helper
├── docs/
│   ├── DEVGUIDE.md                # This file
│   └── macos-build-test-guide.md  # macOS build & Logic Pro X test plan
└── .gitignore
```

---

## 3. DSP Chain — Deep Dive

### 3.1 Bit Crusher (`BitCrusher.h`)

**Purpose:** Reduce amplitude resolution → quantization distortion, digital crunch.

**Algorithm (Pirkle method):**
```
maxVal = 2^(bitDepth - 1) - 1
intSample = roundToInt(sample * maxVal)
output = intSample / maxVal
```

- 32-bit = transparent (bypass equivalent)
- 16-bit = subtle (CD quality quantization)
- 8-bit = chiptune territory
- 4-bit and below = extreme quantization noise

**Parameters:** Bit Depth (1–32), Bypass toggle

### 3.2 Decimator (`Decimator.h`)

**Purpose:** Reduce temporal resolution → aliasing, lo-fi band-limited sound.

**Algorithm:** Sample-and-hold decimation. For integer factors (e.g. 44100→22050), holds every Nth sample. For non-integer factors, uses a randomized block-size selection (ALF method) for rough character.

**Anti-aliasing:** Pre-decimation (before) and post-decimation (after) low-pass filters at `cutoff = targetRate / 2`. Both independently toggleable.

**Parameters:** Sample Rate (100–48000 Hz), AA Pre toggle, AA Post toggle, Bypass toggle

### 3.3 Distortion (`Distortion.h`)

**Purpose:** Harsh/brittle harmonics — the opposite of Sausage Fattener's warm tanh.

**Three modes:**

| Mode | Behavior | Character |
|------|----------|-----------|
| Hard Clip | Hard limit at threshold | Harsh, square-wave buzz |
| Foldback | Reflect signal above threshold | Metallic, ring-mod-like |
| Asymmetric | Different clip on positive vs negative | Broken, gated, unpredictable |

**Oversampling:** 2x and 4x via JUCE half-band polyphase IIR decimator/interpolator. Reduces aliasing from nonlinear distortion.

**Parameters:** Distortion Type (combo), Drive (0–10), Threshold (0.01–1.0), Oversample (Off/2x/4x), Bypass toggle

### 3.4 Noise Generator (`NoiseGenerator.h`)

**Purpose:** Add dither, texture, and/or destructively noisy artifacts.

**Five modes:**

| Mode | Algorithm | Sound |
|------|-----------|-------|
| TPDF | Triangular Probability Density Function dither | Subtle whitish hiss, decorrelates quantization |
| Pink | Paul Kellet V-V filtered noise generator | Natural hiss, musical, lo-fi tape |
| Brown | Random-walk integrator + DC blocker | Rumble, sub-heavy |
| White | Uniform random | Classic hiss |
| Crackle | Impulse generator | Vinyl pops & clicks |

**Parameters:** Noise Mode (combo), Level (0–1), Pop Rate (0–10/s), Pop Intensity (0–1), Bypass toggle

### 3.5 Output Stage (`OutputStage.h`)

**Purpose:** Final shaping and blend.

**SVF Filter:** State-variable filter with Low/Off/Band/High Pass modes. Sweepable frequency (20–20000 Hz) and Q (0.1–10).

**Dry/Wet Mix:** 0% = dry, 100% = fully processed. Linear crossfade with 20ms ramp for click-free transitions.

**Output Gain:** -24 dB to +24 dB range.

**Parameters:** Filter Type (combo), Filter Freq, Filter Q, Mix, Output Gain

---

## 4. Parameter Reference (Full List — 23 Parameters)

| ID | Name | Type | Range | Default | Module |
|----|------|------|-------|---------|--------|
| `bitDepth` | Bit Depth | Float (stepped) | 1–32 | 16 | Bit Crusher |
| `bitEnabled` | Bit Crusher | Bool | Off/On | Off | Bit Crusher |
| `sampRate` | Sample Rate | Float | 100–48000 | 44100 | Decimator |
| `aaPre` | Anti-Alias Pre | Bool | Off/On | On | Decimator |
| `aaPost` | Anti-Alias Post | Bool | Off/On | Off | Decimator |
| `decEnabled` | Decimator | Bool | Off/On | Off | Decimator |
| `distType` | Distortion Type | Choice | Hard Clip/Foldback/Asymmetric | Hard Clip | Distortion |
| `distDrive` | Drive | Float | 0–10 | 1.0 | Distortion |
| `distThreshold` | Threshold | Float | 0.01–1.0 | 0.5 | Distortion |
| `distOversample` | Oversample | Choice | Off/2x/4x | 2x | Distortion |
| `distEnabled` | Distortion | Bool | Off/On | Off | Distortion |
| `noiseMode` | Noise Mode | Choice | TPDF/Pink/Brown/White/Crackle | Pink | Noise |
| `noiseLevel` | Noise Level | Float | 0–1 | 0.0 | Noise |
| `popRate` | Pop Rate | Float | 0–10 | 0.0 | Noise |
| `popIntensity` | Pop Intensity | Float | 0–1 | 0.5 | Noise |
| `noiseEnabled` | Noise | Bool | Off/On | Off | Noise |
| `filterType` | Filter Type | Choice | Off/Low Pass/Band Pass/High Pass | Off | Output |
| `filterFreq` | Filter Freq | Float | 20–20000 | 8000 | Output |
| `filterQ` | Filter Q | Float | 0.1–10 | 0.707 | Output |
| `mix` | Mix | Float | 0–1 | 0.5 | Output |
| `outputGain` | Output Gain | Float | -24–+24 dB | 0.0 | Output |

---

## 5. UI Layout

The editor is a dark-theme (0xFF1A1A1A background) single-window design, 760x440 px default, resizable min 600x380.

- **Title row:** "BRITTLIZER" in bold orange (orangered, 28pt)
- **5 module strips** separated by subtle 1px lines, each 76px tall:

| Strip | Controls |
|-------|----------|
| Bit Crusher | ON toggle, Bit Depth slider |
| Decimator | ON toggle, Sample Rate slider, AA Pre toggle, AA Post toggle |
| Distortion | ON toggle, Type combo, Drive slider, Threshold slider, OS combo |
| Noise | ON toggle, Mode combo, Level slider, Pops/s slider, Pop Amt slider |
| Output | Filter combo, Freq slider, Q slider, Mix slider, Gain slider |

All sliders are horizontal with right-aligned value textboxes. Color scheme: orange accent (0xCC5500) on dark grey.

---

## 6. Building

### Prerequisites (macOS)
- Xcode 15+ with Command Line Tools
- CMake 3.24+ (`brew install cmake`)
- Git

### Build
```bash
cd /path/to/brittlizer
./scripts/build-macos.sh Release
```

Or manually:
```bash
cmake -B Build -G Xcode -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
cmake --build Build --config Release
```

### Outputs
- `Build/Release/BrittLizer.component` (AU)
- `Build/Release/BrittLizer.vst3` (VST3)
- `Build/Release/BrittLizer.app` (Standalone)

See `docs/macos-build-test-guide.md` for detailed build, validation (auval, pluginval), installation to Logic Pro X, code signing, and notarization instructions.

---

## 7. Testing

### Python DSP Tests (38 tests, cross-platform)
```bash
cd /path/to/brittlizer
python3 -m pytest test/test_dsp.py -v
```

Tests cover algorithmic properties of all 5 modules + full pipeline. No JUCE or macOS required — pure Python math validation.

### AU Validation (macOS)
```bash
auval -strict -v aufx BrLz BrAo -c 2
```

### Logic Pro X Test Plan
See `docs/macos-build-test-guide.md` sections 3–5 for detailed installation, validation, and DAW test plan covering: instantiation, parameter automation, bypass, preset save/load, sample rate changes, CPU performance, and Apple Silicon native verification.

---

## 8. Known Issues

- **Latency not reported** — `getLatencySamples()` not overridden. Distortion oversampling adds ~12-24 samples. Acceptable for Logic's auto-compensation but should be explicit.
- **No CI pipeline** — No GitHub Actions configured.
- **No code signing / notarization** — Distribution-ready builds need signing.
- **No preset system** — Factory/user presets not implemented.
- **UI not pixel-perfect** — Functional layout with basic styling. Controls may overlap at extreme resize limits. Designed for iteration.

---

## 9. Architecture Decisions

| Decision | Rationale |
|----------|-----------|
| Fix module order (Crusher → Decimator → Dist → Noise → Output) | Matches lo-fi convention (RC-20, SketchCassette) and keeps signal path predictable |
| Private DSP modules (not exposed as public API) | All DSP is internal to the processor; no external reuse needed |
| All-pass for non-oversampled distortion path | Prevents phase shift accumulation in bypass mode |
| SVF filter instead of biquad | Stable under modulation, good for resonant sweeps |
| Atomic parameter caching in prepareToPlay | Lock-free realtime-safe reads on audio thread |
| Python unit tests for DSP algorithms | Cross-platform validation without JUCE/macOS toolchain |

---

*Generated by CarmyGPT on 2026-07-20 — consolidated from 6 specialist Kanban tasks.*
