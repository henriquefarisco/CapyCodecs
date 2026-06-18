# Image ABI contract

The `capy-codec-image` ABI defines how portable image decoders expose decoded pixel output to CapyOS and other hosts.

## Public surface

The current public header is `src/image/capy_image.h`.

The ABI currently includes (`CAPY_IMAGE_ABI_VERSION` = `2`):

- `CAPY_IMAGE_ABI_VERSION`
- `enum capy_image_error`
- `enum capy_image_format`
- `capy_image_abi_version`
- `capy_image_codec_features` (now also reports `CAPY_IMAGE_FEATURE_PER_CALL_LIMITS`, `CAPY_IMAGE_FEATURE_DETECT` and `CAPY_IMAGE_FEATURE_GENERIC_DECODE`)
- `capy_image_default_limits`
- `capy_bmp_decode_memory`
- `capy_bmp_decode_memory_limited`
- `capy_png_decode_memory`
- `capy_png_decode_memory_limited`
- `capy_jpeg_decode_memory`
- `capy_jpeg_decode_memory_limited`
- `capy_qoi_decode_memory`
- `capy_qoi_decode_memory_limited`
- `capy_image_detect_memory`
- `capy_image_decode_memory`
- `capy_image_query_memory`
- `capy_image_strerror`
- `capy_image_format_name`
- `capy_image_rgba32_free`
- `struct capy_image_allocator`
- `struct capy_image_inflater`
- `struct capy_image_rgba32`
- `struct capy_image_limits`
- `struct capy_image_metadata`

The `*_limited` entry points accept a `const struct capy_image_limits *`
just before `out`. The original `capy_*_decode_memory` entry points keep
their signatures and now delegate to the limited variants with
`capy_image_default_limits`. A NULL `limits` argument means "use the
defaults", so the two families stay behaviourally identical at default
limits.

`capy_image_detect_memory` sniffs the leading magic bytes and reports a
`enum capy_image_format` (`BMP`, `PNG`, `JPEG`, `QOI` or `UNKNOWN`)
without allocating. `capy_image_decode_memory` is a generic dispatcher: it
detects the format and routes to the matching `*_decode_memory_limited`
entry point, threading the allocator, the (PNG-only) inflater and the
optional limits through. Unknown magic resets `out` and returns
`CAPY_IMAGE_ERR_UNSUPPORTED_FORMAT`; a PNG buffer with no inflater fails
closed through the PNG decoder with `CAPY_IMAGE_ERR_INVALID_ARGUMENT`.

`capy_image_query_memory` parses only the container header (no
allocation, no resource limits) and fills `struct capy_image_metadata`
with `format`, `width`, `height`, source `channels`, `bits_per_channel`
and `has_alpha`. `has_alpha` describes the decoded ARGB32 output: it is
`1` only when the output may carry non-opaque alpha (PNG colour types 4
and 6); BMP and JPEG always decode opaque, so a 32-bit BMP reports
`channels == 4` with `has_alpha == 0` (its source alpha is currently
discarded by the decoder). The header acceptance checks mirror the
decoders, so a query that returns `CAPY_IMAGE_OK` implies the header is
decodable — a later decode may still fail on resource limits, a missing
PNG inflater, allocation or corrupt entropy data. On any error the
metadata is zeroed and `format` is `CAPY_IMAGE_FORMAT_UNKNOWN`.

## Output ownership

A successful decode returns a populated `capy_image_rgba32` object.

The output object owns:

- width;
- height;
- pixel pointer;
- allocator used to release the pixel buffer.

Hosts must release successful outputs with `capy_image_rgba32_free`.

## Failure behavior

When an output pointer is valid, every decoder must reset the output object on failure.

A failed decode must not expose partially decoded images, stale pixel pointers or allocator state that implies ownership of invalid memory.

Current public image error codes are:

- `CAPY_IMAGE_OK`
- `CAPY_IMAGE_ERR_INVALID_ARGUMENT`
- `CAPY_IMAGE_ERR_UNSUPPORTED_FORMAT`
- `CAPY_IMAGE_ERR_CORRUPT_DATA`
- `CAPY_IMAGE_ERR_TRUNCATED_DATA`
- `CAPY_IMAGE_ERR_OUT_OF_MEMORY`
- `CAPY_IMAGE_ERR_RESOURCE_LIMIT`
- `CAPY_IMAGE_ERR_INFLATER_FAILED`

## Pixel contract

Current image output is a 32-bit pixel buffer documented by the compatibility contract as ARGB32.

Implementation work that changes channel order, alpha semantics or premultiplication policy must be treated as an ABI-impacting change and documented before release.

## ABI hardening status

Implemented additively:

- public ABI version query;
- public codec feature query;
- official error-code enum compatible with `int` returns;
- default resource-limit query;
- decoder entry points that accept explicit per-call resource limits
  (`capy_*_decode_memory_limited`), enforcing dimension, output-byte and
  temporary-byte budgets before large allocations;
- format detection API (`capy_image_detect_memory` + `enum capy_image_format`);
- generic image decode API (`capy_image_decode_memory`) that detects and
  dispatches to the per-codec limited entry points;
- header-only metadata query API (`capy_image_query_memory` +
  `struct capy_image_metadata`);
- QOI decode (`capy_qoi_decode_memory` + `capy_qoi_decode_memory_limited`),
  wired into detection, generic decode and metadata;
- fail-closed decoder behavior covered for BMP, PNG, JPEG and QOI;
- public dimension-limit rejection covered for BMP, PNG, JPEG and QOI.

The core image ABI hardening surface (version query, feature query,
error model, resource limits, detection, generic decode and metadata
query) is now complete. Remaining image work is feature breadth rather
than ABI hardening:

- richer colour management (sRGB/gamma/ICC) and orientation policy;
- additional formats (ICO/CUR, GIF/APNG, optional WebP/AVIF);
- broader BMP/PNG/JPEG sub-format coverage (palettes, bit depths, CRC).

## Additive evolution examples

Safe additive changes include:

- adding new functions;
- adding new enum values;
- adding new optional structs passed through new functions;
- adding feature query flags;
- adding new codecs behind new entry points.

Unsafe changes include:

- reordering public struct fields;
- changing ownership rules;
- changing pixel channel order silently;
- changing existing function signatures;
- making a decoder call hidden allocators or platform IO.
