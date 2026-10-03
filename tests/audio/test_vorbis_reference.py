"""Optional host-only oracle; requires libvorbis, never a production dependency.

ctypes layouts follow Xiph include/vorbis/codec.h and include/ogg/ogg.h.
Compare only header parsing within CapyCodecs' supported resource profile.
"""
import ctypes as c
import ctypes.util
import pathlib
import sys


class Info(c.Structure):
    _fields_ = [("version", c.c_int), ("channels", c.c_int),
                ("rate", c.c_long), ("upper", c.c_long),
                ("nominal", c.c_long), ("lower", c.c_long),
                ("window", c.c_long), ("setup", c.c_void_p)]


class Comment(c.Structure):
    _fields_ = [("strings", c.POINTER(c.c_char_p)),
                ("lengths", c.POINTER(c.c_int)), ("count", c.c_int),
                ("vendor", c.c_char_p)]


class Packet(c.Structure):
    _fields_ = [("data", c.c_void_p), ("size", c.c_long),
                ("bos", c.c_long), ("eos", c.c_long),
                ("granule", c.c_int64), ("number", c.c_int64)]


class Limits(c.Structure):
    _fields_ = [("input", c.c_size_t), ("output", c.c_size_t),
                ("frames", c.c_uint64), ("rate", c.c_uint32),
                ("channels", c.c_uint16), ("chunks", c.c_uint32)]


class Identification(c.Structure):
    _fields_ = [("rate", c.c_uint32), ("channels", c.c_uint16),
                ("small", c.c_uint16), ("large", c.c_uint16)]


class Tags(c.Structure):
    _fields_ = [("vendor", c.c_uint32), ("count", c.c_uint32),
                ("bytes", c.c_size_t)]


def configure(lib, name, result, *args):
    fn = getattr(lib, name)
    fn.restype, fn.argtypes = result, args
    return fn


def main():
    ref = c.CDLL(ctypes.util.find_library("vorbis") or "libvorbis.so.0")
    ours = c.CDLL(str(pathlib.Path(sys.argv[1]).resolve()))
    init = configure(ref, "vorbis_info_init", None, c.POINTER(Info))
    clear = configure(ref, "vorbis_info_clear", None, c.POINTER(Info))
    ci = configure(ref, "vorbis_comment_init", None, c.POINTER(Comment))
    cc = configure(ref, "vorbis_comment_clear", None, c.POINTER(Comment))
    parse = configure(ref, "vorbis_synthesis_headerin", c.c_int,
                      c.POINTER(Info), c.POINTER(Comment), c.POINTER(Packet))
    block = configure(ref, "vorbis_info_blocksize", c.c_int, c.POINTER(Info), c.c_int)
    version = configure(ref, "vorbis_version_string", c.c_char_p)
    defaults = configure(ours, "capy_audio_default_limits", None, c.POINTER(Limits))
    identify = configure(ours, "capy_vorbis_parse_identification", c.c_int,
                         c.c_void_p, c.c_size_t, c.POINTER(Limits), c.POINTER(Identification))
    tags_parse = configure(ours, "capy_vorbis_parse_comments", c.c_int,
                           c.c_void_p, c.c_size_t, c.POINTER(Limits), c.POINTER(Tags))
    limits = Limits()
    defaults(c.byref(limits))
    ident = b"\x01vorbis" + bytes(4) + b"\x02" + (48000).to_bytes(4, "little") + bytes(12) + b"\xb8\x01"
    comments = b"\x03vorbis\x03\0\0\0abc\x02\0\0\0\x03\0\0\0A=B\0\0\0\0\x01"
    cases = 0

    def compare(id_bytes, comment_bytes=None):
        nonlocal cases
        vi, vc = Info(), Comment()
        init(c.byref(vi))
        ci(c.byref(vc))
        try:
            buf = c.create_string_buffer(id_bytes)
            packet = Packet(c.cast(buf, c.c_void_p), len(id_bytes), 1, 0, 0, 0)
            expected = parse(c.byref(vi), c.byref(vc), c.byref(packet))
            actual = Identification()
            rc = identify(buf, len(id_bytes), c.byref(limits), c.byref(actual))
            assert (rc == 0) == (expected == 0), (id_bytes.hex(), rc, expected)
            if rc == 0:
                assert (actual.rate, actual.channels, actual.small, actual.large) == (
                    vi.rate, vi.channels, block(c.byref(vi), 0), block(c.byref(vi), 1))
            if comment_bytes is not None:
                assert rc == 0
                buf = c.create_string_buffer(comment_bytes)
                packet = Packet(c.cast(buf, c.c_void_p), len(comment_bytes), 0, 0, 0, 1)
                expected = parse(c.byref(vi), c.byref(vc), c.byref(packet))
                tags = Tags()
                rc = tags_parse(buf, len(comment_bytes), c.byref(limits), c.byref(tags))
                assert (rc == 0) == (expected == 0), (comment_bytes.hex(), rc, expected)
                if rc == 0:
                    assert (tags.vendor, tags.count, tags.bytes) == (
                        len(vc.vendor), vc.count, sum(vc.lengths[i] for i in range(vc.count)))
            cases += 1
        finally:
            cc(c.byref(vc))
            clear(c.byref(vi))

    for value in range(256):
        compare(ident[:28] + bytes([value]) + ident[29:])
    for size in range(len(ident)):
        compare(ident[:size])
    compare(ident, comments)
    for size in range(len(comments)):
        compare(ident, comments[:size])
    print(f"[vorbis-reference] {cases} header comparisons passed against {version().decode()}")


if __name__ == "__main__":
    main()
