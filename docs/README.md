# CapyCodecs documentation

CapyCodecs owns portable media codec cores for CapyOS and related projects.

## Current migration

Migrated from CapyOS:

- `CapyOS/include/gui/bmp_loader.h`
- `CapyOS/src/gui/core/bmp_loader.c`
- `CapyOS/include/gui/png_loader.h`
- `CapyOS/src/gui/core/png_loader.c`
- `CapyOS/include/gui/jpeg_loader.h`
- `CapyOS/src/gui/core/jpeg_loader.c`

Created here:

- `src/image/capy_image.h`
- `src/image/image.c`
- `src/image/bmp_decode.c`
- `src/image/png_decode.c`
- `src/image/jpeg_decode.c`
- `tests/image/test_image_contracts.c`
- `docs/validation.md`
- `docs/compatibility.md`

## Boundary

CapyCodecs owns:

- byte-to-pixel/sample decode logic;
- deterministic error returns;
- resource limits;
- allocator-injected memory ownership;
- golden/fuzz fixtures in future slices.

CapyOS owns:

- file and network IO;
- kernel allocation wrappers;
- rendering and audio backends;
- sandbox policy;
- UI error reporting.

## Tag-release compatibility model

Early alpha releases use GitHub release tags plus a compatibility index without certificate/signature enforcement. The minimum metadata required for a release is:

- component id;
- tag;
- artifact name;
- sha256;
- required CapyOS ABI versions;
- dependencies;
- permissions.

Signature enforcement is intentionally deferred until the release pipeline and trust-store design are active.

## Next slices

1. Add repository-level host test harness.
2. Add golden BMP/PNG/JPEG fixtures.
3. Add truncated/corrupt image fixtures.
4. Add PNG inflater adapters in each host.
5. Add CapyOS codec adapter when Etapas 6-7 permit integration.
