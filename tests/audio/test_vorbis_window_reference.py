"""Compare freestanding Vorbis window/overlap against Python math."""
import ctypes as c
import math
import pathlib
import random
import sys


class Window(c.Structure):
    _fields_ = [(name, c.c_uint16) for name in
                ("block_size", "left_start", "left_end", "right_start", "right_end")] + [
                (name, c.c_ubyte) for name in
                ("mode", "mapping", "blockflag", "previous_window", "next_window")]


def shape(n, short_n, previous, following):
    if n != short_n and not previous:
        ls, le = n // 4 - short_n // 4, n // 4 + short_n // 4
    else:
        ls, le = 0, n // 2
    if n != short_n and not following:
        rs, re = 3 * n // 4 - short_n // 4, 3 * n // 4 + short_n // 4
    else:
        rs, re = n // 2, n
    return Window(n, ls, le, rs, re, 0, 0, n != short_n, previous, following)


def slope(index, count):
    inner = math.sin((index + 0.5) / count * math.pi / 2)
    return math.sin(math.pi / 2 * inner * inner)


def f32(value):
    return c.c_float(value).value


def main():
    ours = c.CDLL(str(pathlib.Path(sys.argv[1]).resolve()))
    apply = ours.capy_vorbis_window_apply
    apply.argtypes = [c.POINTER(Window), c.c_void_p, c.c_size_t, c.c_float,
                      c.c_void_p, c.c_size_t, c.c_void_p, c.c_size_t]
    apply.restype = c.c_int
    overlap = ours.capy_vorbis_overlap_add
    overlap.argtypes = [c.c_void_p, c.c_size_t, c.c_void_p, c.c_size_t, c.c_float,
                        c.c_void_p, c.c_size_t, c.c_void_p, c.c_size_t,
                        c.POINTER(c.c_size_t)]
    overlap.restype = c.c_int
    sizes = [1 << exponent for exponent in range(6, 14)]
    checked = 0
    maximum_error = 0.0
    for n in sizes:
        short_n = min(256, n)
        for previous in (0, 1):
            for following in (0, 1):
                window = shape(n, short_n, previous, following)
                source = (c.c_float * n)(*([1.0] * n))
                output, scratch = (c.c_float * n)(), (c.c_float * n)()
                assert apply(c.byref(window), source, n, 2.0, output, n, scratch, n) == 0
                for i, actual in enumerate(output):
                    if i < window.left_start or i >= window.right_end: expected = 0.0
                    elif i < window.left_end:
                        expected = slope(i - window.left_start, window.left_end - window.left_start)
                    elif i < window.right_start: expected = 1.0
                    else:
                        expected = slope(window.right_end - i - 1,
                                         window.right_end - window.right_start)
                    error = abs(actual - f32(expected))
                    maximum_error = max(maximum_error, error)
                    assert error <= 1.2e-7, (n, previous, following, i, actual, expected)
                    checked += 1

    rng = random.Random(0x51A9E)
    overlap_samples = 0
    for previous_size in sizes:
        for current_size in sizes:
            previous_values = [f32(rng.randrange(-16, 17) / 8) for _ in range(previous_size)]
            current_values = [f32(rng.randrange(-16, 17) / 8) for _ in range(current_size)]
            previous = (c.c_float * previous_size)(*previous_values)
            current = (c.c_float * current_size)(*current_values)
            count = previous_size // 4 + current_size // 4
            output, scratch = (c.c_float * count)(), (c.c_float * count)()
            produced = c.c_size_t()
            assert overlap(previous, previous_size, current, current_size, 8.0,
                           output, count, scratch, count, c.byref(produced)) == 0
            assert produced.value == count
            for i, actual in enumerate(output):
                pi = previous_size // 2 + i
                ci = current_size // 4 - previous_size // 4 + i
                expected = (previous_values[pi] if pi < previous_size else 0.0) + \
                           (current_values[ci] if 0 <= ci < current_size // 2 else 0.0)
                assert actual == f32(expected)
                overlap_samples += 1
    print(f"[vorbis-window-reference] {checked} coefficients, {overlap_samples} overlap samples; max error {maximum_error:.3g}")


if __name__ == "__main__":
    main()
