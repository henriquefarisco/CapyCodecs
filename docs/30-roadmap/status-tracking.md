# Status tracking

This document tracks planned codec work by milestone.

| Milestone | Status | Acceptance criteria | Related areas |
| --- | --- | --- | --- |
| Image ABI hardening | In progress | Public errors, ABI query and default limits are documented and tested; per-call limit APIs remain planned | `src/image/capy_image.h`, `docs/10-contracts/image-abi.md` |
| Test harness split | Ready | Tests are organized by codec and contract area in a single contract binary | `tests/image/` |
| Golden fixtures | In progress | BMP, PNG and JPEG baseline fixtures validate dimensions and pixel hashes; broader variant fixtures remain pending | `tests/fixtures/` |
| Negative fixtures | In progress | Initial invalid magic, truncated data and unsupported-mode fixtures fail closed for BMP/PNG/JPEG | `tests/fixtures/` |
| Allocator failure matrix | In progress | BMP, PNG and JPEG golden fixtures force allocator failure at each allocation index | `tests/image/test_alloc_failures.c` |
| PNG inflater failures | Ready | Callback errors and short output fail closed and release temporaries | `tests/image/test_inflater_failures.c` |
| Resource-limit tests | In progress | Initial BMP/PNG/JPEG dimension rejection is covered; overflow and temporary-budget cases remain pending | `tests/image/test_limits.c` |
| BMP robust decode | Planned | Common BMP variants decode or reject deterministically | `src/image/bmp_decode.c` |
| PNG robust decode | Planned | PNG chunks, CRC, filters and inflater failures are covered | `src/image/png_decode.c` |
| JPEG baseline decode | In progress | Baseline grayscale and RGB golden fixtures decode; broader sampling/table cases remain pending; unsupported modes reject clearly | `src/image/jpeg_decode.c` |
| CapyOS image adapter | Planned | CapyOS consumes `capy-codec-image` via adapter, not source paths | CapyOS integration |
| Image format dispatch | Planned | Detection and generic decode APIs exist additively | `src/image/` |
| Modern image formats | Planned | QOI, ICO/CUR, GIF/APNG and optional WebP/AVIF are staged | `src/image/` |
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
