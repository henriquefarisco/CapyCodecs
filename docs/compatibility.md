# CapyCodecs compatibility and integration contract

CapyCodecs modules must remain portable codec cores with no CapyOS kernel dependency.

Authoritative CapyOS references:

- `CapyOS/docs/reference/integration/modular-installation-architecture.md`
- `CapyOS/docs/reference/integration/media-codec-integration-contract.md`

## Owned ABI

CapyCodecs owns the `capy-codec-image` ABI for image decoding.

This ABI covers:

- decoder entry points;
- `capy_image_rgba32` output ownership;
- allocator injection;
- PNG inflater injection;
- ARGB32 pixel contract;
- deterministic error returns.

## Compatibility rules

- Public structs and function signatures must evolve additively.
- Existing decoder functions must keep fail-closed behavior on invalid input.
- All output images must be zeroed/reset on failure when `out` is valid.
- Decoders must not allocate through `malloc`, `kalloc` or hidden globals.
- PNG must not depend directly on CapyOS `tinf`; hosts provide inflater callbacks.
- Resource limits must be explicit and documented before increasing them.

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
