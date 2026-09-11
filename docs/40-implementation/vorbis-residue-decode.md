# Complete Vorbis residue scheduling (2026-09-09)

The private `vorbis_residue_decode` layer consumes retained residue setup and
orchestrates classification words, base-N class expansion and all eight passes.
Types 0 and 1 are scheduled independently for each active channel. Type 2 uses
one classification stream and applies sequential VQ values across the
interleaved channel bundle, as required before inverse coupling.

The caller supplies channel-major spectral output, a transactional scratch
copy, one partition scratch for types 0/1 (two for type 2), and byte-sized
classification storage. Bounds are fixed at 8 channels and 4096 bins per
channel. No allocation, I/O, platform callback, PCM conversion or IMDCT occurs.
`max_vectors` bounds aggregate VQ work across every channel and pass.

Hard errors preserve caller output, clear the result and do not roll back
already-consumed bits. Audio EOF is nominal: complete vectors accumulated in
scratch are committed, `exhausted=1` is returned, and scheduling stops. An
all-skipped bundle consumes no bits; for Type 2, any active channel enables the
whole interleaved bundle, matching the coupling-domain rule.

Focused tests cover all three residue types, two-channel interleave, channel
skip, all-skipped input, partial packet exhaustion, vector-budget rejection and
damaged retained state. The independent deterministic oracle exercises 2000
random schedules across 1..8 channels and varied begin/end/partition bounds.
Sanitizer, freestanding and whole-repository evidence are recorded in the
validation document.

This remains a private synthesis primitive. Public ABI, package feature bits,
version, CapyOS adapter and player behavior are unchanged. The next boundary is
audio-packet mode/window parsing and orchestration into Floor1, residue,
coupling, floor gain, IMDCT and overlap-add.
