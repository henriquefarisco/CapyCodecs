# Test strategy

CapyCodecs tests should encode the compatibility contract as executable checks.

## Test organization

The image test suite is organized into focused files:

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

Planned next test files:

- `tests/image/test_detect.c`

## Golden fixtures

Golden fixtures validate successful decode output.

Each fixture should define:

- encoded bytes or fixture path;
- expected width;
- expected height;
- expected pixel hash;
- expected alpha behavior;
- required feature flags.

Initial golden set:

- BMP 1x1 24-bit;
- BMP 2x2 24-bit with row padding;
- BMP 32-bit;
- PNG RGB;
- PNG RGBA;
- PNG grayscale;
- JPEG baseline RGB;
- JPEG baseline grayscale.

BMP, PNG and JPEG baseline items are covered by `tests/fixtures/image/golden_image_fixtures.h` and `tests/image/test_golden.c`.

## Negative fixtures

Negative fixtures validate fail-closed behavior.

Required cases:

- empty input;
- truncated headers;
- truncated payloads;
- invalid magic bytes;
- invalid dimensions;
- unsupported compression;
- offset outside file;
- row stride overflow;
- chunk length overflow;
- invalid CRC;
- invalid marker sequence;
- missing required tables.

Initial negative coverage is implemented in `tests/fixtures/image/negative_image_fixtures.h` and `tests/image/test_negative.c`.

## Allocator failure matrix

For every successful fixture, tests should force allocator failure at each allocation index.

Expected behavior:

- decoder returns an error;
- output object is reset;
- no stale pointer remains;
- any temporary allocations are released through the injected free callback.

Initial allocator failure coverage is implemented in `tests/image/test_alloc_failures.c` for BMP, PNG and JPEG golden fixtures.

## PNG inflater failure tests

PNG inflater tests should cover:

- inflater callback returns an error;
- inflater callback returns success but produces a short output;
- output object reset after failure;
- temporary allocations released through the injected free callback.

Initial inflater failure coverage is implemented in `tests/image/test_inflater_failures.c`.

## Resource-limit tests

Resource tests should cover:

- width above maximum;
- height above maximum;
- width x height overflow;
- output byte count overflow;
- temporary memory budget exhaustion;
- animation frame limit exhaustion;
- metadata size limit exhaustion.

Initial resource-limit coverage is implemented in `tests/image/test_limits.c` for BMP, PNG and JPEG dimensions above the public defaults.

## Host adapter tests

Adapters should be tested outside the codec core.

Examples:

- zlib/miniz/tinf inflater adapter for PNG;
- CapyOS allocator adapter;
- CapyOS sandbox launch policy;
- release metadata compatibility checks.
