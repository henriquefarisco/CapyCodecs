# Video codec implementation guide

The future `capy-codec-video` ABI should provide incremental decoded frames without owning playback, scheduling or rendering.

## ABI goals

The video ABI should expose:

- decoder state creation/destruction;
- packet input;
- decoded frame output;
- timestamps;
- frame dimensions;
- pixel format;
- color range;
- color primaries where available;
- planes and strides;
- allocator injection;
- deterministic error returns.

## Frame output model

Frames should support planar formats first:

- YUV420;
- YUV422 if needed;
- YUV444 if needed;
- RGBA as a conversion output or adapter path.

Each frame should describe:

- plane pointers;
- plane strides;
- visible width and height;
- coded width and height;
- timestamp;
- duration if known;
- ownership and release callback.

## Implementation sequence

1. Define decoder state lifecycle.
2. Define packet input API.
3. Define frame output API.
4. Add IVF parsing for simple VPx/AV1 test streams or keep it under container ABI.
5. Add VP8 as first video codec candidate.
6. Add VP9 after VP8 architecture is stable.
7. Add AV1 as optional advanced component.
8. Consider H.264/H.265 only with explicit licensing and dependency policy.

## Incremental decode requirement

Video decode must be streaming and incremental.

Do not:

- load full videos into memory;
- require complete container parse before first frame;
- perform rendering inside codec code;
- perform audio sync inside codec code.

## CapyOS integration boundary

CapyCodecs outputs frames and timing metadata. CapyOS owns:

- rendering;
- compositor integration;
- frame scheduling;
- audio/video sync;
- hardware acceleration policy;
- sandboxing;
- file/network IO.

## Security requirements

Video codecs are high-risk and must require:

- fuzzing before integration;
- sanitizer coverage;
- memory-budget enforcement;
- frame-size limits;
- reference/differential validation where practical;
- strict optional-component isolation for very complex codecs.
