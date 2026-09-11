# Vorbis vector expansion (development)

Private `vorbis_vq` expands one Huffman-selected codebook entry from the
immutable setup packet. Lookup types 1 and 2 and sequence accumulation are
implemented. The caller must supply a validated book and establish membership
of the selected symbol; unused sparse entries are not standalone valid inputs.

## Bounds and floating-point policy

- No heap, I/O, callbacks, libm or global mutable state. Caller buffers and
  descriptors must not overlap. Packet offsets have the same base as parsing.
- At most 65536 entries, 256 dimensions and 16 bits per multiplicand. A fixed
  256-float temporary keeps output unchanged on any error, including failure
  on the last component. The positive finite `max_abs` budget is explicit.
- Type 1 uses repeated quotient/remainder, not an overflowing growing divisor.
  Payload extent and every read are bounded by the supplied packet size.
- Packed floats follow the literal mantissa/sign/exponent formula in the
  [Vorbis specification](https://www.xiph.org/vorbis/doc/Vorbis_I_spec.html).
  Conversion uses binary64 intermediates, rejects values outside finite
  binary32 range, and permits gradual underflow. Each conversion is bounded
  by ten binary-exponentiation iterations. Intermediates remain exact normal
  binary64 values; zero mantissa yields zero.
- Compiling with `__FAST_MATH__` fails deliberately. The caller must initialize
  and preserve floating-point state, with normal round-to-nearest semantics
  and gradual underflow. This host evidence does not prove kernel FP handling.

The reference implementation
[libvorbis 1.3.7 sharedbook.c](https://raw.githubusercontent.com/xiph/vorbis/v1.3.7/lib/sharedbook.c)
clamps unpacked exponents to [-63,63]. Our float oracle compares Xiph only in
that unclamped domain. Outside it, an independent Python `math.ldexp` calculation
checks the literal formula and the explicit finite-range policy. Extreme values
therefore intentionally need not equal Xiph's clamped values. Vector comparisons
use ordinary values in the common domain; no broad bit-exact decoder claim follows.

## Evidence (2026-09-08)

- `make validate` passed, including existing image/audio/parser tests.
- Focused ASan/UBSan passed for float conversion, both lookup types, sequence
  accumulation, range rejection, truncation, invalid capacity/symbol/offset,
  NaN/infinite budgets, unaligned payload, maximum dimension and atomic output.
- `make vorbis-reference-test` passed all earlier oracles plus 110240 packed
  float patterns and 10880 vectors from 512 Xiph-generated books. Vector values
  matched exactly after mapping the reference decoder's sorted entries back
  to original symbols. Internal host reference ABI is pinned to 1.3.7.
- Freestanding GCC compilation passed. `nm -u` lists only private
  `capy_vorbis_bits_init` and `capy_vorbis_bits_read`; no libm/platform dependency.
- An initial test fixture encoded -3 incorrectly as `0xe0300000`. Xiph's packer
  confirmed `0xe0380000`; the fixture was corrected before the successful reruns.

Logs: `build/vorbis-vq-validation.log`, `build/vorbis-vq-sanitizers.log`,
`build/vorbis-vq-reference.log`, `build/vorbis-vq-freestanding.asm`.

This remains a private building block, not PCM decoding. Floor/residue,
mapping, coupling and floor-gain primitives now exist separately; persistent
aggregate setup/packet orchestration, inverse MDCT, overlap and player
integration remain.
No public ABI, version, feature bit, pin, release or VM playback gate changed.

The 2026-09-08 optimization, full-suite sanitizer rerun and measured baseline/
optimized CPU+wall distributions are documented with [floor1 evidence](vorbis-floor1.md).
