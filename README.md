# CapyCodecs

Version: 0.1.0

CapyCodecs owns portable image and audio codec contracts for CapyOS services.

The `capy-codec-audio` v1 surface decodes bounded WAV/PCM input to owned,
interleaved PCM through an injected allocator. It performs no file, network or
audio-device access.

## Validation

```sh
make validate
```

The release gate compiles with strict C warnings, runs image and audio contract
tests, checks release metadata and verifies hardened compile flags.
