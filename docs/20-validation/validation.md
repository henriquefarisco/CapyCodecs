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

1. Expand corrupt fixtures for deeper codec-specific parser states.
2. Add deeper resource-limit tests for overflow and temporary-memory budgets.
3. Add fuzz harnesses.
4. Add sanitizer jobs.
5. Add differential tests against reference decoders.
6. Add benchmark and memory-budget reporting.
