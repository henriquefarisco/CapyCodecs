# CapyCodecs compatibility and integration contract

CapyCodecs modules must remain portable codec cores with no CapyOS kernel dependency.

## CapyOS reference version

- CapyOS core pinned for this contract: `0.8.0-alpha.241+20260519`
- Authoritative cross-repo matrix: `CapyOS/docs/reference/integration/compatibility-matrix.md`
- Canonical manifest format consumed by the in-tree adapter: `CapyOS/docs/reference/integration/capypkg-publisher-manifest-format.md`
- Manual deploy runbook: `CapyOS/docs/operations/manual-module-deploy-runbook.md`

## Authoritative CapyOS references

- `CapyOS/docs/reference/integration/modular-installation-architecture.md`
- `CapyOS/docs/reference/integration/media-codec-integration-contract.md`
- `CapyOS/docs/reference/integration/compatibility-matrix.md`
- `CapyOS/docs/reference/integration/capypkg-publisher-manifest-format.md`

## Owned ABI

CapyCodecs currently owns the `capy-codec-image` ABI for image decoding.

This ABI covers:

- decoder entry points;
- `capy_image_rgba32` output ownership;
- allocator injection;
- PNG inflater injection;
- ARGB32 pixel contract;
- deterministic error returns.

## Compatibility rules

- **Additive public API:** Public structs and function signatures must evolve additively.
- **Fail closed:** Existing decoder functions must fail closed on invalid input.
- **Reset on failure:** All output images must be zeroed/reset on failure when `out` is valid.
- **Injected allocation:** Decoders must not allocate through `malloc`, `kalloc` or hidden globals.
- **Injected inflater:** PNG must not depend directly on CapyOS `tinf`; hosts provide inflater callbacks.
- **Explicit limits:** Resource limits must be explicit and documented before increasing them.

## Install/update boundary

CapyCodecs artifacts may be optional components. CapyOS owns:

- file/network IO;
- allocator adapters;
- renderer/audio backend adapters;
- sandbox policy;
- staging and activation;
- rollback metadata.

## Dependency rules

Components that need image decode should depend on `capy-codec-image`, not on internal source paths.

A browser component may depend on a codec component, but codec components must not depend on browser, UI or CapyOS runtime internals.

## Validation before CapyOS integration

Before CapyOS consumes a CapyCodecs release, externally validate:

- invalid/truncated image rejection;
- allocator failure paths;
- PNG inflater failure paths;
- golden BMP/PNG/JPEG fixtures;
- maximum dimension/resource limit behavior;
- no direct CapyOS kernel includes.

CapyOS image integration is gated by Etapas 6-7. Audio/video integration is gated by Etapa 10.

## Publishing as a Capy package (future, when the relevant stage opens)

When CapyCodecs is delivered as a remote module to the CapyOS
`services/capypkg` adapter, the publisher must follow
`CapyOS/docs/reference/integration/capypkg-publisher-manifest-format.md`.
The key requirements that affect CapyCodecs are:

- `payload_url` must be HTTPS only;
- `payload_sha256` must be lowercase 64 hex of the published artifact;
- `payload_size` ≤ 1 MiB during the alpha streaming-buffer window;
- `name` must follow `[a-zA-Z0-9._-]` (suggested `org.capyos.codecs.image-basic` for the current ABI slice);
- `install_root` must live under `/var/capypkg` or `/opt/`;
- `signature_ed25519` must cover the canonical descriptor
  `name=N|version=V|payload_sha256=H|payload_url=U\n`;
- `depends` should remain empty until the codec splits into
  sub-packages.

The `CAPY_IMAGE_ABI_VERSION` declared in `src/image/capy_image.h`
must be reflected in the `required_abis[].minimum_version` field of
the high-level JSON index (`CapyAgent` side) but is not consumed by
the in-tree alpha adapter today.
