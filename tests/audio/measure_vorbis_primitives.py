"""Run maintained host microbenchmarks; preserve raw samples and build identity.

Usage: python3 -B tests/audio/measure_vorbis_primitives.py OUTPUT.json
No performance acceptance threshold is implied. Batch averages are NOT
individual-call tail latency. Do not compare materially different environments.
"""
import csv
import hashlib
import io
import json
import pathlib
import platform
import statistics
import subprocess
import sys
import time


def main():
    root = pathlib.Path(__file__).resolve().parents[2]
    binary = root / "build/bench_vorbis_primitives"
    files = [binary, root / "Makefile", pathlib.Path(__file__).resolve()]
    files += list((root / "src/audio").glob("vorbis*.[ch]"))
    files += [root / "tests/audio/bench_vorbis_primitives.c"]
    report = {"timestamp_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
              "platform": platform.platform(), "machine": platform.machine(),
              "compiler": subprocess.check_output(["cc", "--version"], text=True).splitlines()[0],
              "cpu": next((line.split(":", 1)[1].strip() for line in pathlib.Path("/proc/cpuinfo").read_text().splitlines() if line.startswith("model name")), "unknown"),
              "hashes": {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest() for p in files},
              "metric": "ns per call, each sample is a batch average; warm host process; no VM or PCM claim",
              "runs": [], "summary": {}}
    for run in range(3):
        output = subprocess.check_output([str(binary)], text=True)
        rows = [{"case": r["case"], "batch": int(r["batch"]), "iterations": int(r["iterations"]),
                 "wall_ns": float(r["wall_ns_per_call"]), "cpu_ns": float(r["cpu_ns_per_call"])}
                for r in csv.DictReader(io.StringIO(output))]
        assert len(rows) == 606
        report["runs"].append(rows)
    for name in sorted({r["case"] for r in report["runs"][0]}):
        values = [r for run in report["runs"] for r in run if r["case"] == name]
        summary = {}
        for metric in ("wall_ns", "cpu_ns"):
            ordered = sorted(r[metric] for r in values)
            summary[metric] = {"median": statistics.median(ordered),
                               "p95": ordered[(len(ordered)*95+99)//100-1],
                               "p99": ordered[(len(ordered)*99+99)//100-1],
                               "min": ordered[0], "max": ordered[-1], "samples": len(ordered)}
        report["summary"][name] = summary
    destination = pathlib.Path(sys.argv[1])
    with destination.open("x", encoding="utf-8") as stream:
        json.dump(report, stream, indent=2)
        stream.write("\n")
    print(json.dumps(report["summary"], indent=2))


if __name__ == "__main__":
    main()
