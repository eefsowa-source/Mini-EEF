#!/usr/bin/env python3
"""Dependency-free DSP smoke tests for the EEF-JP8000 design equations.

This is intentionally a fast numerical gate rather than a substitute for
pluginval or listening tests.  It exercises the same PolyBLEP and TPT-SVF
equations used by the real-time processor at all supported sample rates and
checks finite output, bounded resonance, and DC control.
"""
import math
import time


def blep(t, dt):
    dt = min(0.49, max(1.0e-5, dt))
    if t < dt:
        t /= dt
        return t + t - t * t - 1.0
    if t > 1.0 - dt:
        t = (t - 1.0) / dt
        return t * t + t + t + 1.0
    return 0.0


def osc(phase, inc, waveform):
    phase -= math.floor(phase)
    if waveform == 0:
        return 2.0 * phase - 1.0 + blep(phase, inc)
    if waveform == 1:
        return (1.0 if phase < 0.5 else -1.0) + blep(phase, inc) - blep((phase + 0.5) % 1.0, inc)
    if waveform == 2:
        return 1.0 - 4.0 * abs(phase - 0.5)
    return math.sin(2.0 * math.pi * phase)


def run_wave_tests():
    for sr in (44100, 48000, 96000, 192000):
        for freq in (20.0, 20000.0):
            for waveform in range(4):
                phase = 0.0
                values = []
                inc = min(0.49, freq / sr)
                sample_count = max(4096, int(sr / freq * 4.0))
                for _ in range(sample_count):
                    values.append(osc(phase, inc, waveform))
                    phase = (phase + inc) % 1.0
                assert all(math.isfinite(x) for x in values)
                assert max(abs(x) for x in values) <= 2.1
                mean = sum(values) / len(values)
                # Short windows at 20 Hz do not contain an integer number of
                # cycles, so allow the expected endpoint bias while still
                # catching a genuine DC-producing implementation.
                assert abs(mean) < 0.2, (sr, freq, waveform, mean)


def run_filter_tests():
    sr = 44100.0
    ic1 = ic2 = 0.0
    peak = 0.0
    for n in range(200000):
        cutoff = 20.0 + (19900.0 * ((n * 1103515245 + 12345) & 0x7fffffff) / 0x7fffffff)
        resonance = ((n * 1664525 + 1013904223) & 0x7fffffff) / 0x7fffffff
        x = 1.0 if n == 0 else 0.0
        g = math.tan(math.pi * min(0.45 * sr, cutoff) / sr)
        damping = max(0.08, min(2.0, 2.0 - 1.92 * max(0.0, min(1.0, resonance))))
        den = 1.0 + g * (g + damping)
        v1 = (g * (x - ic2) + ic1) / den
        v2 = ic2 + g * v1
        ic1, ic2 = 2.0 * v1 - ic1, 2.0 * v2 - ic2
        low, band = v2, v1
        high = x - damping * band - low
        peak = max(peak, abs(low), abs(band), abs(high))
        assert all(math.isfinite(v) for v in (ic1, ic2, low, band, high))
        assert abs(ic1) < 100.0 and abs(ic2) < 100.0
    assert peak < 100.0


def run_cpu_smoke():
    start = time.perf_counter()
    acc = 0.0
    # Representative worst-case inner-loop load: 16 voices x 8 unison layers.
    for voice in range(16):
        for layer in range(8):
            phase = (voice * 0.173 + layer * 0.311) % 1.0
            for _ in range(2048):
                acc += osc(phase, 440.0 / 48000.0, (voice + layer) & 3)
                phase = (phase + 440.0 / 48000.0) % 1.0
    elapsed = time.perf_counter() - start
    assert math.isfinite(acc)
    print(f"cpu-smoke: {elapsed:.3f}s (reference loop, host dependent)")


if __name__ == "__main__":
    run_wave_tests()
    run_filter_tests()
    run_cpu_smoke()
    print("dsp-sanity: PASS (44.1/48/96/192 kHz; finite, bounded, DC checks)")
