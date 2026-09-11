"""Host-only floor1 packet fixtures encoded by Xiph's Huffman implementation.

Checks setup retention, all scalar values, exact bits and every truncated prefix.
Not a full libvorbis floor/PCM comparison; internal encoder ABI requires 1.3.7.
"""
import ctypes as c
import ctypes.util
import pathlib
import random
import sys
from test_vorbis_reference import configure
from test_vorbis_codebook_reference import StaticBook, OggBits, Bits
from test_vorbis_huffman_reference import CodecBook, Node, Tree
from test_vorbis_floor1_reference import Plan, oracle


class Class(c.Structure):
    _fields_ = [("dimensions", c.c_uint8), ("subclasses", c.c_uint8),
                ("masterbook", c.c_uint16), ("books", c.c_int16 * 8)]


class Config(c.Structure):
    _fields_ = [("plan", Plan), ("classes", Class * 16),
                ("partition_class", c.c_uint8 * 31), ("partitions", c.c_uint8),
                ("book_count", c.c_uint16)]


class Values(c.Structure):
    _fields_ = [("y", c.c_uint32 * 65), ("present", c.c_uint8), ("exhausted", c.c_uint8)]


def main():
    ref = c.CDLL(ctypes.util.find_library("vorbis") or "libvorbis.so.0")
    ogg = c.CDLL(ctypes.util.find_library("ogg") or "libogg.so.0")
    lib = c.CDLL(str(pathlib.Path(sys.argv[1]).resolve()))
    assert configure(ref, "vorbis_version_string", c.c_char_p)() == b"Xiph.Org libVorbis 1.3.7"
    init = configure(ref, "vorbis_book_init_encode", c.c_int, c.POINTER(CodecBook), c.POINTER(StaticBook))
    clear = configure(ref, "vorbis_book_clear", None, c.POINTER(CodecBook))
    encode = configure(ref, "vorbis_book_encode", c.c_int, c.POINTER(CodecBook), c.c_int, c.POINTER(OggBits))
    wi = configure(ogg, "oggpack_writeinit", None, c.POINTER(OggBits))
    wc = configure(ogg, "oggpack_writeclear", None, c.POINTER(OggBits))
    write = configure(ogg, "oggpack_write", None, c.POINTER(OggBits), c.c_ulong, c.c_int)
    length = configure(ogg, "oggpack_bytes", c.c_long, c.POINTER(OggBits))
    bit_length = configure(ogg, "oggpack_bits", c.c_long, c.POINTER(OggBits))
    build = configure(lib, "capy_vorbis_huffman_build", c.c_int, c.c_void_p, c.c_uint32, c.c_void_p, c.c_size_t, c.POINTER(Tree))
    bi = configure(lib, "capy_vorbis_bits_init", c.c_int, c.POINTER(Bits), c.c_void_p, c.c_size_t, c.c_size_t)
    parse = configure(lib, "capy_vorbis_floor1_config_read", c.c_int, c.POINTER(Bits), c.c_uint, c.POINTER(Config))
    decode = configure(lib, "capy_vorbis_floor1_packet_read", c.c_int, c.POINTER(Config), c.c_void_p, c.c_size_t, c.POINTER(Bits), c.POINTER(Values))
    curve = configure(lib, "capy_vorbis_floor1_curve", c.c_int, c.POINTER(Plan), c.c_void_p, c.c_size_t, c.c_size_t, c.c_void_p, c.c_size_t)
    books = (CodecBook * 5)()
    trees = (Tree * 5)()
    owned = []
    def reader(ob, cut=None):
        bits = Bits()
        assert bi(c.byref(bits), ob.buffer, length(c.byref(ob)), 65536) == 0
        bits.count = bit_length(c.byref(ob)) if cut is None else cut
        return bits
    rng = random.Random(0x10F10004)
    prefixes = scalars = 0
    try:
        for i, depth in enumerate((8, 1, 2, 3, 4)):
            count = 1 << depth
            lengths = (c.c_ubyte * count)(*[depth] * count)
            static = StaticBook(1, count, lengths, 0, 0, 0, 0, 0, None, 0)
            nodes = (Node * (2 * count - 1))()
            owned.append((lengths, static, nodes))  # Keep borrowed C pointers alive.
            assert init(c.byref(books[i]), c.byref(static)) == 0 and books[i].codes
            assert build(lengths, count, nodes, len(nodes), c.byref(trees[i])) == 0
        for trial in range(256):
            setup, packet = OggBits(), OggBits()
            wi(c.byref(setup)); wi(c.byref(packet))
            try:
                dim, sub = rng.randrange(1, 9), trial % 4
                partitions = rng.randrange(min(31, 63 // dim) + 1)
                chosen = trial % 16
                multiplier, range_bits = 1 + trial % 4, rng.randrange(6, 16)
                x = [0, 1 << range_bits] + rng.sample(range(1, 1 << range_bits), partitions * dim)
                refs = [rng.choice((-1, 1, 2, 3, 4)) for _ in range(1 << sub)]
                put = lambda value, width: write(c.byref(setup), value, width)
                put(partitions, 5)
                for _ in range(partitions): put(chosen, 4)
                for index in range(chosen + 1 if partitions else 0):
                    if index != chosen:
                        put(0, 3); put(0, 2); put(0, 8)
                        continue
                    put(dim - 1, 3); put(sub, 2)
                    if sub: put(0, 8)
                    for book in refs: put(book + 1, 8)
                put(multiplier - 1, 2); put(range_bits, 4)
                for pos in x[2:]: put(pos, range_bits)
                config = Config(); bits = reader(setup)
                assert parse(c.byref(bits), 5, c.byref(config)) == 0
                assert bits.position == bit_length(c.byref(setup))
                assert list(config.plan.x)[:len(x)] == x and config.partitions == partitions
                assert config.plan.multiplier == multiplier and config.book_count == 5
                if partitions:
                    cl = config.classes[chosen]
                    assert (cl.dimensions, cl.subclasses, cl.masterbook) == (dim, sub, 0)
                    assert list(cl.books)[:len(refs)] == refs
                for cut in range(bit_length(c.byref(setup))):
                    rejected = Config(); bits = reader(setup, cut)
                    assert parse(c.byref(bits), 5, c.byref(rejected)) != 0
                    assert not rejected.book_count and not rejected.plan.count
                    prefixes += 1
                limit = (256, 128, 86, 64)[multiplier - 1]
                expected = [rng.randrange(limit), rng.randrange(limit)]
                write(c.byref(packet), 1, 1)
                for value in expected: write(c.byref(packet), value, (limit - 1).bit_length())
                for _ in range(partitions):
                    selector = rng.randrange(256) if sub else 0
                    if sub: assert encode(c.byref(books[0]), selector, c.byref(packet)) == 8
                    for _ in range(dim):
                        book = refs[selector & ((1 << sub) - 1)]
                        selector >>= sub
                        value = rng.randrange(1 << book) if book >= 0 else 0
                        if book >= 0: assert encode(c.byref(books[book]), value, c.byref(packet)) == book
                        expected.append(value)
                values = Values(); bits = reader(packet)
                assert decode(c.byref(config), trees, 5, c.byref(bits), c.byref(values)) == 0
                assert values.present and not values.exhausted
                assert list(values.y)[:len(x)] == expected and bits.position == bit_length(c.byref(packet))
                output = (c.c_uint8 * 128)()
                assert curve(c.byref(config.plan), values.y, len(x), 128, output, 128) == 0
                assert list(output) == oracle(x, expected, multiplier, 128)
                scalars += len(expected)
                for cut in range(bit_length(c.byref(packet))):
                    bits = reader(packet, cut); values = Values()
                    assert decode(c.byref(config), trees, 5, c.byref(bits), c.byref(values)) == 0
                    assert not values.present and values.exhausted and bits.error == -4
                    assert not any(values.y)
                    prefixes += 1
            finally:
                wc(c.byref(setup)); wc(c.byref(packet))
    finally:
        for book in books: clear(c.byref(book))
    print(f"[vorbis-floor1-packet-reference] 256 reference-encoded floors / {scalars} scalars / {prefixes} truncated bit-prefixes passed")


if __name__ == "__main__":
    main()
