# Status tracking

This document tracks planned codec work by milestone.

| Milestone | Status | Acceptance criteria | Related areas |
| --- | --- | --- | --- |
| Image ABI hardening | Ready | Public errors, ABI query (v2), default limits and per-call limited entry points are documented and tested | `src/image/capy_image.h`, `docs/10-contracts/image-abi.md` |
| Test harness split | Ready | Tests are organized by codec and contract area in a single contract binary | `tests/image/` |
| Golden fixtures | In progress | BMP, PNG and JPEG baseline fixtures validate dimensions and pixel hashes; broader variant fixtures remain pending | `tests/fixtures/` |
| Negative fixtures | In progress | Initial invalid magic, truncated data and unsupported-mode fixtures fail closed for BMP/PNG/JPEG | `tests/fixtures/` |
| Allocator failure matrix | In progress | BMP, PNG and JPEG golden fixtures force allocator failure at each allocation index | `tests/image/test_alloc_failures.c` |
| PNG inflater failures | Ready | Callback errors and short output fail closed and release temporaries | `tests/image/test_inflater_failures.c` |
| Resource-limit tests | In progress | BMP/PNG/JPEG dimension rejection and per-call limit enforcement (tight-rejects / relaxed-accepts / NULL-defaults) are covered; deeper integer-overflow corpus cases remain pending | `tests/image/test_limits.c` |
| BMP robust decode | Planned | Common BMP variants decode or reject deterministically | `src/image/bmp_decode.c` |
| PNG robust decode | Planned | PNG chunks, CRC, filters and inflater failures are covered | `src/image/png_decode.c` |
| JPEG baseline decode | In progress | Baseline grayscale and RGB golden fixtures decode; broader sampling/table cases remain pending; unsupported modes reject clearly | `src/image/jpeg_decode.c` |
| CapyOS image adapter | Planned | CapyOS consumes `capy-codec-image` via adapter, not source paths | CapyOS integration |
| Image format dispatch | Ready | `capy_image_detect_memory` and `capy_image_decode_memory` exist additively, route BMP/PNG/JPEG and fail closed on unknown magic; covered by `tests/image/test_detect.c` | `src/image/detect.c` |
| Image metadata query | Ready | `capy_image_query_memory` + `struct capy_image_metadata` report format/dimensions/channels/bit-depth/alpha from headers without decoding; covered by `tests/image/test_metadata.c` | `src/image/metadata.c` |
| Modern image formats | In progress | QOI decodes through `capy_qoi_decode_memory` and is wired into detection/dispatch/metadata (`tests/image/test_qoi.c`); ICO/CUR, GIF/APNG and optional WebP/AVIF remain staged | `src/image/qoi_decode.c` |
| Audio ABI | Planned | `capy-codec-audio` defines PCM output and ownership | future `src/audio/` |
| Container ABI | Planned | Packets/timestamps are separated from codec decode | future `src/container/` |
| Video ABI | Planned | Incremental frame decode API is documented | future `src/video/` |
| Fuzzing | Planned | Fuzz harnesses cover parser surfaces | `tests/fuzz/` |
| Release engineering | Planned | Compatibility index and rollback metadata are documented | `docs/40-implementation/release-engineering.md` |

## Status meanings

- **Planned:** Work is specified but not started.
- **In progress:** Implementation or documentation is actively changing.
- **Blocked:** Work depends on another milestone or external decision.
- **Ready:** The milestone meets its acceptance criteria.

## Update rule

A milestone should only move to `Ready` when tests, documentation and compatibility impact are all addressed.
