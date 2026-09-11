# Vorbis codebook setup primitive (development)

`src/audio/vorbis_codebook.{h,c}` provides private LSB-first bit reading and
single-codebook parsing, following [Xiph Vorbis I section 3.2](https://www.xiph.org/vorbis/doc/Vorbis_I_spec.html).
It is not the complete setup header, a Huffman symbol decoder or PCM synthesis.
No public ABI, advertised audio feature, production version or pin changes.

Supported syntax: ordered and unordered lengths, sparse entries, lookup types
0/1/2, raw packed minimum/delta values, value width and sequence flag. The parser
checks Huffman over/undersubscription and the 2015 single-active-entry erratum
(length must be one). All-unused books are rejected under this strict policy.
Lookup type 1 uses bounded integer exponentiation/search, not floating rounding.
Lookup values remain in borrowed immutable packet storage; their bit offset and
count are returned after verifying the entire encoded range is present. No
floating vectors or decode trees are allocated or constructed yet.

## Bounds

- Explicit input-byte budget and checked byte-to-bit conversion.
- Explicit entry/dimension/lookup-value budgets; implementation ceilings of
  65536 entries and 256 dimensions per book.
- Caller-provided length storage, capacity checked before writing; no heap,
  platform callbacks, filesystem or network dependencies.
- Ordered zero-count runs advance length and cannot exceed 32 iterations.
- Tree validation uses a fixed 33-element count array and 64-bit slot arithmetic.
- Errors stick to the bit reader; metadata resets on error. Partial scratch
  lengths must be discarded. Input/state/output/scratch must not overlap.

These per-book limits do not bound the full setup or decoder. The forthcoming
setup owner must enforce aggregate budgets across all books and allocations,
then validate floors, residues, mappings, modes and their references.

## Validation (2026-09-06)

`make validate` and focused ASan/UBSan passed. Tests cover all lookup forms,
ordered/sparse books, empty runs, invalid trees, the single-entry exception,
truncation at every bit boundary of fixtures, limits, 32-bit code lengths,
integer roots near perfect powers, and 10000 deterministic mutations.
These mutations are not a coverage-guided fuzzing claim.

`make vorbis-reference-test` passed the previous 317 header comparisons and 48
new books generated independently by Xiph libvorbis 1.3.7. The new oracle checks
lengths, dimensions, lookup counts/raw fields, final bit positions and lookup
payload values. Its optional internal `static_codebook` test ABI is explicitly
restricted to libvorbis 1.3.7. Production code does not link libvorbis or libogg.

Freestanding compilation with `-fno-builtin` passed and `nm -u` found no undefined
symbols. Logs: `build/vorbis-codebook-validation.log`,
`build/vorbis-codebook-sanitizers.log`, `build/vorbis-codebook-reference.log`.
No VM evidence applies yet: the player still has no Vorbis PCM path.

Aggregate setup parsing and section-reference validation are now implemented
in the private [setup validator](vorbis-setup.md); decoder state construction
and PCM synthesis remain pending.
