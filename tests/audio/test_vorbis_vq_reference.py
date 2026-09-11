"""Host-only VQ oracle; Xiph 1.3.7 internal ABI, never a runtime dependency."""
import ctypes as c
import ctypes.util
import math
import pathlib
import random
import sys
from test_vorbis_reference import configure
from test_vorbis_codebook_reference import StaticBook, OggBits, Bits, Limits, Book
from test_vorbis_huffman_reference import CodecBook


def main():
    ref = c.CDLL(ctypes.util.find_library("vorbis") or "libvorbis.so.0")
    ogg = c.CDLL(ctypes.util.find_library("ogg") or "libogg.so.0")
    ours = c.CDLL(str(pathlib.Path(sys.argv[1]).resolve()))
    assert configure(ref, "vorbis_version_string", c.c_char_p)() == b"Xiph.Org libVorbis 1.3.7"
    fp = configure(ref, "_float32_pack", c.c_long, c.c_float)
    fu = configure(ref, "_float32_unpack", c.c_float, c.c_long)
    unpack = configure(ours, "capy_vorbis_float_unpack", c.c_int, c.c_uint32, c.POINTER(c.c_float))
    floating = 0
    # Xiph clamps exponents outside [-63,63]; compare only its unclamped domain.
    # Outside it, independently check the specification's literal value and our
    # fail-closed binary32 overflow policy, not Xiph's clamped approximation.
    patterns = [sign | (exponent << 21) | mantissa for exponent in range(1024)
                for mantissa in (0, 1, 0xfffff, 0x100000, 0x1fffff)
                for sign in (0, 0x80000000)]
    rng = random.Random(0x10F10003)
    patterns.extend(rng.getrandbits(32) for _ in range(100000))
    for raw in patterns:
        exponent = ((raw >> 21) & 1023) - 788
        value = math.ldexp(raw & 0x1fffff, exponent)
        if raw & 0x80000000:
            value = -value
        observed = c.c_float(99)
        status = unpack(raw, c.byref(observed))
        if abs(value) > float.fromhex("0x1.fffffep127"):
            assert status == -6 and observed.value == 0, hex(raw)
        else:
            assert status == 0 and observed.value == c.c_float(value).value, hex(raw)
            if raw & 0x1fffff:  # Include the sign of underflowed zero.
                expected = c.c_float(value)
                assert bytes(observed) == bytes(expected), hex(raw)
            if -63 <= exponent <= 63:
                assert observed.value == fu(raw), hex(raw)
        floating += 1
    wi = configure(ogg, "oggpack_writeinit", None, c.POINTER(OggBits))
    wc = configure(ogg, "oggpack_writeclear", None, c.POINTER(OggBits))
    size = configure(ogg, "oggpack_bytes", c.c_long, c.POINTER(OggBits))
    pack = configure(ref, "vorbis_staticbook_pack", c.c_int, c.POINTER(StaticBook), c.POINTER(OggBits))
    di = configure(ref, "vorbis_book_init_decode", c.c_int, c.POINTER(CodecBook), c.POINTER(StaticBook))
    dc = configure(ref, "vorbis_book_clear", None, c.POINTER(CodecBook))
    bi = configure(ours, "capy_vorbis_bits_init", c.c_int, c.POINTER(Bits), c.c_void_p, c.c_size_t, c.c_size_t)
    parse = configure(ours, "capy_vorbis_book_parse", c.c_int, c.POINTER(Bits), c.POINTER(Limits), c.c_void_p, c.c_size_t, c.POINTER(Book))
    expand = configure(ours, "capy_vorbis_vq_expand", c.c_int, c.POINTER(Book), c.c_void_p, c.c_size_t, c.c_uint32, c.c_float, c.c_void_p, c.c_size_t)
    vectors = books = 0
    for entries in (1, 4, 16, 64):
        for dimensions in (1, 2, 4, 8):
            for kind in (1, 2):
                root = 1
                while (root + 1) ** dimensions <= entries:
                    root += 1
                count = root if kind == 1 else entries * dimensions
                for sequence in (0, 1):
                    for minimum, delta, quant in ((a, b, q) for a, b in
                            ((-3.0, 0.25), (0.1, -0.3)) for q in (1, 4, 8, 16)):
                        lengths = (c.c_ubyte * entries)(*[max(1, (entries - 1).bit_length())] * entries)
                        values = (c.c_long * count)(*[(i * 7919 + 3) % (1 << quant) for i in range(count)])
                        original = StaticBook(dimensions, entries, lengths, kind, fp(minimum), fp(delta), quant, sequence, values, 0)
                        ob, decoded = OggBits(), CodecBook()
                        wi(c.byref(ob))
                        try:
                            assert pack(c.byref(original), c.byref(ob)) == 0
                            assert di(c.byref(decoded), c.byref(original)) == 0
                            assert decoded.used == entries and decoded.values and decoded.index
                            bits, book, limits = Bits(), Book(), Limits(64, 8, 512)
                            scratch = (c.c_ubyte * entries)()
                            assert bi(c.byref(bits), ob.buffer, size(c.byref(ob)), 65536) == 0
                            assert parse(c.byref(bits), c.byref(limits), scratch, entries, c.byref(book)) == 0
                            expected = c.cast(decoded.values, c.POINTER(c.c_float))
                            symbols = c.cast(decoded.index, c.POINTER(c.c_int))
                            assert sorted(symbols[i] for i in range(entries)) == list(range(entries))
                            for i in range(entries):
                                output = (c.c_float * dimensions)()
                                assert expand(c.byref(book), ob.buffer, size(c.byref(ob)), symbols[i], 1e20, output, dimensions) == 0
                                assert list(output) == [expected[i * dimensions + j] for j in range(dimensions)], (entries, dimensions, kind, sequence, symbols[i])
                                vectors += 1
                            books += 1
                        finally:
                            dc(c.byref(decoded))
                            wc(c.byref(ob))
    print(f"[vorbis-vq-reference] {floating} float patterns; {vectors} vectors from {books} Xiph books matched")


if __name__ == "__main__":
    main()
