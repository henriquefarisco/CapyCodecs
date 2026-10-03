# Vorbis packet mode and window selection (2026-09-09)

The private `vorbis_packet` prefix reader rejects header packets in the audio
phase, decodes the bounded mode number from retained setup, validates its
mapping reference and reads previous/next flags only for long blocks. It then
derives the four Vorbis window transition boundaries for short-short,
short-long, long-short and long-long transitions.

Identification block sizes are supplied by the caller and must be powers of two
from 64 through 8192 with short no larger than long. Output is zero on error;
the packet bit reader keeps sticky truncation/corruption and is not rolled back.
There is no allocation, floating-point work, floor/residue decode, transform,
overlap or PCM output. The resulting boundaries are now consumed by the
[private window and overlap layer](vorbis-window.md).

Focused tests cover one-mode zero-width selectors, short and long modes,
asymmetric window flags, every truncated prefix, non-audio packet type, invalid
mode numbers, damaged mapping references and invalid block-size contracts.
