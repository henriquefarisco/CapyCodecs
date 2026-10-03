# CapyCodecs

Version: 0.1.1

CapyCodecs owns portable image and audio codec contracts for CapyOS services.

The `capy-codec-audio` v1 surface decodes bounded WAV/PCM and Ogg/Vorbis input to owned,
interleaved PCM through an injected allocator. It performs no file, network or
audio-device access.

Vorbis returns S16LE PCM; `make vorbis-decode-test` compares independent
fixtures against FFmpeg and exercises allocation failure and resource limits.
See `docs/compatibility.md` for supported stream boundaries and budgets.

## Validation

```sh
make validate
```

The release gate compiles with strict C warnings, runs image and audio contract
tests, checks release metadata and verifies hardened compile flags.
