# CapyCodecs compatibility and integration contract

CapyCodecs owns the **portable image / audio / video codec cores**
consumed by CapyOS via a future `gui/codecs/` adapter (image) and
audio adapter (audio/video, Etapa 10). CapyCodecs modules must
remain portable codec cores with no CapyOS kernel dependency.

> **Note (2026-05-20):** This file is the authoritative compatibility
> contract for CapyCodecs. A historical mirror exists at
> `docs/10-contracts/compatibility.md` which may lag this file; when
> they disagree, this file wins. The legacy hierarchical docs under
> `docs/10-contracts/`, `docs/20-validation/`, `docs/30-roadmap/` and
> `docs/40-implementation/` continue to be the authoritative reference
> for technical details (image ABI, validation strategy, roadmap).

## CapyOS reference version

- CapyOS core pinned for this contract: `0.8.0-alpha.262+20260602`
- Authoritative cross-repo matrix: [`CapyOS/docs/reference/integration/compatibility-matrix.md`](../../CapyOS/docs/reference/integration/compatibility-matrix.md)
- Canonical manifest format consumed by the in-tree adapter: [`CapyOS/docs/reference/integration/capypkg-publisher-manifest-format.md`](../../CapyOS/docs/reference/integration/capypkg-publisher-manifest-format.md)
- Manual deploy runbook: [`CapyOS/docs/operations/manual-module-deploy-runbook.md`](../../CapyOS/docs/operations/manual-module-deploy-runbook.md)
- Current cross-repo audit: [`CapyOS/docs/reference/integration/compatibility-audit-2026-06-02.md`](../../CapyOS/docs/reference/integration/compatibility-audit-2026-06-02.md)

## Authoritative CapyOS references

- `CapyOS/docs/reference/integration/modular-installation-architecture.md`
- `CapyOS/docs/reference/integration/media-codec-integration-contract.md`
- `CapyOS/docs/reference/integration/external-core-repositories.md`

## Local detailed contracts (hierarchical)

- `docs/00-overview/project-boundary.md` — CapyCodecs vs CapyOS responsibilities.
- `docs/00-overview/migration-status.md` — extraction history.
- `docs/10-contracts/image-abi.md` — `capy-codec-image` ABI surface.
- `docs/10-contracts/dependency-boundaries.md` — allowed/forbidden dependencies.
- `docs/20-validation/validation.md` — host-side test coverage.
- `docs/20-validation/test-strategy.md` — fuzz harnesses, sanitizers, differential.
- `docs/30-roadmap/` — image ABI hardening roadmap; future audio/video.
- `docs/40-implementation/` — implementation notes.

## Owned ABI

CapyCodecs currently owns the `capy-codec-image` ABI (v2).

This ABI covers:

- decoder entry points (`capy_image_*`), including the default-limit
  `capy_{bmp,png,jpeg,qoi}_decode_memory` and the per-call
  `capy_{bmp,png,jpeg,qoi}_decode_memory_limited` variants that accept a
  caller-provided `capy_image_limits`;
- format detection (`capy_image_detect_memory` + `enum capy_image_format`,
  covering BMP/PNG/JPEG/QOI) and generic decode dispatch
  (`capy_image_decode_memory`);
- header-only metadata query (`capy_image_query_memory` +
  `struct capy_image_metadata`);
- `capy_image_rgba32` output ownership;
- allocator injection (no `malloc`/`kalloc`/global heap);
- PNG inflater injection (no direct CapyOS `tinf` dependency);
- ARGB32 pixel contract;
- deterministic error returns;
- human-readable error strings (`capy_image_strerror`, advertised by
  `CAPY_IMAGE_FEATURE_STRERROR`);
- short format names (`capy_image_format_name`, advertised by
  `CAPY_IMAGE_FEATURE_FORMAT_NAME`).

`CAPY_IMAGE_ABI_VERSION` moved from `1` to `2` additively. The v1
entry points keep their signatures (now thin wrappers that pass the
default limits) and new feature bits advertise the additions:
`CAPY_IMAGE_FEATURE_PER_CALL_LIMITS` for the limited variants,
`CAPY_IMAGE_FEATURE_DETECT` for magic-byte detection,
`CAPY_IMAGE_FEATURE_GENERIC_DECODE` for the generic dispatcher,
`CAPY_IMAGE_FEATURE_METADATA` for the header-only metadata query and
`CAPY_IMAGE_FEATURE_QOI_DECODE` for the QOI codec and
`CAPY_IMAGE_FEATURE_STRERROR` for the `capy_image_strerror` error-string
helper and `CAPY_IMAGE_FEATURE_FORMAT_NAME` for the `capy_image_format_name`
helper. A NULL `limits` argument is treated as "use `capy_image_default_limits`".

Audio and video ABIs will be added in `capy-codec-audio` / `capy-codec-video`
when Etapa 10 opens.

CapyCodecs does **not** own:

- file/network I/O (CapyOS adapters provide it);
- allocator implementation (host injects via callback);
- renderer / audio backend (CapyOS owns);
- sandbox policy (CapyOS owns);
- staging / activation / rollback (CapyOS owns);
- direct CapyOS kernel headers, VFS, hidden globals, direct `malloc`,
  direct `kalloc`, direct `tinf` dependency.

## Compatibility rules

- **Additive public API:** public structs and function signatures
  must evolve additively. ABI version `CAPY_IMAGE_ABI_VERSION`
  (declared in `src/image/capy_image.h`) must bump only on additive
  changes until the next integration window.
- **Fail closed:** existing decoder functions must fail closed on
  invalid input (deterministic error code, no partial write).
- **Reset on failure:** all output images must be zeroed/reset on
  failure when `out` is valid.
- **Injected allocation:** decoders must not allocate through
  `malloc`, `kalloc` or hidden globals; the host provides allocator
  callbacks.
- **Injected inflater:** PNG must not depend directly on CapyOS
  `tinf`; hosts provide inflater callbacks.
- **Explicit limits:** resource limits must be explicit and
  documented before increasing them.
- **Pixel contract:** current output is ARGB32; changes to channel
  order, alpha semantics or premultiplication are ABI-impacting and
  require a major bump.

## Error model

| Code | Trigger | Caller expectation |
|---|---|---|
| `CAPY_IMAGE_OK` | success | `out` valid; caller owns the buffer per allocator policy |
| `CAPY_IMAGE_ERR_INVALID_ARGUMENT` | NULL input, zero dimension, invalid stride | abort; do not retry |
| `CAPY_IMAGE_ERR_UNSUPPORTED_FORMAT` | magic bytes don't match a known codec | try another codec or surface to user |
| `CAPY_IMAGE_ERR_CORRUPT_DATA` | data malformed but recognised header | surface error; do not retry |
| `CAPY_IMAGE_ERR_TRUNCATED_DATA` | input shorter than declared | request more bytes if streaming; otherwise surface error |
| `CAPY_IMAGE_ERR_OUT_OF_MEMORY` | host allocator returned NULL | free other buffers and retry, or surface OOM |
| `CAPY_IMAGE_ERR_RESOURCE_LIMIT` | dimension/pixel/sample budget exceeded | surface error; do not silently downscale |
| `CAPY_IMAGE_ERR_INFLATER_FAILED` | host inflater callback returned negative | surface inflater failure |

All errors must be deterministic. CapyCodecs never returns
indeterminate state, never leaves `out` half-written, never panics
the host on bad input.

## Resource and performance limits

| Limit | Default value | Owner / configuration |
|---|---|---|
| Maximum image dimension (width × height) | `4096 × 4096` default; configurable per call (alpha cap: 16384 × 16384) | caller via `capy_image_limits` |
| Maximum decoded output bytes | `max_width × max_height × 4` (≈ 64 MiB at default dims) | caller via `capy_image_limits.max_output_bytes` |
| Maximum temporary bytes | `(max_width × 4 + 1) × max_height` (worst-case PNG scanline pass) | caller via `capy_image_limits.max_temporary_bytes` |
| Maximum memory budget per decode | configurable; bounded by injected allocator | caller |
| Maximum decode time per call | not enforced inside codec; caller responsible for budget via cooperative cancellation | caller |
| Allowed pixel format output | ARGB32 (8/8/8/8) | CapyCodecs ABI |
| Audio sample format (future) | TBD when Etapa 10 opens | CapyCodecs / CapyOS |

All four `capy_image_limits` fields are enforced before any large
allocation. The default-limit `capy_*_decode_memory` wrappers apply
`capy_image_default_limits`; callers that need tighter or looser
budgets use the `capy_*_decode_memory_limited` entry points. PNG no
longer carries a separate internal `1024 × 1024` cap — it honours the
caller-provided dimension and temporary-byte budgets like BMP and
JPEG. Limits are never used to silently downscale; overruns reject
with `CAPY_IMAGE_ERR_RESOURCE_LIMIT`.

## Install/update boundary

CapyCodecs artifacts may be optional Capy packages when the
relevant CapyOS stage opens. CapyOS owns:

- file/network I/O for fetching the package;
- allocator adapters injected into the codec;
- renderer / audio backend adapters consuming the codec output;
- sandbox policy applied to the codec call;
- staging and activation;
- rollback metadata.

## Dependency rules

Components that need image decode should depend on
`capy-codec-image`, not on internal source paths.

A browser component may depend on a codec component, but codec
components must not depend on browser, UI or CapyOS runtime
internals.

Forbidden dependencies:

- CapyOS kernel headers;
- CapyOS VFS;
- network I/O;
- renderer / audio internals;
- hidden allocators;
- direct `malloc` / `free` / `kalloc` / `kfree`;
- direct `tinf` (the inflater must be injected).

## Validation before CapyOS integration

Before CapyOS consumes a CapyCodecs release, externally validate:

- invalid/truncated image rejection (golden fixtures);
- allocator failure paths (failpoint injection);
- PNG inflater failure paths (failpoint injection);
- golden BMP/PNG/JPEG fixtures (`tests/`);
- maximum dimension/resource limit behaviour;
- no direct CapyOS kernel includes (`make validate` enforces);
- `make package` produces canonical `<name>.bin` + `<name>.manifest`
  in line-oriented `key=value` format.

CapyOS image integration is gated by Etapas 6-7. Audio/video
integration is gated by Etapa 10.

## Publishing as a Capy package (future, when Etapas 6-7 open)

When CapyCodecs is delivered as a remote module to the CapyOS
`services/capypkg` adapter, the publisher must follow
[`CapyOS/docs/reference/integration/capypkg-publisher-manifest-format.md`](../../CapyOS/docs/reference/integration/capypkg-publisher-manifest-format.md).
The key requirements that affect CapyCodecs are:

- `payload_url` must be HTTPS only;
- `payload_sha256` must be lowercase 64 hex of the published artifact;
- `payload_size` ≤ 1 MiB during the alpha streaming-buffer window;
- `name` must follow `[a-zA-Z0-9._-]`; suggested canonical name
  `org.capyos.codecs.image-basic` for the current ABI slice;
- `install_root` must live under `/var/capypkg` or `/opt/`;
- `signature_ed25519` must cover the canonical descriptor
  `name=N|version=V|payload_sha256=H|payload_url=U\n`;
- `depends` should remain empty until the codec splits into
  sub-packages.

The `CAPY_IMAGE_ABI_VERSION` declared in `src/image/capy_image.h`
must be reflected in the `required_abis[].minimum_version` field of
the high-level JSON index (`CapyAgent` side) but is not consumed by
the in-tree alpha adapter today.

Until CapyAgent publishes its Ed25519 signer, CapyCodecs cannot be
installed from a `signed` repository in production; `--unsigned`
labs are possible but never promote to user-facing release.
