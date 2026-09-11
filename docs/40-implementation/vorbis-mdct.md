# Fast inverse Vorbis MDCT (2026-09-10)

The private `vorbis_mdct` layer transforms `n/2` spectral floats into `n`
time-domain floats for every legal Vorbis block size from 64 through 8192. Its
butterfly/bit-reversal structure is adapted from Xiph libvorbis 1.3.7 under the
BSD license retained in `THIRD_PARTY_NOTICES.md`.

Unlike the hosted reference initializer, the plan owns no memory and calls no
`malloc`, `sin`, `cos`, `log`, `rint` or `sqrt`. The caller supplies exactly
`n+n/4` binary32 trigonometric values and `n/4` signed 32-bit bit-reversal
entries. Freestanding polynomial sine/cosine generation covers the constrained
angle domain. The maximum plan therefore occupies 40960 bytes of trig data and
8192 bytes of indices. Transform scratch is `n` floats, at most 32768 bytes per
channel.

Plan, input, output and scratch regions are checked for overlap. Plan sizes,
power-of-two/log consistency, every bit-reversal index and finite trig/input/
output values are validated before output publication. The transform uses
caller scratch in-place and commits output only after the complete result is
finite and within `max_abs`. The source adaptation replaces reference loops
that form before-array pointers with explicit bounded iteration counts.

An independent direct-form mathematical oracle checked 42 transforms and 51648
time-domain samples across all legal sizes, including sparse randomized spectra.
Maximum absolute error was `7.76e-07`. Focused ASan/UBSan passed. GCC 15.2
freestanding/analyzer compilation has no unresolved symbols; maximum reported
frame is 192 bytes. Disassembly is retained at
`build/vorbis-mdct-freestanding.asm`.

The maintained optimized WSL host benchmark retained 303 batch samples for an
8192-point inverse transform after the final plan-integrity preflight: median
45155 ns, p95 46595 ns and p99 54050 ns. Raw samples and source/build hashes are
in `build/vorbis-primitives-20260910.json`. These are host batch averages, not a
VM real-time acceptance threshold.

The CapyOS Etapa 10 branch now provides the FP/SIMD task-switch support described
by the window layer. This private transform is still not a public codec feature
or CapyOS player path; complete packet orchestration and runtime playback
evidence remain mandatory.
