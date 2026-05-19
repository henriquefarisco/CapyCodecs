# Image ABI contract

The `capy-codec-image` ABI defines how portable image decoders expose decoded pixel output to CapyOS and other hosts.

## Public surface

The current public header is `src/image/capy_image.h`.

The ABI currently includes:

- `CAPY_IMAGE_ABI_VERSION`
- `enum capy_image_error`
- `capy_image_abi_version`
- `capy_image_codec_features`
- `capy_image_default_limits`
- `capy_bmp_decode_memory`
- `capy_png_decode_memory`
- `capy_jpeg_decode_memory`
- `capy_image_rgba32_free`
- `struct capy_image_allocator`
- `struct capy_image_inflater`
- `struct capy_image_rgba32`
- `struct capy_image_limits`

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
- fail-closed decoder behavior covered for BMP, PNG and JPEG;
- public dimension-limit rejection covered for BMP, PNG and JPEG.

Remaining hardening targets:

- decoder entry points that accept explicit per-call resource limits;
- format detection API;
- generic image decode API;
- metadata query API.

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
