# Image codec implementation guide

Image codecs must implement `capy-codec-image` without platform dependencies.

## Shared implementation rules

- Validate all pointers before use.
- Reset valid output objects before returning an error.
- Use only `capy_image_allocator` for owned buffers.
- Check integer arithmetic for overflow before allocating or indexing.
- Validate dimensions before computing output size.
- Keep decoder temporary memory bounded.
- Reject unsupported features explicitly.
- Keep parsing helpers small and deterministic.

## Output model

The current output object is `struct capy_image_rgba32`.

A successful decode should set:

- `width`;
- `height`;
- `pixels`;
- `allocator`.

A failed decode should leave the output reset.

## Size and stride calculations

Every codec should use checked calculations for:

- `width * height`;
- `pixel_count * sizeof(uint32_t)`;
- source row stride;
- destination row stride;
- chunk or segment offsets;
- temporary buffer sizes.

A calculation overflow must return a resource or corrupt-data error before allocation.

## BMP implementation path

Implement BMP in increasing complexity:

1. Validate `BM` signature.
2. Parse file header and DIB header.
3. Validate pixel offset and declared file size.
4. Support Windows V3 headers first.
5. Decode 24-bit BGR with row padding.
6. Decode 32-bit BGRA/BGRX.
7. Support top-down and bottom-up rows.
8. Add paletted 1/4/8-bit images.
9. Add 16-bit RGB565/RGB555.
10. Add BITFIELDS only after masks are validated.

Reject safely:

- RLE until implemented;
- unsupported DIB headers;
- invalid palette length;
- negative width;
- impossible row stride;
- pixel data outside the input buffer.

## PNG implementation path

Implement PNG in increasing complexity:

1. Validate PNG signature.
2. Parse chunks with length and CRC checks.
3. Require one valid IHDR first.
4. Validate IHDR dimensions, bit depth, color type, compression, filter and interlace.
5. Collect IDAT chunks within memory limits.
6. Require IEND.
7. Inflate IDAT through host-provided callback.
8. Apply scanline filters.
9. Convert color types into output pixels.
10. Add PLTE and tRNS.
11. Add Adam7 only after non-interlaced PNG is complete.

Required filters:

- None;
- Sub;
- Up;
- Average;
- Paeth.

Reject safely:

- invalid chunk order;
- duplicate IHDR;
- missing IEND;
- unsupported interlace before Adam7;
- invalid filter byte;
- decompressed size mismatch;
- inflater callback failure.

## JPEG implementation path

Implement JPEG baseline in increasing complexity:

1. Validate SOI.
2. Iterate markers with length checks.
3. Skip APPn and COM safely.
4. Parse DQT.
5. Parse DHT.
6. Parse SOF0.
7. Parse SOS.
8. Decode entropy-coded data.
9. Dequantize coefficients.
10. Run IDCT.
11. Convert YCbCr or grayscale into output pixels.
12. Validate EOI.

Reject safely:

- progressive SOF markers until supported;
- arithmetic coding;
- unsupported component counts;
- unsupported sampling modes;
- missing quantization or Huffman tables;
- truncated entropy data.

## Animated image path

Before adding GIF/APNG animation, add explicit frame APIs:

- decoder creation/destruction;
- frame count or unknown-frame marker;
- next-frame decode;
- frame duration;
- blend/dispose operation;
- canvas size;
- total memory/frame limits.

Static decode APIs should keep returning a single image.
