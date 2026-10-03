# Private floor1 gain application (2026-09-08)

`vorbis_floor1_gain` performs the two floor1 steps after integer curve
generation: it substitutes each 0..255 curve index using the normative
[Vorbis I inverse-dB table](https://xiph.org/vorbis/doc/Vorbis_I_spec.html#x1-15900010.1)
and multiplies that gain by the corresponding spectral residue scalar. The
table is the same binary32 constant set used by Xiph libvorbis 1.3.7. There is
no runtime `libm` dependency.

## Bounds and failure model

- The input is one floor1 index vector and one spectral vector of 1..4096 bins.
- The caller provides one float scratch vector. Index, spectrum and scratch
  regions must not overlap.
- Spectrum inputs and products must be finite and within positive finite
  `max_abs`. Fast-math is rejected.
- Products are prepared in scratch and committed only on complete success;
  every hard error leaves the spectrum byte-identical.
- No allocation, I/O, callback, global mutable state, platform API or PCM
  conversion is present.

## Validation

- Strict C11 tests cover exact low/middle/high table points, signed products,
  monotonicity across all 256 entries, maximum 4096-bin input, capacity and
  shape limits, NaN rejection, overlap rejection, sentinels and atomic failure.
- The packed binary32 table SHA-256 is pinned to the official Xiph 1.3.7 table:
  `3d48051a5d8e88104ebe447e328147ebbffa53e8b8e907878b77739ac5dc4b7d`.
- The independent host oracle checked that hash and 129827 randomized products
  bit-for-bit. The focused ASan/UBSan run, `make validate` and the complete
  `make vorbis-reference-test` suite passed. Freestanding GCC analysis reported
  no unresolved symbol and a 16-byte static frame.

This remains a private synthesis primitive. Persistent mapping/residue
orchestration, inverse MDCT, windowing, overlap/add, bounded PCM conversion and
CapyOS player integration are still required before any public Vorbis feature
or version change.
