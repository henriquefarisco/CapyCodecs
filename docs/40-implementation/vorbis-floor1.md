# Private floor1 integer curve and measurement (2026-09-08)

`vorbis_floor1` prepares persistent, caller-owned prediction neighbors and sorted
point indices from setup X coordinates, then reconstructs one present floor's
integer curve from decoded scalar Y values. The output is inverse-dB **table
indices**, not spectral gains or PCM. No entropy parsing, absent-floor handling,
gain table or player integration is implied.

The implementation follows the mathematical operations in
[Vorbis sections 7.2.4 and 9.2](https://www.xiph.org/vorbis/doc/Vorbis_I_spec.html).
It preserves prediction order separately from rendering order, including later
reactivation of previously inactive neighbors. Rendering uses a bounded integer
error accumulator, clips at the requested bin count, and extends the final knot
when the setup endpoint is shorter than the output.

## Ownership and resource policy

- 2..65 distinct points, multiplier 1..4, first point zero, second point a
  power of two no greater than 32768, all remaining points inside that interval.
- The prepared plan is immutable and owned by the caller (328 bytes on the
  measured x86-64 host). No heap, I/O, callbacks or floating-point arithmetic.
- At most 4096 output bins (half the maximum Vorbis block). Setup preparation
  is bounded quadratic work over 65 points; rendering is linear in points+bins.
- Malformed/out-of-range wrapped amplitudes fail closed rather than being
  silently clamped. All fallible validation/prediction completes before output
  begins, so any error preserves the entire output buffer. Preparation clears
  its output plan on error. Input/output/plan storage must not overlap.
- Generated x86-64 freestanding GCC `-O2 -mno-red-zone -fstack-usage` reports
  416 bytes each for prepare/curve, no undefined functions. These are per-frame
  static stack reports, not a whole-decoder memory or kernel execution proof.

## Executed validation

`make validate` and the full owning suite rebuilt into a separate sanitizer
directory passed:

```sh
make validate BUILD_DIR=build/sanitized-20260908 \
  CFLAGS='-std=c11 -Wall -Wextra -Werror -pedantic -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer'
make vorbis-reference-test
```

Floor tests cover slopes, clipping/tails, all-or-nothing failure, capacities,
maximum coordinates/dimensions and 50000 deterministic adversarial mutations.
The independent Python specification oracle passed 68540 curves / 2014903 bins:
all 65536 endpoint pairs, 3000 seeded multipoint cases and four maximum cases.
Its residual inversion enumerates possible amplitudes, and its line interpolation
uses direct rational evaluation rather than reproducing the C branch/error loop.
This is not a comparison with a complete libvorbis floor/PCM decoder.

The same reference target retained headers, setup and Huffman checks and expanded
VQ coverage to 110240 packed-float patterns and 10880 vectors from 512 Xiph books,
including 1/4/8/16-bit lookup values. Xiph's exponent-clamp difference remains
explicit in [the VQ policy](vorbis-vq.md).

Logs: `build/vorbis-floor1-validation.log`, `build/vorbis-floor1-reference.log`,
`build/vorbis-floor1-sanitizers.log`, `build/vorbis-full-sanitized-20260908.log`,
`build/vorbis-floor1-freestanding.asm`, `build/vorbis_floor1_freestanding.su`.

## Measured host behavior

Maintained entry points (choose a new JSON name; existing reports are not overwritten):

```sh
make vorbis-benchmark
python3 -B tests/audio/measure_vorbis_primitives.py build/measurement.json
```

Environment: Intel Core i3-10100F, x86-64 WSL2 Ubuntu, kernel
6.18.33.2-microsoft-standard-WSL2, GCC 15.2.0, default Makefile `-O2` without LTO.
Three processes per version, five warmup batches per workload, 101 retained
batches per process. Every sample is a batch average, **not** an individual-call
tail. Timing includes assertions and a volatile sink. Raw CPU/wall samples,
binary/source hashes and environment are preserved in
`build/vorbis-primitives-baseline-20260908.json` and
`build/vorbis-primitives-optimized-20260908.json`.

| Workload | Baseline median ns | Optimized median ns | Optimized p95 ns |
|---|---:|---:|---:|
| Packed float 1.0 | 9.40 | 5.60 | 6.10 |
| Packed float raw=1 (underflow) | 745.35 | 7.40 | 9.80 |
| VQ 256 dimensions, sequence | 3760.50 | 3777.00 | 4079.00 |
| Floor1 65 points, 4096 bins | 3462.00 | 3610.00 | 4280.00 |

The only production optimization between these measurements replaces up to 788
power-of-two multiplications in float unpacking with at most ten binary
exponentiation iterations. Intermediates remain exact normal binary64 values;
the full numeric oracle and sanitizers were rerun. Underflow-case median fell
about 100.7x; this is **not** a 100x full-decoder improvement. VQ was essentially
unchanged; the unchanged floor workload measured 4.3% higher in this run, with
overlapping distributions. No acceptance threshold or real-time guarantee is
invented from these host samples. All outliers remain in the JSON.

## Next integration boundary

Update: [retained floor1 configuration and packet scalar reading](vorbis-floor1-packets.md)
are now implemented privately and share the setup validator's reader. The
full stream's setup/Huffman ownership and packet orchestration remain pending.

Retain and connect complete setup configuration and Huffman books, orchestrate
per-channel floor/residue decoding with the now-available mapping, coupling and
inverse-dB gain primitives, implement inverse MDCT and overlap, then expose
bounded PCM through the versioned adapter and validate OGG
on the official VM. Public ABI/features/version/pins remain unchanged. Existing
WAV VM regression evidence does not execute these private Vorbis functions.
