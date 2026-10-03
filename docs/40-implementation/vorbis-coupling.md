# Private Vorbis inverse coupling (2026-09-08)

`vorbis_coupling` implements the inverse channel-coupling stage defined by the
[Vorbis I specification](https://xiph.org/vorbis/doc/Vorbis_I_spec.html#x1-790004.3.5).
It transforms magnitude/angle residue vectors back into channel spectral
vectors, applying coupling pairs in the required reverse setup order. It does
not parse a mapping, apply a floor, perform IMDCT/overlap or produce PCM.

## Bounds and failure model

- Inputs use caller-owned channel-major float storage, with at most 8 channels,
  4096 spectral bins and 256 coupling steps.
- Pair references are checked before any vector is copied. Each pair must name
  two distinct valid channels.
- Every input and derived scalar must be finite and within the caller's positive
  `max_abs` limit. Fast-math compilation is rejected.
- The caller supplies at most 128 KiB of compact scratch at the maximum shape.
  Transformation happens in scratch and commits only after all pairs pass, so a
  hard error leaves the original vectors byte-identical. Channel-stride padding
  is never modified.
- Vectors, scratch and descriptors are explicit non-overlapping regions. There
  is no heap, I/O, callback, global mutable state or platform dependency.

## Validation

- Focused strict C11 unit tests cover all four sign quadrants, zero, normative
  reverse pair order, channel padding, invalid references, capacity/shape
  limits, NaN rejection, overlapping storage and unchanged output on failure.
- 10000 deterministic C trials checked the formula and sentinels.
- An independent specification oracle compared 330501 spectral scalars across
  2000 deterministic randomized mappings, 2..8 channels, 1..64 bins, padded
  strides and up to 16 repeated coupling steps. Results matched bit-for-bit.
- The focused test passed ASan/UBSan. `make validate` and the complete
  `make vorbis-reference-test` suite passed. Freestanding GCC analysis reported
  no unresolved symbol and a 40-byte static frame.

## Remaining path to PCM

The packet orchestrator must retain mapping state, propagate nonzero vectors,
schedule residue types 0/1/2, invoke this primitive, apply the inverse-dB floor,
then perform inverse MDCT, windowing and overlap/add. Only after differential
PCM evidence may Vorbis be advertised through the public `capy-codec-audio` ABI
or consumed by the CapyOS player.
