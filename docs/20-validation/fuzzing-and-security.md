# Fuzzing and security

Codecs process attacker-controlled bytes and must be treated as high-risk parsing components.

## Security invariants

- Reject invalid input deterministically.
- Avoid undefined behavior.
- Check all integer arithmetic that affects memory sizes or offsets.
- Enforce documented resource limits.
- Avoid hidden allocations and global mutable decoder state.
- Reset output objects on failure.
- Preserve portable fallback paths for every optimized path.

## Fuzz targets

Initial fuzz targets should cover:

- BMP decode memory;
- PNG decode memory with a controlled inflater adapter;
- JPEG decode memory;
- format detection;
- generic image decode dispatch;
- future GIF/APNG frame decode;
- future RIFF/Ogg/WebM/MP4 parsers.

## Corpus policy

The corpus should contain:

- minimal valid files;
- real-world small files;
- truncated files;
- mutated headers;
- maximum dimension declarations;
- palette edge cases;
- malformed chunk/marker order;
- allocator-budget stress cases.

Every security bug should add a regression fixture.

## Sanitizers

Validation should add jobs for:

- AddressSanitizer;
- UndefinedBehaviorSanitizer;
- MemorySanitizer where supported;
- integer-overflow instrumentation where available.

## Differential testing

Differential tests may compare decoded output against external reference tools during host-side testing only.

Useful references:

- libpng;
- libjpeg-turbo;
- stb_image;
- ImageMagick command-line tools.

Reference libraries must not become production dependencies of the portable codec core unless explicitly accepted as an optional component.

## Hardening gates

Release gates should eventually require:

- clean `make validate`;
- clean sanitizer suite;
- fuzz smoke run;
- no forbidden dependencies;
- documented ABI/resource-limit changes;
- updated compatibility index metadata.
