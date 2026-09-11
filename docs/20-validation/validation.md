# CapyCodecs validation

CapyCodecs validation is host-side and portable.

## Current coverage

Current coverage is compiled into `build/test_image_contracts` from focused image test files:

- `tests/image/test_image_contracts.c`
- `tests/image/test_image_common.c`
- `tests/image/test_image_abi.c`
- `tests/image/test_image_lifecycle.c`
- `tests/image/test_bmp.c`
- `tests/image/test_png.c`
- `tests/image/test_jpeg.c`
- `tests/image/test_golden.c`
- `tests/image/test_negative.c`
- `tests/image/test_alloc_failures.c`
- `tests/image/test_inflater_failures.c`
- `tests/image/test_limits.c`
- `tests/image/test_detect.c`
- `tests/image/test_metadata.c`
- `tests/image/test_qoi.c`

The current image-contract test covers:

- image ABI version, feature flags and default limits;
- allocator-injected free/reset behavior;
- fail-closed invalid input behavior and selected public error codes for BMP, PNG and JPEG entry points;
- 1x1 BMP RGB decode through the portable API;
- 1x1 PNG RGB decode through a fake inflater callback;
- golden BMP fixtures for 1x1 24-bit, 2x2 24-bit with row padding and 1x1 32-bit;
- golden PNG RGB, RGBA and grayscale fixtures through injected test inflaters;
- golden JPEG baseline grayscale and RGB fixtures with ARGB32 hash validation;
- negative BMP, PNG and JPEG fixtures for truncated data, invalid magic and unsupported modes;
- allocator failure matrix for BMP, PNG and JPEG golden fixtures;
- PNG inflater callback failure and short-output failure behavior;
- resource-limit rejection for BMP, PNG and JPEG dimensions above the public defaults;
- per-call limit enforcement through the `capy_*_decode_memory_limited` entry points: a tight limit rejects an otherwise-acceptable image, a relaxed limit accepts dimensions above the former PNG `1024` cap, and a NULL `limits` argument falls back to the documented defaults;
- format detection (`capy_image_detect_memory`) for BMP/PNG/JPEG magic, unknown/too-short prefixes and NULL arguments;
- generic decode dispatch (`capy_image_decode_memory`) routing BMP/PNG/JPEG/QOI to the limited entry points, plus fail-closed reset on unsupported magic and on a missing PNG inflater;
- header-only metadata query (`capy_image_query_memory`) reporting format/dimensions/channels/bit-depth/alpha for BMP/PNG/JPEG/QOI golden fixtures, zeroing metadata on unsupported/truncated/progressive headers and NULL arguments, plus query/decode dimension consistency;
- QOI decode (`capy_qoi_decode_memory`) with exact per-pixel checks across the RGB, RGBA, RUN, DIFF, LUMA and INDEX operations, plus fail-closed truncated-header, wrong-magic, corrupt end-marker, dimension-limit, truncated-stream and allocator-failure cases.

The PNG test intentionally avoids zlib or CapyOS `tinf` wiring. Real compressed PNG fixtures belong in a later host-adapter slice.

## Current release gate

The repository-level release gate is:

```sh
make validate
```

It currently runs:

- strict compile/lint checks;
- hardened compile checks;
- host-side image contract tests;
- release metadata checks.

## Required validation before CapyOS integration

Before CapyOS consumes a CapyCodecs release, validate:

- invalid/truncated image rejection;
- allocator failure paths;
- PNG inflater failure paths;
- golden BMP/PNG/JPEG fixtures;
- maximum dimension/resource limit behavior;
- no direct CapyOS kernel includes;
- no hidden `malloc`, `kalloc`, `kfree`, VFS or direct `tinf` usage.

## Validation expansion sequence

The private Ogg framing reader is included in `make test` and `make validate`.
See [its bounds and current evidence](../40-implementation/ogg-reader.md).
This does not advertise OGG/Vorbis decode support in the public audio ABI.

Private Vorbis identification/comment parsers are also included in those gates.
`make vorbis-reference-test` optionally compares them with installed libvorbis;
see [header validation evidence](../40-implementation/vorbis-headers.md).
The same gates now cover the private codebook parser, with an optional
libvorbis-generated oracle: [codebook evidence](../40-implementation/vorbis-codebooks.md).
The private complete setup-structure validator also participates, with 18
encoder-generated reference packets: [setup evidence](../40-implementation/vorbis-setup.md).
Private Huffman symbol decoding now participates as well, including exact
comparison with reference-encoded symbols: [Huffman evidence](../40-implementation/vorbis-huffman.md).
Private packed-float and vector expansion also participate, with independent
float and Xiph vector oracles: [VQ evidence](../40-implementation/vorbis-vq.md).
Prepared floor1 integer curves are covered by exhaustive endpoint and seeded
multipoint tests. Full-suite sanitizers and retained microbenchmark samples:
[floor1 and measurements](../40-implementation/vorbis-floor1.md).
Retained floor1 setup and packet-to-curve handoff now share the setup validator's
parser, with reference-encoded entropy and every-bit-prefix tests:
[floor1 packets](../40-implementation/vorbis-floor1-packets.md).
Private type-0/type-1 residue partitions now participate in the same gates,
including numeric/work budgets and distinct setup/audio truncation behavior:
[residue evidence](../40-implementation/vorbis-residue.md).
Private inverse channel coupling now participates as well, with reverse-order
pair application, atomic failure behavior and an independent randomized
specification oracle: [coupling evidence](../40-implementation/vorbis-coupling.md).
The floor1 inverse-dB substitution and spectrum dot product are now bounded and
atomic, with the official Xiph table pinned by packed binary32 hash and a
randomized product oracle: [floor gain evidence](../40-implementation/vorbis-floor1-gain.md).
Mapping type 0 is now retained rather than merely validated; the shared parser
is covered by full-range state tests and an independently encoded randomized
bitstream oracle: [mapping evidence](../40-implementation/vorbis-mapping.md).
The aggregate setup workspace now retains floor, residue, mapping and mode
state after successful framing, and the complete residue 0/1/2 scheduler is
covered by channel/pass/EOF tests plus a 2000-case independent randomized
oracle: [residue scheduling evidence](../40-implementation/vorbis-residue-decode.md).
Audio-packet mode selection and all four short/long transition boundaries are
also tested, including every focused truncated prefix:
[packet/window evidence](../40-implementation/vorbis-packet.md).
The freestanding Vorbis nested-sine window and variable-size overlap-add are
checked against Python `math.sin` for every legal size transition and measured
with retained raw host samples: [window evidence](../40-implementation/vorbis-window.md).
The fast inverse MDCT now covers every legal block size, including maximum-plan
bounds, overlap rejection and atomic failures. A direct-form independent oracle
checks its normalization and sample order:
[MDCT evidence](../40-implementation/vorbis-mdct.md).
The private post-entropy synthesis stage now connects coupling, floor1 gain,
inverse MDCT, windowing and overlap into transactional interleaved float PCM,
including first-packet priming and nominal packet-exhaustion behavior:
[synthesis evidence](../40-implementation/vorbis-synthesis.md).
Decoder-ready setup now retains bounded Huffman trees, and the private audio
packet orchestrator decodes floors plus per-submap residue bundles without
mixing residue type-2 channel domains:
[audio-packet evidence](../40-implementation/vorbis-audio-packet.md).
With `oggenc` and `ffmpeg` installed, `make vorbis-pcm-reference-test` encodes a
real deterministic stereo stream and compares the complete reference-length
float PCM body. This optional gate also exposes the known untrimmed tail until
Ogg granule positions are retained.

1. Expand corrupt fixtures for deeper codec-specific parser states.
2. Add deeper resource-limit tests for overflow and temporary-memory budgets.
3. Add fuzz harnesses.
4. Add sanitizer jobs.
5. Add differential tests against reference decoders.
6. Add benchmark and memory-budget reporting.
