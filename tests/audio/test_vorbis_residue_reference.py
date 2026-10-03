"""Xiph 1.3.7 successful-partition oracle; separate spec EOF progress oracle."""
import ctypes as c
import ctypes.util
import pathlib
import sys
from test_vorbis_reference import configure
from test_vorbis_codebook_reference import StaticBook, OggBits, Bits, Limits, Book
from test_vorbis_huffman_reference import CodecBook, Node, Tree


class Result(c.Structure):
    _fields_ = [("vectors", c.c_uint32), ("exhausted", c.c_uint8)]


def main():
    ref = c.CDLL(ctypes.util.find_library("vorbis") or "libvorbis.so.0")
    ogg = c.CDLL(ctypes.util.find_library("ogg") or "libogg.so.0")
    lib = c.CDLL(str(pathlib.Path(sys.argv[1]).resolve()))
    assert configure(ref, "vorbis_version_string", c.c_char_p)() == b"Xiph.Org libVorbis 1.3.7"
    ei = configure(ref, "vorbis_book_init_encode", c.c_int, c.POINTER(CodecBook), c.POINTER(StaticBook))
    di = configure(ref, "vorbis_book_init_decode", c.c_int, c.POINTER(CodecBook), c.POINTER(StaticBook))
    clear = configure(ref, "vorbis_book_clear", None, c.POINTER(CodecBook))
    encode = configure(ref, "vorbis_book_encode", c.c_int, c.POINTER(CodecBook), c.c_int, c.POINTER(OggBits))
    pack = configure(ref, "vorbis_staticbook_pack", c.c_int, c.POINTER(StaticBook), c.POINTER(OggBits))
    wi = configure(ogg, "oggpack_writeinit", None, c.POINTER(OggBits))
    wc = configure(ogg, "oggpack_writeclear", None, c.POINTER(OggBits))
    ri = configure(ogg, "oggpack_readinit", None, c.POINTER(OggBits), c.c_void_p, c.c_int)
    size = configure(ogg, "oggpack_bytes", c.c_long, c.POINTER(OggBits))
    bit_size = configure(ogg, "oggpack_bits", c.c_long, c.POINTER(OggBits))
    functions = [configure(ref, name, c.c_long, c.POINTER(CodecBook), c.c_void_p, c.POINTER(OggBits), c.c_int)
                 for name in ("vorbis_book_decodevs_add", "vorbis_book_decodev_add")]
    bi = configure(lib, "capy_vorbis_bits_init", c.c_int, c.POINTER(Bits), c.c_void_p, c.c_size_t, c.c_size_t)
    parse = configure(lib, "capy_vorbis_book_parse", c.c_int, c.POINTER(Bits), c.POINTER(Limits), c.c_void_p, c.c_size_t, c.POINTER(Book))
    build = configure(lib, "capy_vorbis_huffman_build", c.c_int, c.c_void_p, c.c_uint32, c.c_void_p, c.c_size_t, c.POINTER(Tree))
    partition = configure(lib, "capy_vorbis_residue_partition", c.c_int, c.c_uint,
                          c.POINTER(Book), c.POINTER(Tree), c.c_void_p, c.c_size_t,
                          c.POINTER(Bits), c.c_size_t, c.c_uint32, c.c_float,
                          c.c_void_p, c.c_size_t, c.c_void_p, c.c_size_t, c.POINTER(Result))
    def reader(ob, cut=None):
        bits = Bits()
        assert bi(c.byref(bits), ob.buffer, size(c.byref(ob)), 65536) == 0
        bits.count = bit_size(c.byref(ob)) if cut is None else cut
        return bits
    cases = prefixes = 0
    for entries in (4, 16):
        for dim in (1, 2, 4, 8, 16):
            for kind in (1, 2):
                for sequence in (0, 1):
                    root = 1
                    while (root+1)**dim <= entries: root += 1
                    count = root if kind == 1 else entries*dim
                    depth = (entries-1).bit_length()
                    lengths = (c.c_ubyte*entries)(*[depth]*entries)
                    quant = (c.c_long*count)(*[(i*7+3)%16 for i in range(count)])
                    original = StaticBook(dim, entries, lengths, kind, 0xe0380000, 0x5fd00000, 4, sequence, quant, 0)
                    assert configure(ref, "_float32_unpack", c.c_float, c.c_long)(original.delta) == 0.25
                    encoder, decoder, setup = CodecBook(), CodecBook(), OggBits()
                    wi(c.byref(setup))
                    try:
                        assert pack(c.byref(original), c.byref(setup)) == 0
                        assert ei(c.byref(encoder), c.byref(original)) == 0 and encoder.codes
                        assert di(c.byref(decoder), c.byref(original)) == 0 and decoder.values
                        book, tree, limits = Book(), Tree(), Limits(16, 16, 256)
                        bits = reader(setup); scratch_lengths = (c.c_ubyte*entries)()
                        assert parse(c.byref(bits), c.byref(limits), scratch_lengths, entries, c.byref(book)) == 0
                        nodes = (Node*(2*entries-1))()
                        assert build(scratch_lengths, entries, nodes, len(nodes), c.byref(tree)) == 0
                        mapping = c.cast(decoder.index, c.POINTER(c.c_int))
                        values = c.cast(decoder.values, c.POINTER(c.c_float))
                        vectors = {mapping[i]: [values[i*dim+j] for j in range(dim)] for i in range(entries)}
                        for type in (0, 1):
                            for n in (1, dim*5, dim*5+1):
                                total = n//dim if type == 0 else (n+dim-1)//dim
                                packet = OggBits(); wi(c.byref(packet))
                                try:
                                    symbols = [(i*3+1)%entries for i in range(total)]
                                    for symbol in symbols: assert encode(c.byref(encoder), symbol, c.byref(packet)) == depth
                                    base = [((i%7)-3)*0.0625 for i in range(n)]
                                    expected = (c.c_float*n)(*base)
                                    refbits = OggBits(); ri(c.byref(refbits), packet.buffer, size(c.byref(packet)))
                                    assert functions[type](c.byref(decoder), expected, c.byref(refbits), n) == 0
                                    output, scratch = (c.c_float*n)(*base), (c.c_float*n)()
                                    bits = reader(packet); result = Result()
                                    assert partition(type, c.byref(book), c.byref(tree), setup.buffer, size(c.byref(setup)), c.byref(bits), n, total, 1e6, output, n, scratch, n, c.byref(result)) == 0
                                    assert list(output) == list(expected)
                                    assert bits.position == bit_size(c.byref(refbits)) == total*depth
                                    assert result.vectors == total and not result.exhausted
                                    cases += 1
                                    # Xiph type0 buffers a whole partition before adding.
                                    # Spec vector-at-a-time partial results use a separate oracle.
                                    for cut in range(total*depth):
                                        complete = cut//depth; partial = list(base)
                                        for i, symbol in enumerate(symbols[:complete]):
                                            for j, value in enumerate(vectors[symbol]):
                                                index = i*dim+j if type else i+j*(n//dim)
                                                if index < n: partial[index] = c.c_float(partial[index]+value).value
                                        output = (c.c_float*n)(*base); bits = reader(packet, cut)
                                        assert partition(type, c.byref(book), c.byref(tree), setup.buffer, size(c.byref(setup)), c.byref(bits), n, total, 1e6, output, n, scratch, n, c.byref(result)) == 0
                                        assert result.exhausted and result.vectors == complete and bits.error == -4
                                        assert list(output) == partial
                                        prefixes += 1
                                finally: wc(c.byref(packet))
                    finally:
                        clear(c.byref(encoder)); clear(c.byref(decoder)); wc(c.byref(setup))
    print(f"[vorbis-residue-reference] {cases} Xiph partitions matched; {prefixes} truncated prefixes matched vector-progress semantics")


if __name__ == "__main__":
    main()
