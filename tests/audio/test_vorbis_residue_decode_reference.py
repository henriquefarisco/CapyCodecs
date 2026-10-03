"""Independent randomized oracle for the complete residue pass scheduler."""
import ctypes as c
import pathlib
import random
import sys


class Bits(c.Structure):
    _fields_ = [("data", c.POINTER(c.c_ubyte)), ("bit_count", c.c_size_t),
                ("position", c.c_size_t), ("error", c.c_int)]


class Book(c.Structure):
    _fields_ = [(name, c.c_uint32) for name in
                ("entries", "dimensions", "used_entries", "lookup_type",
                 "lookup_values", "minimum_raw", "delta_raw", "value_bits", "sequence")] + [
                ("lookup_bit_offset", c.c_size_t)]


class Node(c.Structure):
    _fields_ = [("child", c.c_uint32 * 2), ("symbol", c.c_uint32)]


class Tree(c.Structure):
    _fields_ = [("nodes", c.POINTER(Node)), ("count", c.c_uint32),
                ("entries", c.c_uint32)]


class Config(c.Structure):
    _fields_ = [("begin", c.c_uint32), ("end", c.c_uint32),
                ("partition_size", c.c_uint32), ("type", c.c_ubyte),
                ("classifications", c.c_ubyte), ("classbook", c.c_ubyte),
                ("cascade", c.c_ubyte * 64), ("books", (c.c_int16 * 8) * 64)]


class Result(c.Structure):
    _fields_ = [("partitions", c.c_uint32), ("vectors", c.c_uint32),
                ("exhausted", c.c_ubyte)]


def add_partition(values, start, size, kind):
    if kind == 0:
        step = size // 2
        for i in range(step):
            values[start + i] += 1.0
            values[start + i + step] += 2.0
        return step
    for i in range(size):
        values[start + i] += (1.0, 2.0)[i & 1]
    return (size + 1) // 2


def check_multiclass(build, decode, rng):
    """Encode full classwords independently, including final short groups."""
    checked = scalars = 0
    covered = set()
    for _ in range(1000):
        kind = rng.randrange(3)
        channels = rng.randrange(1, 5)
        bins = rng.randrange(1, 34)
        dimensions = rng.randrange(1, 5)
        radix = rng.choice((2, 4))
        entries = radix ** dimensions
        code_bits = entries.bit_length() - 1
        nodes = (Node * (2 * entries - 1))()
        vector_nodes = (Node * 3)()
        trees = (Tree * 2)()
        lengths = (c.c_ubyte * entries)(*([code_bits] * entries))
        assert build(lengths, entries, nodes, len(nodes), c.byref(trees[0])) == 0
        vector_lengths = (c.c_ubyte * 2)(1, 1)
        assert build(vector_lengths, 2, vector_nodes, 3, c.byref(trees[1])) == 0
        books = (Book * 2)()
        books[0].entries = entries
        books[0].dimensions = dimensions
        books[1].entries = books[1].dimensions = 2
        books[1].lookup_type = 2
        books[1].lookup_values = 4
        books[1].value_bits = 4
        books[1].delta_raw = 0x60100000
        setup = (c.c_ubyte * 2)(0x21, 0x43)
        domain = bins * channels if kind == 2 else bins
        begin = rng.randrange(domain)
        end = rng.randrange(begin + 1, domain + 1)
        size = rng.randrange(1, end - begin + 1)
        partitions = (end - begin) // size
        cfg = Config(begin, end, size, kind, radix, 0)
        for row in cfg.books:
            for i in range(8): row[i] = -1
        for classification in range(radix):
            for stage in (0, 1, 3):
                if rng.randrange(2):
                    cfg.cascade[classification] |= 1 << stage
                    cfg.books[classification][stage] = 1
        skip = (c.c_ubyte * channels)(*(rng.randrange(2) for _ in range(channels)))
        if all(skip): skip[rng.randrange(channels)] = 0
        active = [channel for channel in range(channels) if not skip[channel]]
        class_channels = [0] if kind == 2 else active
        expected_classes = {channel: [] for channel in class_channels}
        total = channels * bins
        initial = [float(rng.randrange(-4, 5)) for _ in range(total)]
        expected = initial[:]
        if kind == 2:
            expected = [initial[(i % channels) * bins + i // channels]
                        for i in range(total)]
        stream = []
        vectors = decoded_partitions = 0
        for stage in range(8):
            for at in range(0, partitions, dimensions):
                count = min(dimensions, partitions - at)
                if stage == 0:
                    for channel in class_channels:
                        word = rng.randrange(entries)
                        # Equal-length canonical Huffman words emit MSB first;
                        # Ogg packs these bits into bytes LSB first below.
                        stream.extend((word >> shift) & 1
                                      for shift in range(code_bits - 1, -1, -1))
                        full_word = [(word // radix ** shift) % radix
                                     for shift in range(dimensions - 1, -1, -1)]
                        expected_classes[channel].extend(full_word[:count])
                for offset in range(count):
                    partition = at + offset
                    for channel in class_channels:
                        classification = expected_classes[channel][partition]
                        if cfg.books[classification][stage] < 0:
                            continue
                        start = begin + partition * size
                        if kind != 2:
                            start += channel * bins
                        count_vectors = add_partition(expected, start, size,
                                                      1 if kind == 2 else kind)
                        stream.extend([0] * count_vectors)
                        vectors += count_vectors
                        decoded_partitions += 1
        packed = bytearray(max(1, (len(stream) + 7) // 8))
        for index, bit in enumerate(stream):
            packed[index // 8] |= bit << (index % 8)
        audio = (c.c_ubyte * len(packed)).from_buffer_copy(packed)
        if kind == 2:
            flat = expected
            expected = initial[:]
            for i, value in enumerate(flat):
                expected[(i % channels) * bins + i // channels] = value
        out = (c.c_float * total)(*initial)
        scratch = (c.c_float * (total + 2 * size))()
        class_count = len(class_channels) * partitions
        guarded_classes = (c.c_ubyte * (class_count + 2))(*([255] * (class_count + 2)))
        bits = Bits(audio, len(stream), 0, 0)
        result = Result()
        rc = decode(c.byref(cfg), books, trees, 2, setup, 2, c.byref(bits), channels,
                    skip, bins, max(1, vectors), 1000.0, out, total, scratch,
                    len(scratch), c.byref(guarded_classes, 1), class_count,
                    c.byref(result))
        context = (kind, channels, bins, begin, end, size, dimensions, radix)
        assert rc == 0, (context, rc)
        assert list(out) == expected, context
        assert list(guarded_classes)[1:-1] == [
            value for channel in class_channels for value in expected_classes[channel]], context
        assert guarded_classes[0] == guarded_classes[-1] == 255, context
        assert result.vectors == vectors and result.partitions == decoded_partitions, context
        assert not result.exhausted and bits.position == len(stream), context
        covered.add((kind, bool(partitions % dimensions)))
        checked += 1
        scalars += total
    assert len(covered) == 6
    return checked, scalars


def main():
    ours = c.CDLL(str(pathlib.Path(sys.argv[1]).resolve()))
    build = ours.capy_vorbis_huffman_build
    build.argtypes = [c.POINTER(c.c_ubyte), c.c_uint32, c.POINTER(Node),
                      c.c_size_t, c.POINTER(Tree)]
    build.restype = c.c_int
    decode = ours.capy_vorbis_residue_decode
    decode.argtypes = [c.POINTER(Config), c.POINTER(Book), c.POINTER(Tree), c.c_size_t,
        c.c_void_p, c.c_size_t, c.POINTER(Bits), c.c_uint, c.c_void_p, c.c_size_t,
        c.c_uint32, c.c_float, c.c_void_p, c.c_size_t, c.c_void_p, c.c_size_t,
        c.c_void_p, c.c_size_t, c.POINTER(Result)]
    decode.restype = c.c_int

    lengths = (c.c_ubyte * 2)(1, 1)
    storage = ((Node * 3) * 2)()
    trees = (Tree * 2)()
    assert build(lengths, 2, storage[0], 3, c.byref(trees[0])) == 0
    assert build(lengths, 2, storage[1], 3, c.byref(trees[1])) == 0
    books = (Book * 2)()
    books[0].entries = books[1].entries = 2
    books[0].dimensions = books[1].dimensions = 2
    books[1].lookup_type = 2
    books[1].lookup_values = 4
    books[1].value_bits = 4
    books[1].delta_raw = 0x60100000
    setup = (c.c_ubyte * 2)(0x21, 0x43)
    audio = (c.c_ubyte * 8192)()
    rng = random.Random(0xCA9E510)
    checked = scalars = 0

    for _ in range(2000):
        kind = rng.randrange(3)
        channels = rng.randrange(1, 9)
        bins = rng.randrange(1, 65)
        domain = bins * channels if kind == 2 else bins
        begin = rng.randrange(domain + 1)
        size = rng.randrange(1, domain + 1)
        end = rng.randrange(begin, domain + 1)
        cfg = Config(begin, end, size, kind, 1, 0)
        for row in cfg.books:
            for i in range(8): row[i] = -1
        cfg.cascade[0] = 1
        cfg.books[0][0] = 1
        skip = (c.c_ubyte * channels)(*(rng.randrange(2) for _ in range(channels)))
        if all(skip): skip[rng.randrange(channels)] = 0
        total = channels * bins
        initial = [float(rng.randrange(-4, 5)) for _ in range(total)]
        expected = initial[:]
        partitions = (end - begin) // size
        active = sum(not value for value in skip)
        class_decodes = (partitions + 1) // 2
        vectors = 0
        decoded_partitions = 0
        if kind == 2:
            flat = [expected[(i % channels) * bins + i // channels] for i in range(total)]
            for partition in range(partitions):
                vectors += add_partition(flat, begin + partition * size, size, 1)
                decoded_partitions += 1
            for i, value in enumerate(flat):
                expected[(i % channels) * bins + i // channels] = value
            bit_count = class_decodes + vectors
        else:
            for channel in range(channels):
                if skip[channel]: continue
                for partition in range(partitions):
                    vectors += add_partition(expected, channel * bins + begin + partition * size,
                                             size, kind)
                    decoded_partitions += 1
            bit_count = active * class_decodes + vectors
        out = (c.c_float * total)(*initial)
        scratch = (c.c_float * (total + 2 * size))()
        classes = (c.c_ubyte * max(1, active * max(1, partitions)))()
        bits = Bits(audio, bit_count, 0, 0)
        result = Result()
        rc = decode(c.byref(cfg), books, trees, 2, setup, 2, c.byref(bits), channels,
                    skip, bins, max(1, vectors), 1000.0, out, total, scratch,
                    len(scratch), classes, len(classes), c.byref(result))
        assert rc == 0, (kind, channels, bins, begin, end, size, rc)
        assert list(out) == expected
        assert result.vectors == vectors and result.partitions == decoded_partitions
        assert not result.exhausted and bits.position == bit_count
        checked += 1
        scalars += total
    multi_checked, multi_scalars = check_multiclass(build, decode, rng)
    print(f"[vorbis-residue-decode-reference] {checked} single-class + "
          f"{multi_checked} multiclass schedules / {scalars + multi_scalars} scalars matched")


if __name__ == "__main__":
    main()
