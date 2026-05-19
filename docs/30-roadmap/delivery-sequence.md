# Delivery sequence

This sequence keeps codec development linear, testable and compatible with the CapyOS component model.

## Short-term sequence

1. Stabilize the current `capy-codec-image` ABI.
2. Add official public error codes.
3. Add explicit resource-limit configuration.
4. Split tests by codec and contract area.
5. Add golden BMP/PNG/JPEG fixtures.
6. Add corrupt and truncated fixtures.
7. Complete BMP coverage.
8. Complete PNG parser, filters and inflater integration.
9. Complete JPEG baseline decode.
10. Update validation documentation and release gates.

## Medium-term sequence

11. Build the CapyOS image adapter outside the codec core.
12. Add image format detection.
13. Add generic image decode dispatch.
14. Add QOI.
15. Add ICO/CUR.
16. Add GIF static-frame support.
17. Add GIF animation support.
18. Add APNG support.
19. Add image metadata and orientation support.
20. Create `capy-codec-audio`.
21. Implement WAV/PCM.
22. Plan FLAC, Opus and Vorbis components.

## Long-term sequence

23. Create `capy-codec-container`.
24. Implement RIFF, Ogg, WebM and MP4 progressively.
25. Create `capy-codec-video`.
26. Implement incremental video decode pipeline.
27. Add VP8 and VP9.
28. Add AV1 as an optional advanced component.
29. Add continuous fuzzing.
30. Make sanitizers mandatory in CI.
31. Add differential testing.
32. Add benchmarks.
33. Add optional SIMD paths.
34. Formalize release index metadata.
35. Prepare signed component manifests.

## Dependency order

- Image ABI hardening comes before image adapter integration.
- Fixtures and allocator-failure tests come before expanding image formats.
- Container parsing comes before full video playback support.
- Video decode remains separate from CapyOS scheduling, rendering and audio sync.
- Release metadata comes before automatic CapyOS component updates.
