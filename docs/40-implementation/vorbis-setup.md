# Vorbis setup validation (development)

The private `vorbis_setup` validator walks the complete setup packet structure
described by [Xiph Vorbis I sections 4.2.4, 6, 7 and 8](https://www.xiph.org/vorbis/doc/Vorbis_I_spec.html).
It checks codebooks, time placeholders, floors 0/1, residues 0/1/2, mapping 0,
modes and final framing. Cross-references are checked against actual counts;
vector contexts require value-mapped codebooks. Floor1 points must be unique
and no more than 65; coupling channels must be valid and distinct. The unused
mapping time byte is deliberately ignored as specified.

## Scope and bounds

This is retained decoder setup state, not audio-packet orchestration or PCM
synthesis. On success the caller-owned workspace retains codebook descriptors,
floor types and Floor0/Floor1 configurations, residue classifications and all
eight stage-book references, mapping submaps/coupling/mux references, and mode
blockflags/mapping references. Codebook lookup values remain borrowed from the
immutable setup packet. The decoder-ready `capy_vorbis_setup_prepare` variant
additionally builds each codebook's Huffman tree before the reusable length
scratch is overwritten. Tree nodes come from one bounded caller-owned pool,
and retained tree pointers borrow that pool. Failure resets the published
summary and node count; partial workspace, tree and node storage must be
discarded.
The caller supplies channel count from validated identification, immutable
packet memory, a fixed workspace of 256 book descriptors and one reusable
length buffer. Packet/header ordering and Ogg mapping are caller obligations.
On hard failure the summary remains zero and partial workspace state must be
discarded; packet bits are not rolled back. No large transactional copy is made,
which keeps stack use bounded while retaining a fixed-capacity state object.

The input-byte cap, book count, aggregate entries and aggregate lookup-value
budgets are explicit. Per-book budgets are narrowed to the remaining aggregate
allowance before parsing. There is no heap, vector expansion, filesystem,
network or platform callback. Remaining section counts and loop bounds follow
the format's finite field widths. The aggregate totals constrain setup parsing,
not future synthesis allocations; a decoder-wide byte budget is still required.

Metadata is published only after framing succeeds and is zero on failure.
Partial scratch state must not be used after error. All-unused codebooks remain
rejected under the previously documented strict codebook policy. This validator
does not claim exhaustive bitstream compliance or interpret audio packet data.

## Evidence (2026-09-06)

- `make validate` passed image/WAV/Ogg/header/codebook/setup tests.
- Focused ASan/UBSan setup tests passed: both floor types, all residue types,
  aggregate budget exhaustion, invalid references/reserved fields, duplicate
  floor points, invalid coupling/mux, mode mapping, reversed residue ranges,
  missing framing, all fixture byte-prefix truncations and 10000 deterministic
  mutations (not coverage-guided fuzzing).
- `make vorbis-reference-test` passed the prior 317 header and 48 book cases,
  plus 18 real setup packets generated in memory with libvorbisenc 1.3.7:
  mono/stereo, 22050/44100/48000 Hz, quality 0.0/0.5/1.0. Each full packet was
  accepted and every truncated byte prefix was rejected. The encoder is an
  optional host-test dependency, not linked into production.
- Freestanding compilation passed. Its only unresolved functions are the
  three private CapyCodecs bitreader/codebook primitives, not host/runtime APIs.

Logs: `build/vorbis-setup-validation.log`, `build/vorbis-setup-sanitizers.log`,
`build/vorbis-setup-reference.log`. Reference test recipes use `python3 -B` to
avoid dirtying the source tree with bytecode caches.

No public ABI/version/pin/release changed. No VM run or Vorbis playback is
claimed. Next: full residue pass/channel orchestration over this retained state,
inverse MDCT/overlap, differential PCM fixtures, then consumer integration and
QEMU/VMware gates.

The private [Huffman tree/symbol primitive](vorbis-huffman.md) is now implemented
and reference-tested. It is not yet wired into complete decoder state or PCM.
