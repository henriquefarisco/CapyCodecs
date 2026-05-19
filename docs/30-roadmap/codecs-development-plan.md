# CapyCodecs development plan

This plan defines a bold but linear path to evolve CapyCodecs from the current image slice into a robust, portable media-codec platform for CapyOS.

## Strategic direction

CapyCodecs should remain a portable library of codec cores. It should not become a CapyOS IO, rendering, audio, sandbox or package-management subsystem.

The long-term platform should be split into stable ABIs:

- **`capy-codec-image`:** still images, animated images, metadata and color management.
- **`capy-codec-audio`:** decoded PCM samples and streaming audio decode.
- **`capy-codec-container`:** packet extraction, timestamps and stream metadata.
- **`capy-codec-video`:** incremental decoded video frames and pixel planes.

## Non-negotiable invariants

- Public ABI evolution is additive.
- Decode failures are deterministic.
- Output objects are reset on failure when valid.
- All memory ownership is host-injected.
- Compression, IO and platform services are callback-based or adapter-based.
- Resource limits are explicit and documented.
- Codecs remain independent from CapyOS kernel/runtime internals.
- Security bugs create permanent regression fixtures.

## Short term: robust image foundation

### 1. Harden `capy-codec-image`

Goal: make the current image ABI stable enough for serious CapyOS consumption.

Deliverables:

- public ABI version query;
- public codec feature query;
- official error-code enum compatible with existing `int` returns;
- explicit resource-limit struct;
- documented pixel channel and alpha policy;
- allocator ownership tests;
- failure reset tests for all entry points.

Implementation notes:

- Add new functions instead of changing existing signatures.
- Keep existing BMP/PNG/JPEG entry points valid.
- Treat pixel-order clarification as compatibility-critical documentation.
- Keep default limits conservative until benchmarks justify increases.

Acceptance criteria:

- Existing callers keep compiling.
- All failure paths reset valid outputs.
- No decoder allocates through hidden platform APIs.
- Documentation and tests describe every new public contract.

### 2. Build a real validation harness

Goal: turn compatibility rules into executable tests.

Deliverables:

- codec-specific test files;
- golden fixtures for BMP, PNG and JPEG;
- corrupt/truncated fixtures;
- allocator-failure matrix;
- resource-limit tests;
- PNG inflater-failure tests.

Implementation notes:

- Keep tiny byte-array fixtures for minimal cases.
- Use fixture files only when byte arrays become unreadable.
- Hash decoded pixels to avoid enormous expected-output tables.
- Test every successful fixture with forced allocation failures.

Acceptance criteria:

- `make validate` remains the primary gate.
- Contract failures are caught by tests, not manual review.
- Fixtures are small, explicit and easy to audit.

### 3. Complete BMP as the reference simple codec

Goal: use BMP to establish internal parsing, stride and bounds-check patterns.

Deliverables:

- Windows V3 BMP support;
- 24-bit BGR;
- 32-bit BGRA/BGRX;
- top-down and bottom-up images;
- row padding;
- paletted 1/4/8-bit images;
- 16-bit RGB565/RGB555 when safe;
- BITFIELDS support or deterministic rejection.

Implementation notes:

- Centralize little-endian reads.
- Validate file offsets before reading.
- Validate row stride with overflow checks.
- Reject unsupported compression early.
- Treat negative heights as top-down only where valid.

Acceptance criteria:

- Common BMP files decode.
- Unsupported BMP variants reject with clear errors.
- Overflow, offset and palette errors are tested.

### 4. Complete PNG with injected inflater

Goal: implement a correct PNG decoder without direct CapyOS `tinf` dependency.

Deliverables:

- signature validation;
- IHDR validation;
- chunk-order validation;
- CRC validation;
- IDAT concatenation with memory limits;
- filters None, Sub, Up, Average and Paeth;
- RGB, RGBA, grayscale, grayscale-alpha and indexed color;
- PLTE and tRNS handling;
- deterministic rejection for unsupported interlace until Adam7 is implemented;
- host-side inflater adapter tests.

Implementation notes:

- Keep inflater callback mandatory for compressed decode.
- Do not bind production core to zlib, miniz or CapyOS `tinf` directly.
- Calculate decompressed scanline size before allocating.
- Check filter byte per row.
- Keep temporary buffers bounded.

Acceptance criteria:

- Real compressed PNG fixtures decode through a host adapter.
- Corrupt chunks, invalid CRCs and inflater failures reject safely.
- PNG never returns partial output on error.

### 5. Complete JPEG baseline

Goal: decode common baseline JPEG while rejecting unsupported modes safely.

Deliverables:

- SOI/EOI parsing;
- APPn skipping;
- DQT, DHT, SOF0 and SOS parsing;
- baseline Huffman decode;
- dequantization;
- IDCT;
- YCbCr to RGBA32 conversion;
- grayscale support;
- deterministic rejection for progressive, arithmetic or unsupported sampling modes.

Implementation notes:

- Bound MCU dimensions and table counts.
- Validate marker lengths before reads.
- Avoid recursion and large stack buffers.
- Keep color conversion portable first; optimize later.

Acceptance criteria:

- Baseline JPEG fixtures decode.
- Progressive JPEG rejects clearly.
- Truncation inside each major segment is tested.

## Medium term: integration and format expansion

### 6. Add CapyOS image adapter

Goal: let CapyOS consume CapyCodecs without violating the boundary.

Deliverables:

- CapyOS-side allocator adapter;
- CapyOS-side inflater adapter;
- component metadata;
- compatibility index entry;
- integration tests for ABI mismatch and rollback.

Implementation notes:

- Keep adapter outside CapyCodecs core if it requires CapyOS headers.
- CapyCodecs remains byte-buffer based.
- CapyOS owns file IO and sandbox policy.

Acceptance criteria:

- CapyOS depends on `capy-codec-image` ABI.
- No CapyOS kernel include appears in codec sources.

### 7. Add detection and generic dispatch

Goal: expose a safe format-sniffing layer.

Deliverables:

- `capy_image_detect_memory`;
- `capy_image_decode_memory`;
- feature flags for compiled codecs;
- format enum;
- unsupported-format error handling.

Implementation notes:

- Detect by magic bytes and structural validation, not filename.
- Keep codec-specific entry points.
- Use the same allocator and limits model as direct decoders.

Acceptance criteria:

- Hosts can decode unknown image buffers through one API.
- Unsupported formats fail without probing deep unsafe paths.

### 8. Add modern image formats

Goal: support common UI, browser and app image use cases.

Delivery order:

1. QOI for a small, fast, architecture-validating codec.
2. ICO/CUR for icons and pointers.
3. GIF static frame.
4. GIF animation.
5. APNG.
6. WebP as optional component.
7. AVIF as optional advanced component.

Implementation notes:

- Add animation APIs before exposing GIF/APNG animation.
- Limit frame count, canvas size and total decoded bytes.
- Keep complex formats optional when dependencies or licensing are non-trivial.

Acceptance criteria:

- Static image decode remains simple.
- Animated image decode uses explicit frame APIs and limits.
- Optional formats do not bloat the mandatory core.

### 9. Add metadata and color management

Goal: make decoded images predictable for the renderer.

Deliverables:

- metadata query API;
- orientation metadata;
- sRGB/gamma/ICC detection;
- documented alpha premultiplication policy;
- optional orientation application policy.

Implementation notes:

- Prefer exposing metadata before automatically transforming pixels.
- Do not silently rotate or color-convert without an option contract.
- Keep metadata memory bounded.

Acceptance criteria:

- CapyOS knows dimensions, alpha, orientation and color intent.
- Renderer assumptions are documented.

### 10. Create `capy-codec-audio`

Goal: start portable audio decode with simple PCM output.

Deliverables:

- audio ABI header;
- decoded PCM buffer structure;
- sample rate and channel metadata;
- sample format enum;
- streaming decode model;
- WAV/PCM decoder;
- FLAC, Opus and Vorbis plans.

Implementation notes:

- Begin with RIFF/WAV PCM because it validates container parsing and PCM output.
- Keep audio backend integration in CapyOS.
- Use streaming decode for compressed audio to avoid full-file memory pressure.

Acceptance criteria:

- CapyOS can play WAV/PCM through an adapter.
- The audio ABI does not depend on an audio device backend.

## Long term: full media platform

### 11. Create `capy-codec-container`

Goal: separate packet extraction from codec decode.

Deliverables:

- RIFF parser;
- Ogg parser;
- Matroska/WebM parser;
- ISO BMFF/MP4 parser;
- stream metadata;
- packet iteration;
- timestamp handling.

Implementation notes:

- Containers output packets and stream descriptions, not rendered media.
- Codecs consume packets through their own ABIs.
- CapyOS owns scheduling and synchronization.

Acceptance criteria:

- Audio/video codecs no longer parse every container themselves.
- Packet boundaries and timestamps are deterministic.

### 12. Create `capy-codec-video`

Goal: provide incremental video frame decode for modern media.

Deliverables:

- decoder-state API;
- frame output API;
- pixel-plane structure;
- timestamp propagation;
- YUV/RGBA format enums;
- VP8;
- VP9;
- optional AV1;
- optional H.264/H.265 with explicit licensing decision.

Implementation notes:

- Video decode must be incremental.
- Do not load complete videos into memory.
- Keep hardware acceleration as a future adapter path, not the base ABI assumption.

Acceptance criteria:

- CapyOS receives decoded frames and timing data.
- Scheduling, rendering and audio sync remain outside CapyCodecs.

### 13. Make security continuous

Goal: make fuzzing and sanitizers normal release requirements.

Deliverables:

- libFuzzer or AFL++ harnesses;
- sanitizer CI jobs;
- fuzz corpus management;
- security regression fixtures;
- dependency scanning;
- CVE response process.

Acceptance criteria:

- Every parser surface has fuzz coverage.
- Security fixes add permanent fixtures.
- Releases cannot ignore sanitizer failures.

### 14. Add measured performance work

Goal: optimize after correctness and tests are strong.

Deliverables:

- decode throughput benchmarks;
- peak memory benchmarks;
- allocation-count benchmarks;
- portable C optimizations;
- optional SIMD paths;
- fallback validation against optimized paths.

Implementation notes:

- Measure before optimizing.
- Keep C fallback authoritative.
- Feature-detect SIMD outside assumptions about the host OS.

Acceptance criteria:

- Performance changes show measured benefit.
- Optimized and fallback paths produce equivalent output.

### 15. Complete release engineering

Goal: make CapyCodecs a safely updateable CapyOS component family.

Deliverables:

- SemVer policy;
- ABI version policy;
- compatibility index;
- artifact hashes;
- required CapyOS ABI metadata;
- dependency metadata;
- permission metadata;
- rollback metadata;
- future signature hooks.

Acceptance criteria:

- CapyOS can stage, validate, activate and roll back codec components.
- ABI breakage requires a major compatibility decision.

## Definition of done for any codec

A codec is not done until it has:

- documented public entry points;
- explicit ownership rules;
- deterministic error returns;
- fail-closed invalid input behavior;
- resource-limit enforcement;
- golden fixtures;
- corrupt/truncated fixtures;
- allocator-failure tests;
- sanitizer coverage;
- fuzz coverage or a tracked fuzzing exception;
- updated compatibility documentation;
- no forbidden CapyOS/runtime dependency.
