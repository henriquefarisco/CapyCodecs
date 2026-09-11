# Retained Vorbis mapping type 0 state (2026-09-09)

`vorbis_mapping` replaces the setup validator's discard-only mapping parser
with one shared private parser that retains the state required by audio packet
synthesis. It implements mapping type 0 from the
[Vorbis I setup specification](https://xiph.org/vorbis/doc/Vorbis_I_spec.html#x1-650004.2.4):
submaps, coupling pairs, channel mux assignments and per-submap floor/residue
references. The setup validator now calls this same implementation.

## Bounds and failure model

- State is fixed-size: at most 8 channels, 16 submaps and 256 coupling steps.
- Counts for floor and residue references come from already-validated setup
  sections and are capped at the format maximum of 64.
- Mapping type, reserved bits, pair distinctness/range, mux range and
  floor/residue references fail closed.
- The specified unused time byte is consumed but deliberately ignored.
- Output is zero on every error. Bit consumption is sticky and is not rolled
  back, matching the existing bounded setup bit reader.
- There is no allocation, I/O, callback, synthesis or public ABI exposure.

## Validation

- Focused C tests cover retained two-submap state, default mapping behavior,
  the full 256-step representation, every truncated bit prefix, all reference
  classes, ignored time data and zeroed failure output.
- An independent encoder/oracle produced 5000 deterministic randomized valid
  mappings across all supported channel/submap/count ranges and checked every
  retained field plus exact consumed-bit position. Another 14472 truncated bit
  prefixes failed closed with zero output.
- The pre-existing setup tests pass through the refactored parser, proving that
  the complete setup validator retains its behavior.
- The focused mapping test passed ASan/UBSan. `make validate` and the complete
  `make vorbis-reference-test` suite passed. Freestanding GCC analysis reported
  only the expected private bit-reader dependency and a 672-byte static frame.

Modes and the other setup components still need an owned aggregate decoder
state. Packet-level nonzero propagation and residue scheduling must consume this
mapping before inverse coupling and floor gain. IMDCT/window/overlap and bounded
PCM remain subsequent stages.
