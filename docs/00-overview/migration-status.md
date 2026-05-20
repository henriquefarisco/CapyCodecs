# Migration status

CapyCodecs currently contains the first portable image codec slice migrated out of CapyOS.

## Migrated from CapyOS

- `CapyOS/include/gui/bmp_loader.h`
- `CapyOS/src/gui/core/bmp_loader.c`
- `CapyOS/include/gui/png_loader.h`
- `CapyOS/src/gui/core/png_loader.c`
- `CapyOS/include/gui/jpeg_loader.h`
- `CapyOS/src/gui/core/jpeg_loader.c`

## Created in CapyCodecs

- `src/image/capy_image.h`
- `src/image/image.c`
- `src/image/bmp_decode.c`
- `src/image/png_decode.c`
- `src/image/jpeg_decode.c`
- `tests/image/test_image_contracts.c`
- `docs/10-contracts/compatibility.md`
- `docs/20-validation/validation.md`

## Current state

- **Version:** `0.0.3`.
- **Owned ABI:** `capy-codec-image`.
- **Implemented entry points:** BMP, PNG and JPEG memory decoders.
- **Current validation gate:** `make validate`.
- **Current test harness:** modular image contract tests under `tests/image/`.
- **Current golden coverage:** BMP, PNG and JPEG baseline fixtures.
- **Current failure coverage:** fail-closed negative fixtures, allocator failure matrix, PNG inflater failures and public dimension-limit rejection.
- **Current image limit:** `4096 x 4096` pixels.
- **Current PNG model:** inflater callback injected by the host.

## Immediate gaps

- **Fixtures:** Deeper corrupt/truncated fixture sets and JPEG sampling variants remain pending.
- **Resource limits:** Overflow and temporary-memory budget cases remain pending.
- **ABI detail:** Explicit per-call resource-limit entry points remain pending.
- **Adapters:** CapyOS integration should consume releases through the ABI, not internal source paths.
