# Private residue partition synthesis (2026-09-08)

`vorbis_residue` decodes and adds **one partition** using a validated Huffman
tree and the existing VQ lookup primitive. Type 0 scatters vector components in
strides; type 1 adds sequentially. The separate
[complete scheduler](vorbis-residue-decode.md) now provides classword
unpacking, eight-pass scheduling, submap/channel skip handling and type-2
interleave orchestration over this primitive.

## Boundaries and ownership

- Caller owns immutable book/tree/setup packet, the bounded audio bit reader,
  initial output and scratch. All regions/descriptors must not overlap. Scratch
  has at least `count` floats and is unspecified on return; no heap is used.
- `count` is 1..32768 (room for a future flattened 8-channel block). Output and
  scratch each need at most 128 KiB at binary32 size. This is a per-partition
  bound, not a complete stream memory budget. The future orchestrator must also
  bound the aggregate work across channels, partitions and passes.
- `max_vectors` is checked before consuming bits. Type 0 needs floor(count/dim)
  vectors and leaves the remainder unchanged. Type 1 needs ceil(count/dim) and
  clips its final vector at count, matching the bounded Xiph helper.
- Every decoded vector and resulting accumulated value must be finite and
  within positive finite `max_abs`. Initial output is checked too. Binary64
  intermediates avoid overflowing before checking a binary32 sum. FP state is
  caller-owned, with the same rounding/underflow requirements as VQ. Fast-math
  compilation is rejected. No libm, filesystem, networking or callbacks.
- Hard failure leaves output unchanged and clears the result; consumed audio
  bits are not rolled back. Scratch may contain partial work and must not be
  exposed as output after a failure.

## Audio exhaustion is not setup corruption

The [Vorbis residue specification](https://www.xiph.org/vorbis/doc/Vorbis_I_spec.html)
permits audio-packet exhaustion and returns the vector progress already decoded.
This primitive commits complete vectors accumulated so far, returns success
with `exhausted=1`, and preserves the audio reader's sticky truncation flag.
The future orchestrator must stop residue decoding at that point. Unlike floor
exhaustion, this does not mean zeroing all previously decoded residue.

Truncation of the separate immutable **setup** packet during VQ expansion is a
hard error, never nominal audio exhaustion. Numeric/resource and malformed-tree
errors likewise do not become partial success.

Xiph's [type-0 helper](https://raw.githubusercontent.com/xiph/vorbis/v1.3.7/lib/codebook.c)
buffers all partition codewords before adding them, whereas its type-1 helper
adds per vector. Therefore successful partitions are compared to both reference
helpers, but truncated type-0 progress is intentionally checked against the
specification's vector-at-a-time semantics, not claimed equal to Xiph's helper.

## Executed validation

- `make validate` passed with the new partition tests.
- The whole owning test suite was rebuilt with `-O1 -g -fsanitize=address,undefined
  -fno-omit-frame-pointer` into `build/residue-sanitized` and passed.
- Unit cases cover both layouts, existing-output addition, odd/short tails,
  every prefix of the focused fixture, budget/capacity rejection, NaN/infinity,
  sum-range failure, setup-versus-audio truncation and preexisting reader errors.
  All hard errors preserve output; sentinels detect out-of-range writes.
- 10000 deterministic descriptor/input mutations passed. The maximum case
  decoded 32768 scalar vectors, consumed exactly 32768 bits and kept its guard.
- `make vorbis-reference-test` passed all earlier checks plus 240 complete
  partitions compared exactly with libvorbis 1.3.7, including consumed bits.
  Both map types, sequence flags, dimensions 1/2/4/8/16 and short/odd partition
  lengths were exercised. Another 2688 bit-prefix truncations matched the
  separate partial-progress oracle using independently unquantized vectors.
- GCC 15.2 freestanding `-O2 -mno-red-zone -fanalyzer -fstack-usage` passed.
  Disassembly was inspected. Only private Huffman/VQ functions are unresolved;
  the reported static frame is 1200 bytes, excluding called functions (notably
  the VQ expansion frame). No kernel execution or real-time latency is implied.

Logs: `build/vorbis-residue-validation.log`, `build/vorbis-residue-sanitizers.log`,
`build/vorbis-residue-reference.log`, `build/vorbis-residue-freestanding.asm`,
`build/vorbis_residue_freestanding.su`.

## Remaining integration

Residue configuration is now retained and all passes/types are scheduled.
Next, orchestrate the retained
[mapping](vorbis-mapping.md), [inverse coupling](vorbis-coupling.md) and
[floor gain](vorbis-floor1-gain.md) primitives before implementing inverse
MDCT/overlap and the bounded PCM adapter. Public ABI/features/pins/version
and the CapyOS player are unchanged. No new VM run was appropriate for this
private-only path; earlier WAV VM gates do not validate Vorbis playback.
