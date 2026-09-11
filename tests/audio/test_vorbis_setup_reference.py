"""Generate real setup headers in memory with host libvorbisenc; no PCM gate."""
import ctypes as c
import ctypes.util
import pathlib
import sys
from test_vorbis_reference import Info, Comment, Packet, configure
from test_vorbis_codebook_reference import Book, Limits as BookLimits


class DSP(c.Structure):
    _fields_ = [("analysis", c.c_int), ("info", c.POINTER(Info)),
                ("pcm", c.c_void_p), ("pcmret", c.c_void_p)] + [
                (name, c.c_int) for name in ("storage", "current", "returned", "pre", "eof")] + [
                (name, c.c_long) for name in ("lw", "w", "nw", "center")] + [
                (name, c.c_int64) for name in ("granule", "sequence", "glue", "time", "floor", "residue")] + [
                ("backend", c.c_void_p)]


class Limits(c.Structure):
    _fields_ = [("packet", c.c_size_t), ("books", c.c_uint32),
                ("entries", c.c_uint32), ("values", c.c_uint32),
                ("book", BookLimits)]


class Summary(c.Structure):
    _fields_ = [(name, c.c_uint32) for name in (
        "books", "floors", "residues", "mappings", "modes", "entries", "values")] + [
        ("bits", c.c_size_t)]


def main():
    ref = c.CDLL(ctypes.util.find_library("vorbis") or "libvorbis.so.0")
    enc = c.CDLL(ctypes.util.find_library("vorbisenc") or "libvorbisenc.so.2")
    ours = c.CDLL(str(pathlib.Path(sys.argv[1]).resolve()))
    init = configure(ref, "vorbis_info_init", None, c.POINTER(Info))
    clear = configure(ref, "vorbis_info_clear", None, c.POINTER(Info))
    ci = configure(ref, "vorbis_comment_init", None, c.POINTER(Comment))
    cc = configure(ref, "vorbis_comment_clear", None, c.POINTER(Comment))
    ei = configure(enc, "vorbis_encode_init_vbr", c.c_int, c.POINTER(Info), c.c_long, c.c_long, c.c_float)
    di = configure(ref, "vorbis_analysis_init", c.c_int, c.POINTER(DSP), c.POINTER(Info))
    dc = configure(ref, "vorbis_dsp_clear", None, c.POINTER(DSP))
    headers = configure(ref, "vorbis_analysis_headerout", c.c_int, c.POINTER(DSP), c.POINTER(Comment),
                        c.POINTER(Packet), c.POINTER(Packet), c.POINTER(Packet))
    validate = configure(ours, "capy_vorbis_setup_validate", c.c_int,
                         c.c_void_p, c.c_size_t, c.c_uint16, c.POINTER(Limits),
                         c.c_void_p, c.c_void_p, c.c_size_t, c.POINTER(Summary))
    version = configure(ref, "vorbis_version_string", c.c_char_p)()
    assert version == b"Xiph.Org libVorbis 1.3.7", "Host oracle requires tested ABI"
    # The retained setup workspace is intentionally caller-owned and large.
    # Keep this oracle ABI-independent: C tests inspect exact retained fields.
    workspace, lengths = (c.c_ubyte * (256 * 1024))(), (c.c_ubyte * 65536)()
    limits = Limits(1 << 20, 256, 1 << 20, 1 << 22, BookLimits(65536, 256, 1 << 22))
    cases = 0
    for channels in (1, 2):
        for rate in (22050, 44100, 48000):
            for quality in (0.0, 0.5, 1.0):
                vi, vc, dsp = Info(), Comment(), DSP()
                init(c.byref(vi)); ci(c.byref(vc))
                try:
                    assert ei(c.byref(vi), channels, rate, quality) == 0
                    assert di(c.byref(dsp), c.byref(vi)) == 0
                    ident, comment, setup = Packet(), Packet(), Packet()
                    assert headers(c.byref(dsp), c.byref(vc), c.byref(ident), c.byref(comment), c.byref(setup)) == 0
                    summary = Summary()
                    rc = validate(setup.data, setup.size, channels, c.byref(limits), workspace,
                                  lengths, len(lengths), c.byref(summary))
                    assert rc == 0, (channels, rate, quality, setup.size, rc)
                    assert summary.books > 1 and summary.modes and summary.bits <= setup.size * 8
                    # Every byte-prefix must reject; complete setup has a final framing bit.
                    for size in range(setup.size):
                        assert validate(setup.data, size, channels, c.byref(limits), workspace,
                                        lengths, len(lengths), c.byref(summary)) < 0
                    cases += 1
                finally:
                    dc(c.byref(dsp)); cc(c.byref(vc)); clear(c.byref(vi))
    print(f"[vorbis-setup-reference] {cases} libvorbisenc setups and all truncated byte-prefixes passed")


if __name__ == "__main__":
    main()
