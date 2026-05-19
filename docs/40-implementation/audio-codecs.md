# Audio codec implementation guide

The future `capy-codec-audio` ABI should decode encoded audio into explicit PCM buffers or streaming PCM frames.

## ABI goals

The audio ABI should expose:

- decoder entry points;
- sample rate;
- channel count;
- channel layout where needed;
- sample format;
- decoded sample buffer ownership;
- streaming decode state;
- deterministic error returns;
- allocator injection.

## PCM output model

The first output model should support:

- signed 16-bit PCM;
- signed 24-bit or packed 32-bit PCM;
- signed 32-bit PCM;
- float 32-bit PCM;
- interleaved samples initially;
- planar samples later if needed.

## WAV/PCM first slice

WAV/PCM should be implemented first because it validates audio metadata and RIFF parsing without requiring complex compression.

Implementation sequence:

1. Parse RIFF header.
2. Validate WAVE form type.
3. Parse `fmt ` chunk.
4. Validate PCM or float format tag.
5. Parse `data` chunk.
6. Validate sample rate, channel count and bits per sample.
7. Enforce duration and output-size limits.
8. Return decoded PCM view or owned PCM copy based on ABI design.

Reject safely:

- unsupported compression tags;
- impossible block align;
- inconsistent byte rate;
- truncated data chunk;
- excessive duration;
- output-size overflow.

## Compressed audio sequence

After WAV/PCM:

1. FLAC for lossless local media.
2. Opus for modern communication and web media.
3. Vorbis for Ogg/WebM compatibility.
4. MP3/AAC only after licensing and dependency strategy are explicit.

## Streaming requirement

Compressed audio should not require full-file decode into memory.

The ABI should support:

- decoder state object;
- feed/input packets;
- pull decoded frames;
- flush/end-of-stream;
- reset;
- deterministic cleanup.

## CapyOS integration boundary

CapyCodecs decodes samples. CapyOS owns:

- audio device output;
- mixing;
- volume;
- scheduling;
- latency policy;
- sandboxing;
- file/network IO.
