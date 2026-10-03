"""Host-only independent fixtures generated with Xiph libvorbis 1.3.7.

Uses the internal static_codebook test ABI; explicitly version-gated.
No libvorbis dependency is introduced into production CapyCodecs.
"""
import ctypes as c
import ctypes.util
import pathlib
import sys
from test_vorbis_reference import configure


class StaticBook(c.Structure):
    _fields_ = [("dim", c.c_long), ("entries", c.c_long),
                ("lengths", c.POINTER(c.c_ubyte)), ("maptype", c.c_int),
                ("minimum", c.c_long), ("delta", c.c_long),
                ("quant", c.c_int), ("sequence", c.c_int),
                ("values", c.POINTER(c.c_long)), ("allocated", c.c_int)]


class OggBits(c.Structure):
    _fields_ = [("endbyte", c.c_long), ("endbit", c.c_int),
                ("buffer", c.c_void_p), ("ptr", c.c_void_p), ("storage", c.c_long)]


class Bits(c.Structure):
    _fields_ = [("data", c.c_void_p), ("count", c.c_size_t),
                ("position", c.c_size_t), ("error", c.c_int)]


class Limits(c.Structure):
    _fields_ = [("entries", c.c_uint32), ("dimensions", c.c_uint32),
                ("values", c.c_uint32)]


class Book(c.Structure):
    _fields_ = [(name, c.c_uint32) for name in (
        "entries", "dimensions", "used", "type", "values", "minimum",
        "delta", "value_bits", "sequence")] + [("offset", c.c_size_t)]


def main():
    vorbis = c.CDLL(ctypes.util.find_library("vorbis") or "libvorbis.so.0")
    ogg = c.CDLL(ctypes.util.find_library("ogg") or "libogg.so.0")
    ours = c.CDLL(str(pathlib.Path(sys.argv[1]).resolve()))
    version = configure(vorbis, "vorbis_version_string", c.c_char_p)()
    assert version == b"Xiph.Org libVorbis 1.3.7", "Reference internal ABI requires 1.3.7"
    init = configure(ogg, "oggpack_writeinit", None, c.POINTER(OggBits))
    clear = configure(ogg, "oggpack_writeclear", None, c.POINTER(OggBits))
    size = configure(ogg, "oggpack_bytes", c.c_long, c.POINTER(OggBits))
    bit_size = configure(ogg, "oggpack_bits", c.c_long, c.POINTER(OggBits))
    pack = configure(vorbis, "vorbis_staticbook_pack", c.c_int,
                     c.POINTER(StaticBook), c.POINTER(OggBits))
    bi = configure(ours, "capy_vorbis_bits_init", c.c_int,
                   c.POINTER(Bits), c.c_void_p, c.c_size_t, c.c_size_t)
    parse = configure(ours, "capy_vorbis_book_parse", c.c_int,
                      c.POINTER(Bits), c.POINTER(Limits), c.c_void_p,
                      c.c_size_t, c.POINTER(Book))
    read = configure(ours, "capy_vorbis_bits_read", c.c_uint32,
                     c.POINTER(Bits), c.c_uint)
    cases = 0
    for entries in (1, 2, 16, 256):
        for dimensions in (1, 2, 8, 16):
            for kind in (0, 1, 2):
                root = 1
                while (root + 1) ** dimensions <= entries:
                    root += 1
                count = root if kind == 1 else entries * dimensions if kind == 2 else 0
                lengths = (c.c_ubyte * entries)(*[max(1, (entries - 1).bit_length())] * entries)
                values = (c.c_long * max(1, count))(*[i % 16 for i in range(count)])
                original = StaticBook(dimensions, entries, lengths, kind,
                                      0, 0x60100000, 4, 1, values, 0)
                ob = OggBits()
                init(c.byref(ob))
                try:
                    assert pack(c.byref(original), c.byref(ob)) == 0
                    b, result, limits = Bits(), Book(), Limits(256, 32, 8192)
                    decoded = (c.c_ubyte * entries)()
                    assert bi(c.byref(b), ob.buffer, size(c.byref(ob)), 65536) == 0
                    assert parse(c.byref(b), c.byref(limits), decoded, entries, c.byref(result)) == 0
                    assert b.position == bit_size(c.byref(ob))
                    assert list(decoded) == list(lengths)
                    assert (result.entries, result.dimensions, result.type, result.values) == (entries, dimensions, kind, count)
                    if kind:
                        assert (result.minimum, result.delta, result.value_bits, result.sequence) == (0, 0x60100000, 4, 1)
                        b.position = result.offset
                        assert [read(c.byref(b), 4) for _ in range(count)] == list(values)[:count]
                    cases += 1
                finally:
                    clear(c.byref(ob))
    print(f"[vorbis-codebook-reference] {cases} Xiph-generated books matched, including bit offsets and lookup payloads")


if __name__ == "__main__":
    main()
