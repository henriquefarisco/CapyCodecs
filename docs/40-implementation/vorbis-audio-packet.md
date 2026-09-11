# Private Vorbis audio-packet orchestration (2026-09-11)

`vorbis_audio_packet` connects mode/window selection, retained floor1 packet
entropy and complete residue scheduling. It publishes channel-major spectra and
decoded floor values for the post-entropy synthesis stage.

Each mapping submap is gathered into its own dense channel bundle before
residue decoding and scattered back afterward. This is essential for residue
type 2: its interleaved scalar domain contains only the channels assigned to
that submap, not every channel in the stream. Coupling pairs propagate the
nonzero-residue flag before bundle decode. The later synthesis layer applies
inverse coupling in reverse order.

All caller buffers are bounded and non-overlapping. Spectrum and floor outputs
are committed only after the complete packet succeeds. Bit consumption is not
rolled back. Floor entropy exhaustion is the Vorbis nominal end-of-packet case:
the orchestrator publishes absent/exhausted floors and an all-zero spectrum so
the synthesis layer can advance overlap with silence. Residue exhaustion keeps
the complete vectors decoded so far and leaves the undecoded tail zero.

The focused test exercises two channels assigned to distinct submaps, both
using residue type 2, and checks the exact scattered spectral vectors. It also
passes the same packet twice through post-entropy synthesis, proving first-frame
priming followed by nonzero interleaved float PCM. It checks late hard-failure
atomicity, alias rejection, floor truncation and all-channel silence. Setup preparation now builds
and retains every Huffman tree into a bounded caller-owned node pool before its
per-book length scratch is reused.

This remains private. A complete library streaming decoder still needs an owned
Ogg/setup/MDCT lifecycle around these primitives, granule-position trimming,
integer PCM conversion, broader real-stream fixtures and CapyOS playback/VM
evidence before the public audio ABI can claim Vorbis.

The optional `make vorbis-pcm-reference-test` now supplies the host lifecycle
around those private components and encodes a deterministic stereo fixture with
`oggenc` 1.4.3. The complete 11872-frame body published by FFmpeg 8.0.1 matched
at zero offset; mean absolute error was `2.57e-08` and peak error `1.49e-07` in
the expanded run. The 12000-frame source granule is 128 frames longer than that
FFmpeg output. Before granule handling the synthesis stream contained 12864
frames; the Ogg reader now publishes the final packet granule and the harness
trims it to exactly 12000. The test compares the complete FFmpeg body while
separately asserting the CapyCodecs output length through the harness log.
