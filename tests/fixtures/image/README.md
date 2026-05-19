# Image fixtures

Image fixtures are small, explicit and host-side.

## Golden fixtures

Current golden fixtures live in `golden_image_fixtures.h` as C byte arrays.

They cover:

- BMP 1x1 24-bit;
- BMP 2x2 24-bit with row padding;
- BMP 1x1 32-bit;
- PNG 1x1 RGB through the injected test inflater;
- PNG 1x1 RGBA through the injected test inflater;
- PNG 1x1 grayscale through the injected test inflater;
- JPEG 1x1 baseline grayscale;
- JPEG 1x1 baseline RGB.

Golden output is validated with an ARGB32 FNV-1a pixel hash.

## Pending fixtures

Pending golden coverage:

- additional JPEG sampling and table variants.

Current negative coverage:

- truncated headers;
- truncated payloads;
- invalid magic bytes;
- unsupported compression or marker modes.

Pending negative coverage:

- invalid dimensions;
- invalid offsets;
- deeper codec-specific marker and table errors.
