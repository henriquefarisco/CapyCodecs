"""Independent bitstream oracle for retained Vorbis mapping type 0 state."""

import ctypes as c
import pathlib
import random
import sys


class Bits(c.Structure):
    _fields_ = [("data", c.POINTER(c.c_uint8)),
                ("bit_count", c.c_size_t), ("position", c.c_size_t),
                ("error", c.c_int)]


class Mapping(c.Structure):
    _fields_ = [("coupling_steps", c.c_uint16), ("submaps", c.c_uint8),
                ("magnitude", c.c_uint8 * 256), ("angle", c.c_uint8 * 256),
                ("mux", c.c_uint8 * 8), ("floor", c.c_uint8 * 16),
                ("residue", c.c_uint8 * 16)]


def encode(fields):
    packed = []

    def put(value, width):
        packed.extend((value >> bit) & 1 for bit in range(width))

    put(0, 16)
    put(fields["submaps"] > 1, 1)
    if fields["submaps"] > 1:
        put(fields["submaps"] - 1, 4)
    put(bool(fields["magnitude"]), 1)
    if fields["magnitude"]:
        put(len(fields["magnitude"]) - 1, 8)
        width = (fields["channels"] - 1).bit_length()
        for magnitude, angle in zip(fields["magnitude"], fields["angle"]):
            put(magnitude, width)
            put(angle, width)
    put(0, 2)
    if fields["submaps"] > 1:
        for mux in fields["mux"]:
            put(mux, 4)
    for submap in range(fields["submaps"]):
        put(fields["time"][submap], 8)
        put(fields["floor"][submap], 8)
        put(fields["residue"][submap], 8)
    output = bytearray((len(packed) + 7) // 8)
    for index, bit in enumerate(packed):
        output[index // 8] |= bit << (index % 8)
    return output, len(packed)


def main():
    library = c.CDLL(str(pathlib.Path(sys.argv[1]).resolve()))
    init = library.capy_vorbis_bits_init
    init.argtypes = [c.POINTER(Bits), c.POINTER(c.c_uint8), c.c_size_t, c.c_size_t]
    init.restype = c.c_int
    read = library.capy_vorbis_mapping_read
    read.argtypes = [c.POINTER(Bits), c.c_uint, c.c_uint, c.c_uint,
                     c.POINTER(Mapping)]
    read.restype = c.c_int
    rng = random.Random(0x4D415000)
    cases = 0
    truncated = 0
    for case in range(5000):
        channels = rng.randrange(1, 9)
        floors = rng.randrange(1, 65)
        residues = rng.randrange(1, 65)
        submaps = rng.randrange(1, 17)
        step_count = 0 if channels == 1 else rng.randrange(0, 17)
        magnitude, angle = [], []
        for _ in range(step_count):
            mag = rng.randrange(channels)
            ang = rng.randrange(channels - 1)
            if ang >= mag:
                ang += 1
            magnitude.append(mag)
            angle.append(ang)
        fields = {
            "channels": channels,
            "submaps": submaps,
            "magnitude": magnitude,
            "angle": angle,
            "mux": [rng.randrange(submaps) for _ in range(channels)],
            "time": [rng.randrange(256) for _ in range(submaps)],
            "floor": [rng.randrange(floors) for _ in range(submaps)],
            "residue": [rng.randrange(residues) for _ in range(submaps)],
        }
        payload, bit_count = encode(fields)
        data = (c.c_uint8 * len(payload)).from_buffer_copy(payload)
        bits, mapping = Bits(), Mapping()
        assert init(c.byref(bits), data, len(payload), len(payload)) == 0
        bits.bit_count = bit_count
        assert read(c.byref(bits), channels, floors, residues, c.byref(mapping)) == 0
        assert bits.position == bit_count
        assert mapping.submaps == submaps and mapping.coupling_steps == step_count
        assert list(mapping.magnitude[:step_count]) == magnitude
        assert list(mapping.angle[:step_count]) == angle
        assert list(mapping.mux[:channels]) == (fields["mux"] if submaps > 1 else [0] * channels)
        assert list(mapping.floor[:submaps]) == fields["floor"]
        assert list(mapping.residue[:submaps]) == fields["residue"]
        if case < 50:
            for cut in range(bit_count):
                assert init(c.byref(bits), data, len(payload), len(payload)) == 0
                bits.bit_count = cut
                c.memset(c.byref(mapping), 0xA5, c.sizeof(mapping))
                assert read(c.byref(bits), channels, floors, residues, c.byref(mapping)) < 0
                assert bytes(mapping) == bytes(c.sizeof(mapping))
                truncated += 1
        cases += 1
    print(f"[vorbis-mapping-reference] {cases} mappings and {truncated} bit prefixes passed")


if __name__ == "__main__":
    main()
