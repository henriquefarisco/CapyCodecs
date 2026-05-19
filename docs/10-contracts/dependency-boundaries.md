# Dependency boundaries

CapyCodecs must remain a portable media-codec layer rather than a CapyOS runtime subsystem.

## Allowed dependencies

- Standard C headers needed by portable codec logic.
- Local public headers inside CapyCodecs.
- Host-provided callbacks for allocation, inflation, IO-like data access or platform services.
- Optional test-only reference tools used outside the production codec core.

## Forbidden production dependencies

- CapyOS kernel headers.
- CapyOS runtime headers.
- VFS or file IO calls.
- Network IO calls.
- Renderer or GUI internals.
- Audio backend internals.
- Hidden global allocators.
- Direct `malloc` or `free` in decoder logic.
- Direct `kalloc`, `kfree` or CapyOS allocator calls.
- Direct PNG dependency on CapyOS `tinf`.

## Dependency direction

Consumers may depend on codec ABIs:

```text
CapyOS component -> capy-codec-image
CapyOS component -> capy-codec-audio
CapyOS component -> capy-codec-video
```

Codec components must not depend on consumers:

```text
capy-codec-image -x-> browser
capy-codec-image -x-> renderer
capy-codec-image -x-> kernel
```

## Optional components

Large or license-sensitive codecs should be optional components:

- WebP;
- AVIF;
- Opus;
- AAC;
- VP9;
- AV1;
- H.264;
- H.265.

Optional components still follow the same allocator, error, sandbox and release-contract rules.
