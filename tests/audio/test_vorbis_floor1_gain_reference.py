"""Pin the normative Xiph floor1 table and compare the spectral dot product."""

import ctypes as c
import hashlib
import pathlib
import random
import struct
import sys


XIPH_TABLE_SHA256 = "3d48051a5d8e88104ebe447e328147ebbffa53e8b8e907878b77739ac5dc4b7d"


def f32(value):
    return c.c_float(value).value


def main():
    library = c.CDLL(str(pathlib.Path(sys.argv[1]).resolve()))
    apply_gain = library.capy_vorbis_floor1_apply_gain
    apply_gain.argtypes = [
        c.POINTER(c.c_uint8), c.c_size_t, c.c_float,
        c.POINTER(c.c_float), c.c_size_t,
        c.POINTER(c.c_float), c.c_size_t,
    ]
    apply_gain.restype = c.c_int

    indices = (c.c_uint8 * 256)(*range(256))
    spectrum = (c.c_float * 256)(*([1.0] * 256))
    scratch = (c.c_float * 256)()
    assert apply_gain(indices, 256, c.c_float(1.0), spectrum, 256,
                      scratch, 256) == 0
    table_bytes = struct.pack("<256f", *spectrum)
    assert hashlib.sha256(table_bytes).hexdigest() == XIPH_TABLE_SHA256
    gains = list(spectrum)

    rng = random.Random(0xF1001DB)
    compared = 0
    for _ in range(2000):
        count = rng.randrange(1, 129)
        selected = [rng.randrange(256) for __ in range(count)]
        values = [f32(rng.uniform(-100000.0, 100000.0)) for __ in range(count)]
        expected = [f32(values[i] * gains[selected[i]]) for i in range(count)]
        selected_array = (c.c_uint8 * count)(*selected)
        values_array = (c.c_float * count)(*values)
        work = (c.c_float * count)()
        assert apply_gain(selected_array, count, c.c_float(100000.0),
                          values_array, count, work, count) == 0
        assert struct.pack(f"<{count}f", *values_array) == struct.pack(
            f"<{count}f", *expected)
        compared += count
    print(f"[vorbis-floor1-gain-reference] Xiph table hash and {compared} products matched")


if __name__ == "__main__":
    main()
