# Container codec implementation guide

The future `capy-codec-container` ABI should parse media containers and expose packet streams to audio and video decoders.

## Container responsibility

Containers should handle:

- file/container structure;
- stream discovery;
- codec identifiers;
- packet boundaries;
- timestamps;
- durations;
- keyframe flags;
- metadata needed for decoder setup.

Containers should not perform final audio/video decode.

## ABI goals

The container ABI should expose:

- container detection;
- stream enumeration;
- stream metadata;
- packet iteration;
- seek map where supported;
- deterministic parse errors;
- allocator injection;
- bounded metadata memory.

## RIFF sequence

RIFF should be first because WAV is needed for audio and has simple structure.

Implementation sequence:

1. Parse RIFF header.
2. Validate form type.
3. Iterate chunks with checked lengths.
4. Support WAVE metadata for `capy-codec-audio`.
5. Add AVI only if a concrete CapyOS need appears.

## Ogg sequence

Ogg is needed for Vorbis and Opus.

Implementation sequence:

1. Parse Ogg pages.
2. Validate capture pattern and checksums.
3. Track logical streams.
4. Expose packets with granule positions.
5. Feed Vorbis/Opus decoders through their audio ABI.

## Matroska/WebM sequence

WebM is needed for VP8, VP9, AV1, Opus and Vorbis.

Implementation sequence:

1. Parse EBML header.
2. Parse Segment info.
3. Parse Tracks.
4. Iterate Clusters.
5. Expose Blocks/SimpleBlocks as packets.
6. Enforce metadata and nesting limits.

## ISO BMFF/MP4 sequence

MP4 is needed for AAC, H.264, HEVC and AV1-in-MP4 scenarios.

Implementation sequence:

1. Parse boxes with checked size handling.
2. Validate `ftyp`.
3. Parse `moov` metadata.
4. Parse track sample tables.
5. Expose samples as packets.
6. Support fragmented MP4 only after baseline MP4 is stable.

## Security requirements

Container parsers must enforce:

- maximum nesting depth;
- maximum box/chunk count;
- maximum metadata bytes;
- checked offsets and sizes;
- deterministic handling of unknown chunks/boxes;
- no unbounded seeks or full-file buffering.
