"""Specification oracle: closed-form interpolation, no reference runtime ABI.

Checks integer floor indices only, not inverse-dB gains, entropy decode or PCM.
"""
import ctypes as c
import pathlib
import random
import sys
from test_vorbis_reference import configure


class Plan(c.Structure):
    _fields_ = [("x", c.c_uint16 * 65)] + [(name, c.c_uint8 * 65)
                for name in ("low", "high", "order")] + [
                ("count", c.c_uint8), ("multiplier", c.c_uint8)]


def oracle(x, encoded, multiplier, bins):
    limit = (256, 128, 86, 64)[multiplier - 1]
    final, active = list(encoded[:2]), {0, 1}
    def interpolate(a, b, pos):
        delta = final[b] - final[a]
        offset = abs(delta) * (pos - x[a]) // (x[b] - x[a])
        return final[a] + (offset if delta >= 0 else -offset)
    for i, value in enumerate(encoded[2:], 2):
        low = max((j for j in range(i) if x[j] < x[i]), key=x.__getitem__)
        high = min((j for j in range(i) if x[j] > x[i]), key=x.__getitem__)
        prediction = interpolate(low, high, x[i])
        # Construct the inverse mapping by listing alternating negative and
        # positive residuals until one edge, then the remaining one-sided tail.
        # This intentionally differs from the production room/branch formula.
        candidates = [prediction]
        for distance in range(1, limit):
            if prediction - distance >= 0:
                candidates.append(prediction - distance)
            if prediction + distance < limit:
                candidates.append(prediction + distance)
        assert len(candidates) == limit
        final.append(candidates[value])
        if value:
            active.update((low, high, i))
    knots = sorted(active, key=x.__getitem__)
    result = []
    for a, b in zip(knots, knots[1:]):
        # Floor rendering scales endpoints BEFORE interpolation, unlike
        # prediction; use direct rational evaluation, not the C error loop.
        ya, yb = final[a] * multiplier, final[b] * multiplier
        for pos in range(x[a], min(x[b], bins)):
            offset = abs(yb - ya) * (pos - x[a]) // (x[b] - x[a])
            result.append(ya + (offset if yb >= ya else -offset))
    result.extend([final[knots[-1]] * multiplier] * max(0, bins - x[knots[-1]]))
    return result


def main():
    lib = c.CDLL(str(pathlib.Path(sys.argv[1]).resolve()))
    prepare = configure(lib, "capy_vorbis_floor1_prepare", c.c_int,
                        c.c_void_p, c.c_size_t, c.c_uint, c.POINTER(Plan))
    curve = configure(lib, "capy_vorbis_floor1_curve", c.c_int,
                      c.POINTER(Plan), c.c_void_p, c.c_size_t, c.c_size_t,
                      c.c_void_p, c.c_size_t)
    cases = bins_checked = 0
    def check(x, encoded, multiplier, bins):
        nonlocal cases, bins_checked
        p = Plan()
        xx, yy = (c.c_uint16 * len(x))(*x), (c.c_uint32 * len(x))(*encoded)
        output = (c.c_uint8 * bins)()
        assert prepare(xx, len(x), multiplier, c.byref(p)) == 0
        assert curve(c.byref(p), yy, len(x), bins, output, bins) == 0
        assert list(output) == oracle(x, encoded, multiplier, bins), (x, encoded, multiplier)
        cases += 1
        bins_checked += bins
    # Exhaust every endpoint pair, ascending and descending, including tails.
    for a in range(256):
        for b in range(256):
            check([0, 16], [a, b], 1, 19)
    rng = random.Random(0x10F10002)
    for _ in range(3000):
        extent = 1 << rng.randrange(1, 16)
        count = rng.randrange(2, min(65, extent + 1) + 1)
        x = [0, extent] + rng.sample(range(1, extent), count - 2)
        multiplier = rng.randrange(1, 5)
        limit = (256, 128, 86, 64)[multiplier - 1]
        encoded = [rng.randrange(limit) for _ in x]
        # Include inactive points subsequently reactivated as neighbors.
        for i in range(2, count, 3):
            encoded[i] = 0
        check(x, encoded, multiplier, rng.randrange(1, 513))
    for multiplier in range(1, 5):
        check([0, 32768] + list(range(63, 0, -1)), [32, 63] + [0, 1, 2] * 21,
              multiplier, 4096)
    print(f"[vorbis-floor1-reference] {cases} curves / {bins_checked} bins matched the independent specification oracle")


if __name__ == "__main__":
    main()
