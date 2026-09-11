# Private Vorbis post-entropy synthesis (2026-09-11)

`src/audio/vorbis_synthesis.{h,c}` connects the retained mapping and floor1
state to the already validated inverse-coupling, floor gain, inverse MDCT,
window and overlap primitives. Its result is interleaved binary32 PCM frames.
The first packet only primes overlap state, as required by the Vorbis packet
lifecycle; later packets produce `previous_size/4 + current_size/4` frames.

The contract is caller-owned and allocation-free. It binds retained overlap to
the original channel count, supports all legal short/long block transitions,
and publishes PCM, previous-window storage and decoder state only after every
channel succeeds. Spectral input is documented as consumable workspace. A
floor packet exhausted while reading entropy makes every channel silent. An
absent floor silences its own channel. Floor0 is rejected as unsupported rather
than being synthesized incorrectly.

The focused test covers first-packet priming, two-channel interleaving, a
nonzero floor1/residue impulse through the entire mathematical chain, global
silence on nominal packet exhaustion, persistent-state atomicity and channel
binding.

This is deliberately not advertised in public audio ABI v1. The missing
packet-level orchestrator must still build retained Huffman trees, decode floors
and schedule each residue submap using the submap's actual channel bundle before
calling this stage. In particular, residue type 2 cannot be decoded over all
stream channels when a mapping assigns only a subset to that submap. Complete
Ogg packet lifecycle, differential decoded-PCM fixtures, bounded integer PCM
conversion and CapyOS player integration remain required before public Vorbis
support.
