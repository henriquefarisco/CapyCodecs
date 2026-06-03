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
- `src/image/detect.c`
- `src/image/metadata.c`
- `src/image/qoi_decode.c`
- `tests/image/test_image_contracts.c`
- `docs/10-contracts/compatibility.md`
- `docs/20-validation/validation.md`

## Current state

- **Version:** `0.0.7`.
- **Owned ABI:** `capy-codec-image` (`CAPY_IMAGE_ABI_VERSION` = `2`).
- **Implemented entry points:** BMP, PNG, JPEG and QOI memory decoders, each with a default-limit wrapper and a `*_decode_memory_limited` variant that takes a caller-provided `capy_image_limits`; plus `capy_image_detect_memory` (magic-byte detection), `capy_image_decode_memory` (generic dispatch) and `capy_image_query_memory` (header-only metadata).
- **Current validation gate:** `make validate`.
- **Current test harness:** modular image contract tests under `tests/image/`.
- **Current golden coverage:** BMP, PNG, JPEG baseline and QOI fixtures.
- **Current failure coverage:** fail-closed negative fixtures, allocator failure matrix, PNG inflater failures, public dimension-limit rejection and per-call limit enforcement.
- **Current default image limit:** `4096 x 4096` pixels (uniform across BMP/PNG/JPEG; PNG no longer carries a separate `1024` cap).
- **Current PNG model:** inflater callback injected by the host.

## Immediate gaps

- **Fixtures:** Deeper corrupt/truncated fixture sets and JPEG sampling variants remain pending.
- **Resource limits:** Per-call dimension, output-byte and temporary-byte budgets are now enforced; deeper integer-overflow corpus cases remain pending.
- **ABI detail:** Format detection and generic decode entry points remain pending.
- **Adapters:** CapyOS integration should consume releases through the ABI, not internal source paths.
