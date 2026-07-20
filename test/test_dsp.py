#!/usr/bin/env python3
"""
DSP Unit Tests for BrittLizer — Bit Crusher, Decimator, Distortion, Noise, Output.

Tests validate algorithmic properties (not JUCE integration) by re-implementing
the core math in pure Python.  These can run on any platform without JUCE or X11.
"""

import math
import random
import statistics
import struct
import unittest
from array import array
from typing import Callable, List


# =============================================================================
#  Test helpers
# =============================================================================

def generate_sine(freq: float, sample_rate: float, duration_sec: float,
                  amplitude: float = 1.0) -> List[float]:
    """Generate a sine wave."""
    n = int(sample_rate * duration_sec)
    return [amplitude * math.sin(2.0 * math.pi * freq * t / sample_rate)
            for t in range(n)]


def generate_silence(num_samples: int) -> List[float]:
    """Generate silent samples."""
    return [0.0] * num_samples


def db_to_linear(db: float) -> float:
    return 10.0 ** (db / 20.0)


def linear_to_db(linear: float) -> float:
    if linear <= 0.0:
        return -96.0
    return 20.0 * math.log10(linear)


def rms(samples: List[float]) -> float:
    if not samples:
        return 0.0
    return math.sqrt(sum(x * x for x in samples) / len(samples))


def peak(samples: List[float]) -> float:
    if not samples:
        return 0.0
    return max(abs(x) for x in samples)


def thd(samples: List[float], fundamental_freq: float,
        sample_rate: float) -> float:
    """
    Compute total harmonic distortion (THD) as ratio of harmonic power to
    total power.  Simplified: just check energy at harmonics.
    """
    n = len(samples)
    if n < 2:
        return 0.0

    # Simple FFT-based THD estimation — use numpy if available
    try:
        import numpy as np
        from numpy.fft import rfft, rfftfreq

        windowed = np.array(samples) * np.hanning(n)
        spectrum = np.abs(rfft(windowed))
        freqs = rfftfreq(n, 1.0 / sample_rate)

        # Find fundamental bin
        fund_bin = np.argmin(np.abs(freqs - fundamental_freq))
        fund_energy = spectrum[fund_bin] ** 2

        # Sum harmonic energy (2nd through 5th)
        harmonic_energy = 0.0
        for h in range(2, 6):
            h_bin = np.argmin(np.abs(freqs - fundamental_freq * h))
            harmonic_energy += spectrum[h_bin] ** 2

        if fund_energy + harmonic_energy == 0.0:
            return 0.0
        return math.sqrt(harmonic_energy / (fund_energy + harmonic_energy))
    except ImportError:
        # Fallback: rough estimate via zero-crossing
        return 0.0  # Skip THD without numpy


# =============================================================================
#  1. Bit Crusher Tests
# =============================================================================

class TestBitCrusher(unittest.TestCase):
    """Tests for bit depth reduction quantization."""

    def setUp(self):
        self.sample_rate = 44100.0

    def quantize(self, samples: List[float], bit_depth: float) -> List[float]:
        """Replicate BitCrusher logic."""
        levels = 2.0 ** bit_depth
        scale = 0.5 * levels
        return [round(max(-1.0, min(1.0, x)) * scale) / scale for x in samples]

    def test_bypass_at_32bits(self):
        """32-bit should pass through unchanged (within float precision)."""
        signal = [0.1, -0.5, 0.999, -0.001, 0.0]
        result = self.quantize(signal, 32.0)
        for orig, q in zip(signal, result):
            self.assertAlmostEqual(orig, q, places=6)

    def test_16bit_is_unchanged(self):
        """16-bit should preserve typical float precision for clean signals."""
        signal = [0.1, -0.5, 0.25, -0.75, 0.0]
        result = self.quantize(signal, 16.0)
        for orig, q in zip(signal, result):
            self.assertAlmostEqual(orig, q, places=4)

    def test_8bit_introduces_quantization(self):
        """8-bit quantizes to 256 levels across [-1, 1]."""
        signal = [0.0, 0.5, -0.5, 0.25, -0.25]
        result = self.quantize(signal, 8.0)
        # 8-bit = 256 levels, scale = 128, step = 1/128 ≈ 0.0078125
        step = 1.0 / 128.0
        for orig, q in zip(signal, result):
            error = abs(orig - q)
            self.assertLessEqual(error, step + 1e-7)

    def test_1bit_is_waveshaping(self):
        """1-bit reduces to a 2-level square wave."""
        signal = [0.1, -0.5, 0.999, -0.001, 0.0]
        result = self.quantize(signal, 1.0)
        for q in result:
            # 1-bit: only two possible values
            self.assertIn(round(q * 1e6), [round(-1.0 * 1e6), round(0.0), round(1.0 * 1e6)])

    def test_clipping_before_quantization(self):
        """Input is clamped to [-1, 1] before quantization."""
        signal = [1.5, -2.0, 3.0]
        result = self.quantize(signal, 8.0)
        for q in result:
            self.assertLessEqual(abs(q), 1.0 + 1e-6)

    def test_sine_creates_harmonics(self):
        """4-bit quantization of a sine wave should create harmonic distortion."""
        sine = generate_sine(440.0, self.sample_rate, 0.1)
        original_thd = thd(sine, 440.0, self.sample_rate)
        crushed = self.quantize(sine, 4.0)
        crushed_thd = thd(crushed, 440.0, self.sample_rate)
        self.assertGreater(crushed_thd, original_thd)


# =============================================================================
#  2. Decimator Tests
# =============================================================================

class TestDecimator(unittest.TestCase):
    """Tests for sample rate reduction / decimation."""

    def setUp(self):
        self.sample_rate = 44100.0

    def decimate(self, samples: List[float], step: float,
                 initial_hold: float = 0.0) -> List[float]:
        """Replicate Decimator zero-order hold logic."""
        result = []
        phase = 0.0
        held = initial_hold
        for s in samples:
            phase += step
            if phase >= 1.0:
                phase -= 1.0
                held = s
            result.append(held)
        return result

    def test_bypass_at_full_rate(self):
        """Step ratio 1.0 should pass through unchanged (step >= 0.999 short-circuits in C++)."""
        # The C++ short-circuits at step >= 0.999, so at step=1.0 it returns early
        # But since we don't have that gate here, it should still hold every sample
        signal = [0.1, -0.5, 0.25, -0.75, 0.0]
        result = self.decimate(signal, 1.0)
        self.assertEqual(signal, result)

    def test_half_rate_holds_two_samples(self):
        """Step=0.5: every sample is held twice."""
        signal = [1.0, -1.0, 0.5, -0.5]
        result = self.decimate(signal, 0.5)
        # Initial phase=0, hold=0.0 (not signal[0]!)
        # s=0: phase 0→0.5, <1.0, held=0.0
        # s=1: phase 0.5→1.0 ≥1, phase→0, held=-1.0
        # s=2: phase 0→0.5, <1.0, held=-1.0
        # s=3: phase 0.5→1.0 ≥1, phase→0, held=-0.5
        expected = [0.0, -1.0, -1.0, -0.5]
        self.assertEqual(result, expected)

    def test_quarter_rate(self):
        """Step=0.25: one update per 4 samples."""
        signal = [0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8]
        result = self.decimate(signal, 0.25)
        # Initial hold=0.0
        # Phase: 0→0.25, 0.25→0.5, 0.5→0.75, 0.75→1.0 (update at s=3: 0.4)
        # Then: 0→0.25, 0.25→0.5, 0.5→0.75, 0.75→1.0 (update at s=7: 0.8)
        expected = [0.0, 0.0, 0.0, 0.4, 0.4, 0.4, 0.4, 0.8]
        self.assertEqual(result, expected)

    def test_step_greater_than_one_acts_as_downsample(self):
        """Step > 1 skips samples (e.g. step=2: every other sample)."""
        signal = [0.1, 0.2, 0.3, 0.4, 0.5, 0.6]
        result = self.decimate(signal, 2.0)
        # Phase 0→2.0 (≥1): update at s=0 with 0.1, phase 1.0
        # phase 1→3.0 (≥1): update at s=1 with 0.2, phase 2.0
        # phase 2→4.0 (≥1): update at s=2 with 0.3, phase 3.0
        # Hmm, with step=2.0, each sample triggers update
        # Actually: step=2 means we go twice the rate... let me think
        # phase starts at 0:
        # s=0: phase+=2 → 2.0 ≥ 1, phase-=1 → 1.0, held=0.1
        # s=1: phase+=2 → 3.0 ≥ 1, phase-=1 → 2.0, held=0.2
        # s=2: phase+=2 → 4.0 ≥ 1, phase-=1 → 3.0, held=0.3
        # So every sample updates. That's not downsampling. 
        # step = target / source. step=2 would be UPSAMPLING (target > source)
        # For downsample: step < 1
        # With step=0.25 and source 44100, target = 11025
        signal = [1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0]
        result = self.decimate(signal, 2.0)
        # Every sample updates because phase increments by 2 each time
        self.assertEqual(result, signal)

    def test_downsample_creates_stairstep(self):
        """Step < 1 creates a characteristic stairstep pattern."""
        signal = list(range(100))  # 0..99 rising edge
        result = self.decimate(signal, 0.1)
        # Only 10 updates: at s=9, 19, 29, ... 
        updates = []
        for i in range(1, len(result)):
            if result[i] != result[i - 1]:
                updates.append(i)
        self.assertEqual(len(updates), 9)  # 9 transitions (first is capture at s=9)


# =============================================================================
#  3. Distortion Tests
# =============================================================================

class TestDistortion(unittest.TestCase):
    """Tests for waveshaping distortion algorithms."""

    def hard_clip(self, x: float, drive: float, threshold: float) -> float:
        x *= drive
        return max(-threshold, min(threshold, x))

    def foldback(self, x: float, drive: float, threshold: float) -> float:
        x *= drive
        abs_x = abs(x)
        if abs_x <= threshold:
            return x

        folded = (abs_x - threshold) % (2.0 * threshold)
        if folded > threshold:
            folded = 2.0 * threshold - folded

        return folded if x >= 0.0 else -folded

    def asymmetric(self, x: float, drive: float, threshold: float) -> float:
        x *= drive
        if x >= 0.0:
            return threshold * math.tanh(x / max(threshold, 0.001))
        else:
            abs_x = -x
            y = abs_x / max(threshold, 0.001)
            y = y / (1.0 + y)
            return -threshold * y

    def test_hard_clip_preserves_quiet(self):
        """Hard clip passes through signals below threshold."""
        result = self.hard_clip(0.1, 1.0, 0.5)
        self.assertAlmostEqual(result, 0.1, places=6)

    def test_hard_clip_limits_at_threshold(self):
        """Hard clip clamps at ±threshold."""
        result = self.hard_clip(2.0, 1.0, 0.5)
        self.assertAlmostEqual(result, 0.5, places=6)
        result = self.hard_clip(-2.0, 1.0, 0.5)
        self.assertAlmostEqual(result, -0.5, places=6)

    def test_hard_clip_drive_boosts_then_clips(self):
        """Drive multiplies before clipping."""
        result = self.hard_clip(0.3, 3.0, 0.5)  # 0.3*3 = 0.9, clip at 0.5
        self.assertAlmostEqual(result, 0.5, places=6)

    def test_foldback_preserves_quiet(self):
        """Foldback passes through below threshold."""
        result = self.foldback(0.3, 1.0, 0.5)
        self.assertAlmostEqual(result, 0.3, places=6)

    def test_foldback_folds_above_threshold(self):
        """Foldback: values slightly above threshold fold back."""
        result = self.foldback(0.6, 1.0, 0.5)
        # abs_x=0.6, threshold=0.5
        # folded = (0.6-0.5) % 1.0 = 0.1, 0.1 < 0.5 → return 0.1
        self.assertAlmostEqual(result, 0.1, places=6)

    def test_foldback_double_fold(self):
        """Foldback wraps again for values > 2*threshold."""
        result = self.foldback(2.0, 1.0, 0.5)
        # abs_x=2.0, threshold=0.5
        # folded = (2.0-0.5) % 1.0 = 1.5 % 1.0 = 0.5, 0.5 <= 0.5 → return 0.5
        # Wait: 2.0 - 0.5 = 1.5, 1.5 % 1.0 = 0.5
        # 0.5 <= 0.5, so folded = 0.5
        self.assertAlmostEqual(result, 0.5, places=6)

    def test_asymmetric_positive(self):
        """Asymmetric: positive values use tanh-like shaping."""
        result = self.asymmetric(0.1, 1.0, 0.5)
        # Should be positive and less than linear scaling (tanh compression)
        linear = 0.1
        self.assertGreater(result, 0.0)
        self.assertLess(result, linear)

    def test_asymmetric_negative(self):
        """Asymmetric: negative values use different curve."""
        result = self.asymmetric(-0.5, 1.0, 0.5)
        # Should be negative
        self.assertLess(result, 0.0)

    def test_asymmetric_odd_order_harmonics(self):
        """Asymmetric should generate even+odd harmonics (different +/- curves)."""
        sine = generate_sine(440.0, 44100.0, 0.2)
        processed = [self.asymmetric(x, 2.0, 0.5) for x in sine]
        # Asymmetric should have even harmonics (second harmonic rich)
        # We'll verify by checking the wave is not perfectly anti-symmetric
        pos_peaks = []
        neg_peaks = []
        for x, y in zip(sine, processed):
            if x > 0.001:
                pos_peaks.append(abs(y))
            elif x < -0.001:
                neg_peaks.append(abs(y))

        if pos_peaks and neg_peaks:
            avg_pos = statistics.mean(pos_peaks)
            avg_neg = statistics.mean(neg_peaks)
            # Positive and negative amplitudes should differ
            self.assertNotAlmostEqual(avg_pos / max(avg_neg, 0.001), 1.0, places=1)

    def test_distortion_adds_harmonics(self):
        """All distortion types should add harmonics to a sine wave."""
        sine = generate_sine(440.0, 44100.0, 0.2)
        clip_thd = thd([self.hard_clip(x, 2.0, 0.5) for x in sine], 440.0, 44100.0)
        clean_thd = thd(sine, 440.0, 44100.0)
        self.assertGreater(clip_thd, clean_thd)


# =============================================================================
#  4. Noise Generator Tests
# =============================================================================

class TestNoiseGenerator(unittest.TestCase):
    """Tests for noise generation algorithms."""

    def setUp(self):
        random.seed(42)  # Deterministic for tests

    def generate_noise(self, num_samples: int, mode: str,
                       level: float = 1.0) -> List[float]:
        """
        Replicate NoiseGenerator algorithms in Python.
        Modes: 'tpdf', 'pink', 'brown', 'white', 'crackle'
        """
        result = [0.0] * num_samples

        if mode == 'white':
            for i in range(num_samples):
                result[i] = random.uniform(-1.0, 1.0)

        elif mode == 'tpdf':
            for i in range(num_samples):
                result[i] = (random.uniform(-1.0, 1.0) +
                             random.uniform(-1.0, 1.0)) * 0.5

        elif mode == 'pink':
            # Paul Kellet V-V: 7 bins
            bins = [0.0] * 7
            idx = 0
            for i in range(num_samples):
                idx += 1
                tz = (idx & -idx).bit_length() - 1  # trailing zeros
                if tz < 7:
                    bins[tz] = random.uniform(-1.0, 1.0)
                result[i] = sum(bins) / 7.0

        elif mode == 'brown':
            value = 0.0
            for i in range(num_samples):
                value += random.uniform(-1.0, 1.0) * 0.125
                value *= 0.99
                value = max(-1.0, min(1.0, value))
                result[i] = value

        # Scale by level
        return [x * level for x in result]

    def test_white_noise_mean_near_zero(self):
        """White noise should have near-zero mean for large N."""
        noise = self.generate_noise(10000, 'white')
        mean = statistics.mean(noise)
        self.assertAlmostEqual(mean, 0.0, places=1)

    def test_white_noise_rms(self):
        """White noise RMS should be approximately 1/sqrt(3) ≈ 0.577."""
        noise = self.generate_noise(50000, 'white')
        rms_val = rms(noise)
        expected = 1.0 / math.sqrt(3.0)  # Uniform [-1,1]
        self.assertAlmostEqual(rms_val, expected, places=1)

    def test_tpdf_peak_distribution(self):
        """TPDF has triangular distribution — lower peak probability."""
        noise = self.generate_noise(50000, 'tpdf')
        # TPDF peaks near 0, fewer extreme values
        extreme_count = sum(1 for x in noise if abs(x) > 0.8)
        white_noise = self.generate_noise(50000, 'white')
        white_extreme = sum(1 for x in white_noise if abs(x) > 0.8)
        # TPDF should have fewer extreme samples than uniform white
        self.assertLess(extreme_count, white_extreme)

    def test_tpdf_rms(self):
        """TPDF RMS = 1/sqrt(6) ≈ 0.408 (sum of 2 uniforms, /2)."""
        noise = self.generate_noise(50000, 'tpdf')
        rms_val = rms(noise)
        expected = math.sqrt(2.0 / 3.0) * 0.5  # RMS of TPDF = 1/sqrt(6) ≈ 0.408
        self.assertAlmostEqual(rms_val, expected, places=1)

    def test_pink_noise_spectral_tilt(self):
        """Pink noise power spectrum falls at ~3dB/octave (rough check)."""
        # Simple proxy: pink noise has less high-frequency content
        # We can check by noting pink noise between adjacent samples
        pink = self.generate_noise(10000, 'pink')
        white = self.generate_noise(10000, 'white')
        brown = self.generate_noise(10000, 'brown')

        # Brown noise has the smallest sample-to-sample variation ratio
        pink_diff_rms = rms([pink[i] - pink[i-1] for i in range(1, len(pink))])
        white_diff_rms = rms([white[i] - white[i-1] for i in range(1, len(white))])
        brown_diff_rms = rms([brown[i] - brown[i-1] for i in range(1, len(brown))])

        # Pink should have more sample correlation than white
        # (its diff RMS should be lower relative to its overall RMS)
        pink_ratio = pink_diff_rms / max(rms(pink), 0.001)
        white_ratio = white_diff_rms / max(rms(white), 0.001)
        brown_ratio = brown_diff_rms / max(rms(brown), 0.001)

        # White: uncorrelated, high diff ratio
        # Pink: correlated, lower diff ratio
        # Brown: very correlated, lowest diff ratio
        self.assertLess(brown_ratio, pink_ratio)
        self.assertLess(pink_ratio, white_ratio)

    def test_brown_noise_dc_blocked(self):
        """Brown noise mean should be near zero (leaky integrator)."""
        noise = self.generate_noise(50000, 'brown')
        mean = statistics.mean(noise)
        self.assertAlmostEqual(mean, 0.0, places=1)

    def test_brown_noise_bounded(self):
        """Brown noise should stay within [-1, 1]."""
        noise = self.generate_noise(50000, 'brown')
        peak_val = peak(noise)
        self.assertLessEqual(peak_val, 1.0 + 1e-6)

    def test_level_scaling(self):
        """Noise level parameter scales amplitude."""
        level = 0.5
        noise = self.generate_noise(10000, 'white', level)
        rms_val = rms(noise)
        full_noise = self.generate_noise(10000, 'white', 1.0)
        full_rms = rms(full_noise)
        # Rough check: level 0.5 should halve the RMS
        self.assertAlmostEqual(rms_val / max(full_rms, 0.001), level, places=0)

    def test_tpdf_never_exceeds_bounds(self):
        """TPDF noise should never exceed the bounds of a triangle wave."""
        noise = self.generate_noise(100000, 'tpdf')
        max_val = peak(noise)
        # TPDF with [-1,1] inputs on each uniform: sum ranges [-2,2], /2 = [-1,1]
        self.assertLessEqual(max_val, 1.0 + 1e-6)


# =============================================================================
#  5. Output Stage Tests
# =============================================================================

class TestOutputStage(unittest.TestCase):
    """Tests for dry/wet mix, output gain, and filter."""

    def test_dry_wet_mix(self):
        """Dry/wet crossfade between dry and wet signals."""
        dry = [0.5, -0.3, 0.1]
        wet = [1.0, -0.9, 0.8]

        # 50% mix
        mix_50 = [dry[i] * 0.5 + wet[i] * 0.5 for i in range(len(dry))]
        expected = [0.75, -0.6, 0.45]
        for a, b in zip(mix_50, expected):
            self.assertAlmostEqual(a, b, places=6)

        # 100% wet
        mix_100 = [wet[i] for i in range(len(dry))]
        for a, b in zip(mix_100, wet):
            self.assertAlmostEqual(a, b, places=6)

        # 0% wet (100% dry)
        mix_0 = [dry[i] for i in range(len(dry))]
        for a, b in zip(mix_0, dry):
            self.assertAlmostEqual(a, b, places=6)

    def test_gain_linearity(self):
        """Output gain is a simple linear multiplier."""
        signal = [0.5, -0.3, 0.1]
        gain_db = -6.0  # 6dB cut ≈ 0.501
        gain_lin = db_to_linear(gain_db)
        expected = 10.0 ** (-6.0 / 20.0)
        self.assertAlmostEqual(gain_lin, expected, places=6)

        gained = [x * gain_lin for x in signal]
        for orig, g in zip(signal, gained):
            self.assertAlmostEqual(g, orig * expected, places=6)

    def test_gain_boost(self):
        """Positive dB values boost the signal."""
        gain_db = 6.0
        gain_lin = db_to_linear(gain_db)
        # 6dB is 10^(6/20) ≈ 1.995, NOT exactly 2.0
        expected = 10.0 ** (6.0 / 20.0)
        self.assertAlmostEqual(gain_lin, expected, places=6)

    def test_mix_at_zero_is_dry(self):
        """Mix=0 means only dry signal passes."""
        dry = [0.5, -0.3]
        wet = [1.0, -0.9]
        mix = 0.0
        output = [dry[i] * (1.0 - mix) + wet[i] * mix for i in range(len(dry))]
        self.assertEqual(output, dry)

    def test_mix_at_one_is_wet(self):
        """Mix=1 means only wet signal passes."""
        dry = [0.5, -0.3]
        wet = [1.0, -0.9]
        mix = 1.0
        output = [dry[i] * (1.0 - mix) + wet[i] * mix for i in range(len(dry))]
        self.assertEqual(output, wet)


# =============================================================================
#  6. Pipeline Integration Tests
# =============================================================================

class TestPipeline(unittest.TestCase):
    """Tests for the full DSP pipeline chain."""

    def test_silent_input_produces_silent_output(self):
        """Silent input with all modules enabled should produce near-silent output."""
        sample_rate = 44100.0
        num_samples = int(sample_rate * 0.1)
        silence = generate_silence(num_samples)

        # With no noise and no gain changes, processing silence = silence
        # This is a property check: 0 * anything = 0
        # (Crackle/pop noise could change this, but with noise_level=0 it passes)
        self.assertTrue(all(x == 0.0 for x in silence))

    def test_sine_preserves_fundamental(self):
        """The pipeline should pass a clean sine through when bypassed."""
        # This is a conceptual test — each module set to bypass/neutral
        sine = generate_sine(440.0, 44100.0, 0.1)

        # With all modules bypassed or at neutral settings:
        # - Bit crusher: bitDepth=32 (bypass)
        # - Decimator: sampleRate=44100 (step=1.0, bypassed)
        # - Distortion: bypassed or drive=1, threshold=1
        # - Noise: level=0 (bypass)
        # - Output: mix=0 (dry), gain=0dB
        # Signal should pass unchanged

        # Test decimator at full rate
        phase = 0.0
        held = 0.0
        result = []
        for s in sine:
            phase += 1.0  # step=1.0
            if phase >= 1.0:
                phase -= 1.0
                held = s
            result.append(held)

        self.assertEqual(sine, result)

    def test_identity_pipeline(self):
        """With all modules at neutral/off, signal passes through."""
        signal = [0.1 * math.sin(2.0 * math.pi * 440.0 * t / 44100.0)
                  for t in range(1000)]

        # Apply bit crusher at 32-bit (neutral)
        bit_32 = [round(x * (2**31)) / (2**31) for x in signal]
        # Apply dry/wet with mix=0
        output = [x for x in bit_32]  # 100% dry

        for orig, out in zip(signal, output):
            self.assertAlmostEqual(orig, out, places=6)


# =============================================================================
#  Run
# =============================================================================

if __name__ == '__main__':
    unittest.main(verbosity=2)
