# BrittLizer — Design Document

> **Project:** BrittLizer — The Anti-Sausage Fattener
> **Date:** 2026-07-20
> **Status:** Draft Design
> **Build Target:** Logic Pro X (AU) + any DAW (VST3) on Apple Silicon (arm64)

---

## 1. Overview

BrittLizer does the **opposite** of Sausage Fattener. Where Sausage Fattener fattens, saturates, compresses, and glues, BrittLizer **thins, degrades, and brittlizes** — turning clean, polished audio into something harsh, lo-fi, or destructively textured.

The plugin is a **multi-stage degradation processor** with five independent modules that can be chained and blended:

```
Input → [Bit Depth Reducer] → [Sample Rate Decimator] → [Distortion] 
       → [Dither/Noise Layer] → [Output Filter] → [Dry/Wet Mix] → Output
```

Each module can be bypassed independently, and the order is fixed (a common signal-flow topology in lo-fi plugins like RC-20, SketchCassette, Krush2, and ALF).

---

## 2. DSP Module Designs

### 2.1 Bit Depth Reducer (Bit Crusher)

**Purpose:** Reduce the amplitude resolution of the signal, creating quantization distortion and the classic "digital" crunch.

**Algorithm (Pirkle method, as used in ALF):**
```
maxVal = 2^(bitDepth - 1) - 1
intSample = roundToInt(sample * maxVal)
output = intSample / maxVal
```

At 16-bit (CD quality) the effect is subtle-to-inaudible. At 8-bit it's chiptune territory. At 4-bit and below, extreme quantization noise dominates.

**Key consideration:** Quantization without dither produces correlated distortion harmonics (especially audible at low bit depths). The dither module (2.4) is positioned *after* this stage to optionally mask those artifacts — but because this is a *degradation* plugin, the raw quantization may be desirable.

**Parameter:**
| Param | Type | Range | Default | Description |
|-------|------|-------|---------|-------------|
| Bit Depth | Int | 1–32 | 16 | Target bit depth. 1 = extreme, 32 = bypass |
| Bit Depth On/Off | Toggle | Off/On | Off | Bypass this module |

**Reference:** Will Pirkle, *Designing Audio Effect Plugins in C++*, 2019 [src:pirkle-dap]; Chris Relyea, "ALF: Creating a Lo-fi Effects Plugin Using C++ and JUCE", 2025 [src:alf-paper]

---

### 2.2 Sample Rate Decimator (Downsampler)

**Purpose:** Reduce temporal resolution by lowering the effective sample rate, producing aliasing artifacts and the characteristic "lo-fi" band-limited sound.

**Algorithm (sample-and-hold decimation):**
```
N = floor(originalSampleRate / targetSampleRate)
output = sample[floor(index / N) * N]  // hold every Nth sample
```

For **integer factors** (e.g., 44.1kHz → 22.05kHz, N=2): simple sample-and-hold, keeping every Nth sample and holding it in place.

For **non-integer factors**: the ALF randomization algorithm provides an efficient alternative to interpolation:
```
nLower = floor(factor), nUpper = ceil(factor)
pLower = nUpper - factor  // probability of using nLower
if (random() < pLower) blockSize = nLower
else blockSize = nUpper
// Then hold samples for blockSize duration
```

This trades precision for computational efficiency and a "rough" character — both desirable in a lo-fi plugin.

**Antialiasing:** A pre-decimation low-pass filter at `cutoff = targetSampleRate / 2` prevents aliasing artifacts when the user wants cleaner degradation. A post-filter is available for additional smoothing.

**Buffer-boundary smoothing:** When block sizes don't divide evenly, apply a short fade-out at buffer end and fade-in at buffer start to prevent clicks/pops.

**Parameter:**
| Param | Type | Range | Default | Description |
|-------|------|-------|---------|-------------|
| Sample Rate | Float | 100–48000 Hz | 44100 | Target sample rate (lower = more extreme) |
| Anti-Alias Pre | Toggle | Off/On | On | Low-pass filter before decimation |
| Anti-Alias Post | Toggle | Off/On | Off | Low-pass filter after decimation |
| Decimator On/Off | Toggle | Off/On | Off | Bypass this module |

**Reference:** Chris Relyea, "ALF paper", 2025 [src:alf-paper]; Wikipedia "Downsampling (signal processing)" [src:wikipedia-downsampling]

---

### 2.3 Distortion Module

**Purpose:** Introduce harmonic content that sounds thin, harsh, or brittle — the opposite of Sausage Fattener's warm saturation.

Sausage Fattener uses **normalized tanh** (`tanh(x*drive)/tanh(drive)`), which is a soft-clipping saturation that rounds peaks and introduces warm even-order harmonics. BrittLizer's distortion should do the opposite:

**Three selectable algorithms:**

#### 2.3.1 Hard Clip
```
output = clamp(drive * input, -threshold, +threshold)
```
Creates strong odd harmonics (3rd, 5th, 7th) — the characteristic buzz of transistor fuzz. Sharp transitions sound aggressive and "thin" at moderate to high drive.

#### 2.3.2 Foldback
```
while (sample > threshold || sample < -threshold) {
    if (sample >  threshold) sample =  2 * threshold - sample
    if (sample < -threshold) sample = -2 * threshold - sample
}
```
Instead of clipping, the waveform "folds" back on itself. Produces complex, metallic, dissonant harmonics. Especially effective on bass content — creates thin, glitchy artifacts.

#### 2.3.3 Asymmetric Clip
```
if (sample > 0) output = tanh(drive * sample)      // soft on positive
else            output = clamp(drive * sample, -threshold, +threshold)  // hard on negative
```
Asymmetric clipping generates even-order harmonics in addition to odd — a characteristic that sounds "broken" or "lopsided." Very different from the symmetric saturation of Sausage Fattener.

**Oversampling recommendation:** All distortion types generate harmonics above Nyquist. Use `juce::dsp::Oversampling<float>` with 2x or 4x factor for anti-aliasing (toggleable — some users want the raw aliasing).

**Parameter:**
| Param | Type | Range | Default | Description |
|-------|------|-------|---------|-------------|
| Distortion Type | Enum | Hard Clip / Foldback / Asymmetric | Hard Clip | Select distortion algorithm |
| Drive | Float | 0.0–10.0 | 1.0 | Pre-distortion gain multiplier |
| Threshold | Float | 0.01–1.0 | 0.5 | Clip/fold threshold (lower = more aggressive) |
| Oversample | Toggle | Off/2x/4x | 2x | Anti-aliasing oversampling factor |
| Distortion On/Off | Toggle | Off/On | Off | Bypass this module |

**Reference:** JDSherbert, "Audio-DSP-Distortion" GitHub [src:jdsherbert-distortion]; DAFx papers on waveshaping [src:dafx-waveshaping]

---

### 2.4 Dither / Noise Layer

**Purpose:** Add noise to the signal. Dithering at the bit-depth reduction stage reduces quantization artifacts, but here the noise is used *creatively* — to simulate tape hiss, vinyl static, or digital noise floor.

**Three modes:**

#### 2.4.1 TPDF Dither
```
dither = (randomFloat(-1,1) + randomFloat(-1,1))  // triangular PDF
scaling = 2^(-bitDepth)
output = sample + dither * scaling
```
Standard triangular probability density function dither. When used *before* quantization, it decorrelates quantization error from the signal. In BrittLizer it can be used as an effect in its own right (audible at low bit depths).

#### 2.4.2 Noise Floor (colored noise overlay)
```
// Generate or loop a noise sample
noiseSample = noiseBuffer[phase % noiseBufferLength]
// Apply spectrum shaping filter (pink/brown noise)
output = sample + noiseLevel * noiseSample
```
Overlays a looped noise sample (pink noise for tape hiss, brown noise for rumble, white noise for digital artifacts). Volume-controllable.

#### 2.4.3 Vinyl Crackle / Pops
```
if (random() < popProbability) {
    output = sample + popAmplitude * impulseResponse
}
```
Sparse, randomized impulse events simulating vinyl dust pops. Probability and amplitude adjustable.

**Parameter:**
| Param | Type | Range | Default | Description |
|-------|------|-------|---------|-------------|
| Noise Mode | Enum | TPDF / Pink / Brown / White / Crackle | Pink | Noise type |
| Noise Level | Float | 0.0–1.0 | 0.0 | Volume of added noise |
| Pop Rate | Float | 0.0–10.0 (Hz) | 0.0 | Frequency of vinyl pop events |
| Pop Intensity | Float | 0.0–1.0 | 0.5 | Amplitude of pop events |
| Noise On/Off | Toggle | Off/On | Off | Bypass this module |

**Reference:** Chris Relyea, "ALF paper" vinyl noise implementation [src:alf-paper]; Wikipedia "Noise shaping" [src:wikipedia-noise-shaping]; Vanderkooy & Lipshitz, "Dither in Digital Audio", 1987 [src:dither-paper]

---

### 2.5 Output Section (Filter → Mix → Gain)

**Purpose:** Shape the final degraded signal and blend with the original.

**Post-filter:** A simple biquad low-pass or band-pass filter to tame harshness from earlier stages. JUCE provides `juce::dsp::IIR::Filter<float>` which can be configured as:
- Low-pass (for "telephone" or "AM radio" effect)
- Band-pass (for "tinny" thin sound)
- High-pass (remove low end, emphasizing thin character)

**Dry/wet mix:** Linear crossfade between dry input and processed signal. JUCE's `juce::dsp::DryWetMixer` handles this with proper gain compensation and latency.

**Output gain:** Simple multiplier for the final level.

**Parameter:**
| Param | Type | Range | Default | Description |
|-------|------|-------|---------|-------------|
| Filter Type | Enum | Off / LP / BP / HP | Off | Post-processing filter |
| Filter Freq | Float | 20–20000 Hz | 8000 | Filter cutoff frequency |
| Filter Q | Float | 0.1–10.0 | 0.707 | Filter resonance |
| Dry/Wet Mix | Float | 0.0–1.0 | 0.5 | Blend between dry and processed |
| Output Gain | Float | -24–+24 dB | 0.0 | Final output level trim |

**Reference:** JUCE DSP module documentation [src:juce-dsp]; Zölzer et al., *DAFX: Digital Audio Effects*, 2nd ed. [src:dafx-book]

---

## 3. Complete Parameter List

| ID | Name | Type | Range | Default | Module |
|----|------|------|-------|---------|--------|
| `bitDepth` | Bit Depth | Int [1–32] | 1–32 | 16 | Bit Crusher |
| `bitEnabled` | Bit Crusher On | Bool | Off/On | Off | Bit Crusher |
| `sampRate` | Sample Rate | Float [100–48000] | 100–48000 | 44100 | Decimator |
| `aaPre` | Anti-Alias Pre | Bool | Off/On | On | Decimator |
| `aaPost` | Anti-Alias Post | Bool | Off/On | Off | Decimator |
| `decEnabled` | Decimator On | Bool | Off/On | Off | Decimator |
| `distType` | Distortion Type | Enum {0,1,2} | Hard Clip/Foldback/Asym | Hard Clip | Distortion |
| `distDrive` | Distortion Drive | Float [0–10] | 0.0–10.0 | 1.0 | Distortion |
| `distThreshold` | Dist Threshold | Float [0.01–1.0] | 0.01–1.0 | 0.5 | Distortion |
| `distOversample` | Oversample | Enum {0,1,2} | Off/2x/4x | 2x | Distortion |
| `distEnabled` | Distortion On | Bool | Off/On | Off | Distortion |
| `noiseMode` | Noise Mode | Enum {0,1,2,3,4} | TPDF/Pink/Brown/Wht/Crackle | Pink | Noise |
| `noiseLevel` | Noise Level | Float [0–1] | 0.0–1.0 | 0.0 | Noise |
| `popRate` | Pop Rate | Float [0–10] | 0.0–10.0 | 0.0 | Noise |
| `popIntensity` | Pop Intensity | Float [0–1] | 0.0–1.0 | 0.5 | Noise |
| `noiseEnabled` | Noise On | Bool | Off/On | Off | Noise |
| `filterType` | Filter Type | Enum {0,1,2,3} | Off/LP/BP/HP | Off | Output |
| `filterFreq` | Filter Freq | Float [20–20000] | 20–20000 | 8000 | Output |
| `filterQ` | Filter Q | Float [0.1–10] | 0.1–10.0 | 0.707 | Output |
| `mix` | Dry/Wet Mix | Float [0–1] | 0.0–1.0 | 0.5 | Output |
| `outputGain` | Output Gain | Float [-24–+24] | -24–+24 dB | 0.0 | Output |

**Total: 23 parameters** — manageable for a single-page UI with tabbed or collapsible module sections.

---

## 4. Proposed UI Layout

### Design Philosophy
- Follow the **Sausage Fattener simplicity** ethos but with more depth
- Dark theme with high-contrast parameters
- Each module is a horizontal strip with on/off toggle + key parameters
- Visual degradation feedback (waveform display optional)

### Layout (960×520px, resizable)

```
┌──────────────────────────────────────────────────────────────┐
│  ═══════════════════  BRITTLIZER  ═══════════════════        │
│                                         v1.0.0               │
├──────────────────────────────────────────────────────────────┤
│ ┌─Bit Crusher───[ON]──────────────────────────────────────┐ │
│ │  Bit Depth: [═══●═══════════════] 32                     │ │
│ │  1                             32                        │ │
│ └──────────────────────────────────────────────────────────┘ │
│ ┌─Decimator─────[ON]──────────────────────────────────────┐ │
│ │  Sample Rate: [══●══════════════════] 11025 Hz           │ │
│ │  100                      48000                          │ │
│ │  ☑ Anti-Alias Pre   ☐ Anti-Alias Post                   │ │
│ └──────────────────────────────────────────────────────────┘ │
│ ┌─Distortion────[ON]──[▼ Hard Clip  ]─────────────────────┐ │
│ │  Drive: [═════●════════]  Threshold: [═════●════════]   │ │
│ │  0.0         10.0       0.01           1.0               │ │
│ │  Oversample: [▼ 2x  ]                                   │ │
│ └──────────────────────────────────────────────────────────┘ │
│ ┌─Noise─────────[ON]──[▼ Pink Noise]──────────────────────┐ │
│ │  Level: [═══●═══════════════]  Pop Rate: [═══════] /s   │ │
│ │  0.0          1.0            0.0         10.0            │ │
│ └──────────────────────────────────────────────────────────┘ │
│ ┌─Output──────────────────────────────────────────────────┐ │
│ │  Filter: [▼ LP ]  Freq: [═══●═════] 8000  Q: [══●══]   │ │
│ │  Mix:   [══════●══════] 0.5  Gain: [══●═══] 0.0 dB    │ │
│ └──────────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────┘
```

### Key UI Decisions
1. **Module-level bypass** — each module has its own power toggle so users can build a custom signal chain
2. **Sliders with numeric readout** — precise parameter control alongside visual feedback
3. **Dropdown selects** for distortion type, noise mode, filter type — reduces knob count
4. **Dark theme** with orange/red accents (the "heat/brittle" color palette, opposite of Sausage Fattener's green)
5. **Tooltips** on hover show the parameter's effect description

### JUCE Implementation
- Use `juce::AudioProcessorValueTreeState` (APVTS) for all parameter automation and persistence
- UI built with `juce::Component` subclasses (one per module strip)
- Sliders use `juce::Slider::LinearHorizontal` style
- Dropdowns use `juce::ComboBox`
- Toggle buttons use `juce::TextButton` with click-to-toggle
- Use `juce::LookAndFeel` customization for the dark theme

---

## 5. Build Configuration for Logic Pro X

### Target Formats
- **Audio Unit (AU)** — for Logic Pro X native compatibility
- **VST3** — for other DAWs (Ableton, Reaper, FL Studio)
- **Standalone** — for standalone testing

### Apple Silicon (arm64) Universal Binary
- Target both `arm64` and `x86_64` architectures
- Xcode build setting: `ARCHS = arm64 x86_64`
- Validate with: `lipo -info /path/to/BrittLizer.component`
- Use Xcode 15+ with macOS 14+ SDK for latest arm64 optimizations

### JUCE Configuration (CMakeLists.txt)
```cmake
juce_add_plugin(BrittLizer
    PRODUCT_NAME "BrittLizer"
    FORMATS AU VST3 Standalone
    COMPANY_NAME "Brosio Audio"
    COMPANY_WEBSITE "https://ambrosiogomez.com"
    COMPANY_EMAIL "brosio@example.com"
    PLUGIN_MANUFACTURER_CODE BrAo
    PLUGIN_CODE BrLz
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORY "Fx|Distortion"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_TO_OUTPUT_DIR "$ENV{HOME}/Library/Audio/Plug-Ins/Components"
    COPY_PLUGIN_AFTER_BUILD TRUE
)

target_compile_definitions(BrittLizer PRIVATE
    JucePlugin_ManufacturerCode=0x4272416F     # 'BrAo'
    JucePlugin_PluginCode=0x42724C7A           # 'BrLz'
)

target_link_libraries(BrittLizer PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_plugin_client
    juce::juce_dsp
    juce::juce_gui_basics
)

# Universal binary
set_target_properties(BrittLizer PROPERTIES
    XCODE_ATTRIBUTE_ARCHS "arm64 x86_64"
)
```

### Minimum Requirements
- **macOS:** 11.0 (Big Sur) or later
- **Architecture:** Apple Silicon + Intel (Universal 2 binary)
- **JUCE:** 7.x or 8.x
- **Build System:** CMake 3.24+ with Xcode generator

### Installation Path for Logic Pro X
- AU component: `~/Library/Audio/Plug-Ins/Components/BrittLizer.component`
- Logic Pro X scans AU plugins at launch or can be force-scanned via Plug-in Manager

### AU Validation Checklist
1. `auval -strict -a` — validates all installed AUs
2. `auval -strict -v aufx BrLz BrAo` — targeted validation
3. `auval -strict -v aufx BrLz BrAo -c 2` — stereo mode validation
4. Check for: parameter persistence, bypass, sample rate changes, buffer size changes
5. Use `pluginval` (free from iPlug2) for stress testing at level 10

### Code Signing & Notarization
1. Sign with Developer ID Application certificate:
   ```
   codesign --force --sign "Developer ID Application: Your Name" --deep \
       --timestamp --options runtime BrittLizer.component
   ```
2. Notarize:
   ```
   xcrun notarytool submit BrittLizer.zip \
       --apple-id <email> --team-id <team> --password <app-password> --wait
   ```
3. Staple:
   ```
   xcrun stapler staple BrittLizer.component
   ```

### Denormal Float Handling
```cpp
void processBlock(AudioBuffer<float>& buffer, MidiBuffer&) {
    const juce::ScopedNoDenormals noDenormals;
    // ... processing code ...
}
```

**Reference:** Task `t_d48464c2` — Sausage Fattener research (JUCE, Apple Silicon, AU validation) [src:task-sausage-research]; JUCE documentation [src:juce-docs]

---

## 6. Signal Flow Detail

```
Input (float -1..1)
    │
    ▼
┌─────────────┐
│ Bit Crusher │ ← bitDepth, bitEnabled
│ (quantize)  │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ Decimator   │ ← sampRate, aaPre, aaPost, decEnabled
│ (S&H hold)  │
│ + anti-     │
│ aliasing LP │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ Distortion  │ ← distType, distDrive, distThreshold,
│ (clip/fold/ │   distOversample, distEnabled
│  asym)      │
│ + oversamp  │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ Noise Layer │ ← noiseMode, noiseLevel, popRate,
│ (dither/    │   popIntensity, noiseEnabled
│  vinyl/     │
│  hiss)      │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ Output      │ ← filterType, filterFreq, filterQ,
│ (post-filt, │   mix, outputGain
│  dry/wet,   │
│  gain)      │
└──────┬──────┘
       │
       ▼
Output (float -1..1)
```

Each module passes `AudioBuffer<float>&` to the next. Modules that are bypassed pass the buffer through unchanged.

---

## 7. Naming & Branding

**Plugin Name:** BrittLizer
**Company:** Brosio Audio
**Plugin Code:** `BrLz`
**Manufacturer Code:** `BrAo`
**AU Type:** `aufx` (Audio Unit Effect)

**Design Theme:** "What happens when you leave the sausage in too long." Dark oranges, reds, and charcoal — the visual opposite of Sausage Fattener's vibrant green. Cracked/burnt texture motifs.

---

## 8. Gaps & Open Questions

1. **Oversampling latency** — JUCE's `dsp::Oversampling` adds latency. Need to determine acceptable latency for Logic Pro X (AU requires reporting via `getLatencySamples()`).
2. **CPU budget** — 5 DSP stages running at 4x oversampling could push CPU high. Need profiling on Apple Silicon (M1/M2/M3/M4) to determine safe default configurations.
3. **Preset system** — Not in scope for v1 but worth noting. JUCE provides `AudioProcessor::getCurrentProgram()` / `setCurrentProgram()` support. Planned for v1.1.
4. **Stereo vs. Mid/Side** — Should the bit crusher and decimator process stereo channels independently? Current design is per-channel with no MS option. Could be a future feature.
5. **Modulation (LFO)** — Commercial lo-fi plugins (RC-20, SketchCassette) include LFO modulation for wow/flutter. Not planned for v1 but documented as a v2 feature.
6. **Waveform visualization** — A real-time waveform or spectrum display would help users understand the degradation. Adds UI complexity — consider for v1.1.
7. **Linux build** — JUCE supports Linux (VST3). The design targets macOS/Logic Pro X first, but the CMakeLists.txt can be extended with `LINUX` format target.

---

## 9. References

| ID | Source | Description |
|----|--------|-------------|
| [src:alf-paper] | Chris Relyea, "ALF: Creating a Lo-fi Effects Plugin Using C++ and JUCE", 2025 | Complete lo-fi plugin design with downsampling randomization algorithm |
| [src:jdsherbert-distortion] | JDSherbert, "Audio-DSP-Distortion", GitHub | C++ distortion algorithms (soft clip, hard clip, foldback) |
| [src:pirkle-dap] | Will Pirkle, *Designing Audio Effect Plugins in C++*, Routledge 2019 | Bit depth reduction quantization algorithm |
| [src:wikipedia-downsampling] | Wikipedia, "Downsampling (signal processing)" | Decimation theory, anti-aliasing filters |
| [src:wikipedia-noise-shaping] | Wikipedia, "Noise shaping" | Dither and noise shaping for quantization |
| [src:dither-paper] | Vanderkooy & Lipshitz, "Dither in Digital Audio", JAES 1987 | TPDF dither theory |
| [src:dafx-book] | Zölzer et al., *DAFX: Digital Audio Effects*, 2nd ed. | Filter design, waveshaping, modulation effects |
| [src:juce-docs] | JUCE Documentation, juce.com | DSP module API, AudioProcessor, oversampling |
| [src:task-sausage-research] | Kanban task t_d48464c2, Research: Sausage Fattener DSP + JUCE + Apple Silicon AU | Apple Silicon universal binary, code signing, auval, denormals |
| [src:ribcrusher] | viljavai, "RibCrusher" GitHub | Bitcrusher VST3 with dither, bytebeat expression parsing |

---

*Design document generated 2026-07-20 by CarmyGPT for Brosio Audio. Ready for implementation.*
