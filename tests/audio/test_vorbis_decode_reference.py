"""Generate independent Ogg fixtures; compare bounded S16 output with FFmpeg."""
import array
import math
import pathlib
import struct
import subprocess
import sys
import tempfile
import wave

decoder = pathlib.Path(sys.argv[1]).resolve()
for frames, transients in ((12000, False), (48000, True)):
    with tempfile.TemporaryDirectory() as td:
        root = pathlib.Path(td)
        wav, ogg, ours, reference = [root / x for x in ("in.wav", "in.ogg", "ours.s16", "reference.s16")]
        with wave.open(str(wav), "wb") as f:
            f.setnchannels(2); f.setsampwidth(2); f.setframerate(48000)
            pcm = bytearray()
            for i in range(frames):
                left = round(12000 * math.sin(2 * math.pi * 440 * i / 48000))
                right = round(9000 * math.sin(2 * math.pi * 997 * i / 48000))
                if transients:
                    left = 30000 if i in (9600, 24000, 38400) else left
                    right = -left
                pcm.extend(struct.pack("<hh", left, right))
            f.writeframes(pcm)
        subprocess.run(["oggenc", "-Q", "-q", "4", "-o", str(ogg), str(wav)], check=True)
        subprocess.run([str(decoder), str(ogg), str(ours)], check=True)
        subprocess.run(["ffmpeg", "-v", "error", "-i", str(ogg), "-f", "s16le", str(reference)], check=True)
        def load(path):
            a = array.array("h", path.read_bytes())
            if sys.byteorder != "little": a.byteswap()
            return a
        a, b = load(ours), load(reference)
        assert len(a) == frames * 2, (len(a), frames * 2)
        count = min(len(a), len(b))
        error = sum(abs(a[i] - b[i]) for i in range(count)) / count
        peak = max(abs(a[i] - b[i]) for i in range(count))
        assert count >= (frames - 256) * 2 and error < 9 and peak < 100, (count, error, peak)
        print(f"[bounded-vorbis-reference] frames={frames} mean={error:.3f} peak={peak}")
