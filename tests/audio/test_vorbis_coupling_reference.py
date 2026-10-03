"""Independent specification oracle for private Vorbis inverse coupling."""

import ctypes as c
import pathlib
import random
import struct
import sys


def f32(value):
    return c.c_float(value).value


def bits(value):
    return struct.pack("<f", value)


def decouple(vectors, magnitude, angle, bins):
    for step in range(len(magnitude) - 1, -1, -1):
        mag = vectors[magnitude[step]]
        ang = vectors[angle[step]]
        for index in range(bins):
            m, a = mag[index], ang[index]
            if m > 0.0:
                if a > 0.0:
                    new_m, new_a = m, m - a
                else:
                    new_m, new_a = m + a, m
            elif a > 0.0:
                new_m, new_a = m, m + a
            else:
                new_m, new_a = m - a, m
            mag[index], ang[index] = f32(new_m), f32(new_a)


def main():
    library = c.CDLL(str(pathlib.Path(sys.argv[1]).resolve()))
    inverse = library.capy_vorbis_inverse_coupling
    inverse.argtypes = [
        c.POINTER(c.c_float), c.c_size_t, c.c_size_t, c.c_size_t, c.c_size_t,
        c.POINTER(c.c_uint8), c.POINTER(c.c_uint8), c.c_size_t, c.c_float,
        c.POINTER(c.c_float), c.c_size_t,
    ]
    inverse.restype = c.c_int
    rng = random.Random(0xC0A117)
    compared = 0
    for _ in range(2000):
        channels = rng.randrange(2, 9)
        bins = rng.randrange(1, 65)
        stride = bins + rng.randrange(0, 4)
        steps = rng.randrange(1, 17)
        magnitude = []
        angle = []
        for __ in range(steps):
            mag = rng.randrange(channels)
            ang = rng.randrange(channels - 1)
            if ang >= mag:
                ang += 1
            magnitude.append(mag)
            angle.append(ang)
        reference = []
        flat = []
        for channel in range(channels):
            row = []
            for __ in range(bins):
                value = f32(rng.uniform(-8.0, 8.0))
                if rng.randrange(32) == 0:
                    value = f32(0.0)
                row.append(value)
            reference.append(row)
            flat.extend(row)
            flat.extend([f32(1234.0 + channel)] * (stride - bins))
        decouple(reference, magnitude, angle, bins)
        vector_array = (c.c_float * len(flat))(*flat)
        scratch = (c.c_float * (channels * bins))()
        mag_array = (c.c_uint8 * steps)(*magnitude)
        angle_array = (c.c_uint8 * steps)(*angle)
        rc = inverse(vector_array, len(flat), channels, stride, bins,
                     mag_array, angle_array, steps, c.c_float(1.0e30),
                     scratch, channels * bins)
        assert rc == 0
        for channel in range(channels):
            for index in range(bins):
                assert bits(vector_array[channel * stride + index]) == bits(reference[channel][index])
                compared += 1
            for index in range(bins, stride):
                assert vector_array[channel * stride + index] == f32(1234.0 + channel)
    print(f"[vorbis-coupling-reference] {compared} spectral scalars matched")


if __name__ == "__main__":
    main()
