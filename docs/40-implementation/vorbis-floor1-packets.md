# Private retained floor1 setup and packet values (2026-09-08)

`vorbis_floor1_packet` connects three existing private primitives: setup bit
reading, Huffman scalar decoding and [floor1 curve construction](vorbis-floor1.md).
It returns an owned-by-caller configuration containing partition classes,
dimensions, master/subclass book references and a prepared prediction/render
plan. The setup validator now uses this same reader instead of maintaining a
duplicate floor1 parser. That validator still discards configuration after
validation; a full persistent stream decoder is not yet connected.

## Contract and failure behavior

- Config reading starts after the 16-bit floor type, which the caller must
  establish as type 1. Setup truncation is an error. Config output clears on
  failure and becomes immutable after success; no setup packet pointer is kept.
- Packet reading starts at the channel's floor-present bit, not the whole
  packet header. The caller supplies immutable Huffman trees with matching
  book count and owns all lifetimes. Buffers/descriptors must not overlap.
- No heap, I/O, floating-point arithmetic, callbacks or platform dependencies.
  At most 31 partitions, 16 classes, 65 values and 256 book references. Decoding
  requires at most 94 bounded Huffman calls (63 values plus 31 selectors).
- Success with `present=1` publishes all scalar values together. The caller may
  pass them to `capy_vorbis_floor1_curve`. A normal absent floor sets both
  `present=0` and `exhausted=0` and consumes only the presence bit.
- Per [Vorbis packet/floor semantics](https://www.xiph.org/vorbis/doc/Vorbis_I_spec.html),
  end-of-packet during floor reading is nominal, not a malformed setup. It
  returns success with `present=0`, `exhausted=1`, zeroed values, and retains the
  bit reader's sticky truncation flag. The eventual packet orchestrator MUST
  zero **all channel vectors**, skip residue and proceed to overlap. It must
  not interpret exhaustion as merely one silent channel and continue decoding.
- Structural corruption, invalid book references and out-of-range amplitudes
  remain errors, with cleared output; they are not converted to exhaustion.
  The immutable config's bounded descriptors are checked before packet reads.

No public ABI, feature bits, pins, version, package or runtime consumer changed.
This is not a PCM decoder and does not support floor0 synthesis. Private
inverse-dB gain, residue, retained mapping and coupling primitives now exist;
full persistent setup/packet orchestration, inverse MDCT/overlap and bounded PCM
remain to be integrated.

## Executed evidence

- `make validate` passed, including the setup validator using the shared reader.
- Full repository suite rebuilt with ASan/UBSan into `build/floor-packet-sanitized`
  passed (same sanitizer CFLAGS documented in the floor1 measurement report).
- Focused tests cover class selection, unused subclass books, exact Y values,
  curve handoff, all truncated bit prefixes, absent floor, preexisting errors,
  malformed Huffman nodes, invalid references/amplitudes and 10000 deterministic
  setup/packet mutations. No partial scalar output survived failures.
- `make vorbis-reference-test` passed all previous oracles, including the 18
  real encoder setup packets and their truncated prefixes. The new optional
  Xiph 1.3.7 Huffman encoder generated 256 floor fixtures: all 7701 scalars and
  exact consumed bits matched; 153928 setup/audio bit-prefix truncations passed.
  Fixtures cover all multipliers/subclass widths, class-number holes, zero
  partitions, unused books and seeded partition sizes. Resulting curves also
  matched the independent specification oracle. This is reference-encoded
  entropy evidence, not an independent full libvorbis floor or PCM comparison.
- Freestanding GCC 15.2 `-O2 -mno-red-zone -fstack-usage` and `-fanalyzer` passed.
  Disassembly was inspected. Undefined functions are only the private bit reader,
  floor plan preparation and Huffman decoder. Per-function static stack is 944
  bytes for config reading and 416 bytes for packet reading; these numbers do
  not include callees or whole-stream storage.

Logs: `build/vorbis-floor-packet-validation.log`,
`build/vorbis-floor-packet-sanitizers.log`,
`build/vorbis-floor-packet-reference.log`,
`build/vorbis-floor-packet-freestanding.asm`,
`build/vorbis_floor1_packet_freestanding.su`.

No VM was rerun for this private-only change: existing WAV runtime does not
execute these functions. OGG runtime acceptance remains open; earlier WAV VM
evidence must not be presented as coverage for this decoder path.

Follow-up: [type-0/type-1 partition synthesis](vorbis-residue.md) is now available
privately. It does not yet provide the full residue/channel/pass orchestrator.
