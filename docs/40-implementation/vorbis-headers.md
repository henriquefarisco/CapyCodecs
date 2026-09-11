# Vorbis identification and comments (development)

`src/audio/vorbis_headers.{h,c}` parses the first two Vorbis packet types without
allocation, retained input pointers, callbacks, I/O or platform dependencies.
This private interface does not advertise Vorbis decoding in `capy-codec-audio`.
Its normative reference is the [Xiph Vorbis I specification, sections 4.2 and 5](https://www.xiph.org/vorbis/doc/Vorbis_I_spec.html).

Identification validates the signature/type, version, nonzero channels/rate,
ordered legal block exponents and framing bit. It enforces both caller limits
and existing CapyCodecs channel/rate ceilings before exposing metadata. Bitrate
hints do not control allocation. No duration or decoded size is inferred from
these headers.

Comments validate packet type/signature, vendor and field lengths, bounded field
count and framing. All offsets use remaining-length comparisons. The output is
only byte/count metadata: strings are neither rendered nor exposed, and UTF-8
and tag-name semantics are not validated. Truncated/malformed comments return
an error under this strict parser policy; a future decoder must explicitly
decide whether to discard optional tags, not mislabel invalid tags as validated.
Outputs are reset on failure. Trailing padding is not treated as metadata.

The caller must obtain packets from the Ogg reader, enforce header ordering and
Ogg/Vorbis mapping rules, validate the third/setup header, then decode audio.
These functions alone do none of those jobs. Packet and comment-count limits
do not yet constitute a bound on the complete decoder's workspace.

## Validation (2026-09-06)

- `make validate`: image/WAV/Ogg/header tests and repository checks passed.
- ASan/UBSan: header tests passed, including all truncated prefixes, all 256
  block-exponent combinations, oversized lengths/counts, resource limits,
  invalid framing, output reset and 10000 deterministic mutation iterations
  over each packet type (not coverage-guided fuzzing).
- `make vorbis-reference-test`: 317 comparisons against installed Xiph
  libVorbis 1.3.7 passed for identification fields, block sizes, comment counts
  and malformed/truncated headers within the shared supported profile.
  Python 3/libvorbis are optional host-test dependencies only. The reference
  test loads a separately compiled host shared object, not the kernel.
- `-ffreestanding -fno-builtin` compiled; `nm -u` found no undefined symbols.

Evidence: `build/vorbis-headers-validation.log`,
`build/vorbis-headers-sanitizers.log`, `build/vorbis-headers-reference.log`.

No public ABI/version/pin/release changed. No new VM run is claimed: this code
is not connected to the player. Remaining work includes setup/codebook/floor/
residue/mapping validation, bounded decoding workspace, PCM synthesis with
reference audio fixtures, consumer integration and QEMU/VMware playback gates.

The private [codebook setup primitive](vorbis-codebooks.md) is now implemented
and separately tested. Aggregate setup validation and PCM synthesis remain open.
