# Vorbis window and overlap-add (2026-09-09)

The private `vorbis_window` layer applies the specification window to one
inverse-MDCT block and overlaps a current block with the cached previous block.
It supports every legal power-of-two block size from 64 through 8192 and all
short/long transition pairs. Finished output length is exactly
`previous_size/4 + current_size/4`; first-frame priming remains the future
packet decoder's lifecycle responsibility.

Window slopes use the Vorbis nested-sine equation. To remain freestanding, a
binary64 polynomial evaluates sine on `[0, pi/2]`; no `sin`, `cos`, `sqrt`,
heap, I/O or platform callback is referenced. The first omitted Taylor term is
below `5e-15` on the interval and output is stored as binary32. Window and
overlap hard failures leave output unchanged through caller-owned scratch.
Inputs, products and sums must remain finite within the explicit `max_abs`.

The independent Python `math.sin` oracle checked 65280 coefficients across all
legal sizes/transitions and another 65280 overlap samples across every legal
previous/current size pair. All binary32 coefficients matched exactly in the
executed environment. Focused ASan/UBSan passed. GCC 15.2 freestanding analysis
reported no unresolved symbols and static frames of 48 bytes for window and
overlap, plus 8 bytes for the sine helper.

The maintained host benchmark records 303 retained batch samples after warmup.
For a full 8192-sample window, median wall time was 166937.95 ns, p95
187277.7 ns and p99 206062.45 ns on the recorded WSL host. These are warm host
batch averages, not VM, real-time tail-latency or an acceptance threshold. Raw
samples and build hashes are in `build/vorbis-primitives-20260909.json`.

The inverse transform feeding this layer is now implemented and validated in
[the private MDCT stage](vorbis-mdct.md).

This implementation is still private. The CapyOS Etapa 10 branch now preserves
the complete architectural FXSAVE image across task switches and has a two-task
ring-3 XMM-cookie runtime gate. Its normal userland build consequently enables
SSE2 scalar floating point. That removes the earlier compile-time integration
blocker, but does not itself constitute Vorbis playback evidence: packet-level
entropy orchestration, a public ABI decision and player/VM validation remain.
The historical failed compilation probe is retained as evidence of the old
boundary in `build/vorbis-window-userland-probe.log`.
