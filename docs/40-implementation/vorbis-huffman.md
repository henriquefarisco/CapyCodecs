# Vorbis Huffman symbol decoding (development)

The private `vorbis_huffman` primitive builds an owned-by-caller decode tree
from entry-ordered lengths and reads symbols using the bounded Vorbis bit reader.
It follows [Xiph's codeword assignment and single-entry erratum](https://www.xiph.org/vorbis/doc/Vorbis_I_spec.html).
Lengths are not sorted into conventional canonical Huffman order: each entry
receives the leftmost available prefix of its requested length in entry order.

## Ownership and cost

- No heap, I/O, callbacks or hidden global state. The caller supplies immutable
  lengths and writable nodes; node memory must stay alive and immutable after
  successful construction. Buffers/state/output must not overlap.
- At most 65536 entries, lengths 1..32 (zero means unused), and `2*used-1`
  nodes. The single-active-symbol exception needs one node and consumes one bit
  on each decode, accepting either bit value.
- Tree construction uses fixed 33-slot prefix/count work arrays, at most 32
  operations per entry in each bounded traversal, and validates tree completeness
  before publishing output. Insufficient capacity and malformed lengths fail.
- Decoding takes at most 32 bits and checks node indexes. Failed reads preserve
  the bit-reader error and return no symbol (`UINT32_MAX`).
- Partial node storage is invalid after build failure; the output tree resets.

This does not yet connect the setup validator to a complete decoder state.
[Private VQ expansion](vorbis-vq.md), floor/residue, retained mapping, inverse
coupling and floor-gain primitives are now available separately; aggregate
packet orchestration, inverse MDCT, overlap and PCM output remain pending.
ABI/version/features/pins and the CapyOS player are unchanged.

## Evidence (2026-09-07)

`make validate` and focused ASan/UBSan passed. Tests cover the specification's
irregular entry-order example, exact consumed bits, every truncated codeword,
undersized node storage, invalid/empty trees, sparse single-entry behavior,
maximum depth 32, and 10000 permutations of valid length sets.

`make vorbis-reference-test` retained all earlier checks and passed 1746 symbols
encoded by libvorbis 1.3.7 across 17 books. Every symbol and consumed bit count
matched. Two additional sparse single-symbol cases matched the reference decoder.
The first oracle attempt called the encoder on a sparse single-entry book;
libvorbis returned a null codeword table despite an initialization success code,
and its encode function crashed. The test now checks that table and exercises
this exception through the decoder API. No CapyCodecs acceptance check was removed.

Freestanding compilation passed; the only unresolved function is the private
`capy_vorbis_bits_read`, not a platform API. Logs:
`build/vorbis-huffman-validation.log`, `build/vorbis-huffman-sanitizers.log`,
`build/vorbis-huffman-reference.log` (successful rerun).

No new VM or PCM reproduction gate is claimed. Reference libraries remain
optional host-only dependencies, with their internal ABI gated to 1.3.7.
