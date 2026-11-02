# Credits and references

Analog ’83 is original code. No source code from other projects is copied into it. It does use published techniques and a few published values, credited here. Product names appear below only where they are part of a paper's title, as an accurate citation requires. The engine doesn't emulate, or claim compatibility with, any specific commercial product.

## Values taken from published work

**Room reverb delay lengths: Freeverb.** The room's four comb-filter lengths (25.3, 26.9, 29.0, 30.7 ms), two all-pass lengths (12.6 and 10.0 ms) and stereo offset are Freeverb's tuning values (1116, 1188, 1277, 1356; 556, 441; stereo spread 23 samples at 44.1 kHz), converted to milliseconds. The parallel-combs-into-all-passes structure follows the same design.

- Jezar at Dreampoint, *Freeverb*, June 2000. Released into the public domain.
- Used in: `Analog83Record.cpp` (`kCombMs`, `kAllPassMs`, stereo spread).

**Hi-hat oscillators.** The hi-hat uses six square-wave oscillators at inharmonic frequencies, summed and filtered. This is the classic analog drum-machine hi-hat/cymbal architecture, analyzed in detail in:

- K. J. Werner, J. S. Abel, J. O. Smith III, “The TR-808 Cymbal: a Physically-Informed, Circuit-Bendable, Digital Model,” *Proc. 40th International Computer Music Conference / 11th Sound and Music Computing Conference (ICMC/SMC)*, Athens, Greece, September 2014.

The six frequencies used (205.3, 304.4, 369.6, 522.7, 540, 800 Hz) are widely published values for this kind of circuit. I could not trace them to a single primary source; maintainers are welcome to substitute their own tuning. Used in: `Analog83HiHat.cpp` (`kOscHz`).

## Algorithms

| Technique | Source | Used in |
|---|---|---|
| State-variable filter, trapezoidal (TPT) form | A. Simper, Cytomic technical papers on solving the SVF with trapezoidal integration (2011–2013); V. Zavalishin, *The Art of VA Filter Design*, Native Instruments | `SVF` in `Analog83Common.h` |
| Biquad lowpass, highpass and peaking EQ | R. Bristow-Johnson, “Cookbook formulae for audio EQ biquad filter coefficients” (the “Audio EQ Cookbook”) | `Biquad` in `Analog83Record.*` |
| PolyBLEP band-limited square waves | V. Välimäki and A. Huovilainen, “Antialiasing Oscillators in Subtractive Synthesis,” *IEEE Signal Processing Magazine*, 24(2), 2007, and the polynomial band-limited step technique that grew out of that line of work | `polyBlep()` in `Analog83HiHat.cpp` |
| Bounded multi-producer queue | D. Vyukov, “Bounded MPMC queue,” 1024cores.net | `MpscQueue` in `Analog83Kit.h` |
| White noise generator | G. Marsaglia, “Xorshift RNGs,” *Journal of Statistical Software*, 8(14), 2003 | `Noise` in `Analog83Common.h` |
| DC blocker (one-pole, one-zero highpass) | J. O. Smith III, *Introduction to Digital Filters*, CCRMA, Stanford | `DcBlocker` in `Analog83Common.h` |
| Fast tanh | Padé approximant from Lambert's continued fraction for tanh (standard mathematics) | `fastTanh()` in `Analog83Common.h` |
| 4-point cubic Hermite interpolation | Standard technique; see O. Niemitalo, “Polynomial Interpolators for High-Quality Resampling of Oversampled Audio,” 2001 | `WowFlutter` in `Analog83Record.cpp` |

## Background reading (no code or values taken)

These were listed as references when the engine was proposed (issue #16). They informed the general understanding of analog drum circuits, but nothing from them is used in the code:

- K. J. Werner, J. S. Abel, J. O. Smith III, “A Physically-Informed, Circuit-Bendable, Digital Model of the Roland TR-808 Bass Drum Circuit,” *Proc. 17th International Conference on Digital Audio Effects (DAFx-14)*, Erlangen, Germany, September 2014.
- Other open-source drum synthesis projects mentioned in issue #16. No code from them is used.

The kick is an original design: a sine with a fast pitch drop, a slight pitch droop, a filtered-noise beater, and tube/tape-style saturation, following generally documented analog kick behavior rather than any particular circuit.
