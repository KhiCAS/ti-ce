#!/usr/bin/env python3
"""Run a BENCHMARK=YES app in CEmu; record calculator clock ticks and results."""

import argparse
import hashlib
import json
from pathlib import Path
import statistics
import subprocess


def prepare_rom(source, destination):
    """Remove existing apps from a disposable copy, including language packs.

    OS 'Reset Apps' preserves the active language pack. App trailers form a
    backwards chain ending at 0x3B0000; removing it satisfies relocate_app.py's
    first-slot contract without modifying the original ROM or OS code.
    """
    if destination.exists() and destination.samefile(source):
        raise ValueError("The prepared ROM must be a separate file")
    data = bytearray(source.read_bytes())
    end = 0x3B0000
    removed = []
    while True:
        size = int.from_bytes(data[end - 3:end], "little")
        start = end - 3 - size
        if size < 0x12A or start < 0 or data[start:start + 2] != b"\x81\x0f":
            break
        removed.append({"start": hex(start), "bytes": size + 3})
        end = start
    if removed:
        data[end:0x3B0000] = b"\xff" * (0x3B0000 - end)
    destination.write_bytes(data)
    return removed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runner", type=Path, required=True)
    parser.add_argument("--rom", type=Path, required=True)
    parser.add_argument("--app", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--reference", type=Path)
    parser.add_argument("--timeout-ms", type=int, default=300000)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    prepared_rom = args.out / "benchmark-input.rom"
    removed_apps = prepare_rom(args.rom, prepared_rom)
    log_path = args.out / "emulator.log"
    with log_path.open("w") as log:
        proc = subprocess.Popen(
            [str(args.runner.resolve()), "--rom", str(prepared_rom.resolve())],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=log,
            text=True, bufsize=1,
        )
        transcript = []

        def command(text):
            proc.stdin.write(text + "\n")
            proc.stdin.flush()
            reply = proc.stdout.readline().strip()
            transcript.append([text, reply])
            if not reply.startswith("OK"):
                raise RuntimeError(f"{text}: {reply}")

        try:
            ready = proc.stdout.readline().strip()
            if not ready.startswith("CEMU_HEADLESS_READY"):
                raise RuntimeError(ready)
            command("run 3000")
            command("send-file auto " + str(args.app.resolve()))
            command("run 3000")
            command("keys clear,delay:300,2nd,delay:200,cos,delay:1000,2,delay:2000,enter,delay:1500")
            for _ in range(0, args.timeout_ms, 1000):
                command("run 1000")
                text = log_path.read_text()
                if "KHICAS_BENCH_DONE" in text:
                    break
                if "NMI reset" in text or "Reset caused" in text:
                    raise RuntimeError("Calculator reset; see emulator.log")
            else:
                command("screenshot " + str((args.out / "timeout.bmp").resolve()))
                raise RuntimeError("Benchmark did not finish before the emulated deadline")
        finally:
            try:
                if proc.poll() is None:
                    try:
                        command("quit")
                    except (BrokenPipeError, RuntimeError):
                        pass
                proc.wait(timeout=10)
            finally:
                if proc.poll() is None:
                    proc.kill()
                    proc.wait()
                (args.out / "protocol.json").write_text(json.dumps(transcript, indent=2) + "\n")

    cases, checks = {}, {}
    frequency, expected_cases, expected_checks = None, 12, 0
    for line in log_path.read_text().splitlines():
        if line.startswith("KHICAS_BENCH_BEGIN|"):
            fields = line.split("|")
            frequency = int(fields[1])
            if len(fields) > 2:
                expected_cases, expected_checks = map(int, fields[2:4])
        if line.startswith("KHICAS_CHECK|"):
            _, number, value = line.split("|", 2)
            if number in checks or value != "0":
                raise AssertionError("Failed exact-zero identity: " + line)
            checks[number] = value
        if not line.startswith("KHICAS_BENCH|"):
            continue
        _, number, repeat, parse, evaluate, render, expr, value = line.split("|", 7)
        case = cases.setdefault(number, {"expression": expr, "samples": [], "result": value})
        if case["result"] != value:
            raise AssertionError(f"Unstable result for {expr}")
        if value.replace(" ", "") == expr.replace(" ", ""):
            raise AssertionError(f"Workload remained unevaluated: {expr}")
        case["samples"].append({"repeat": int(repeat), "parse_ticks": int(parse),
                                "eval_ticks": int(evaluate), "print_ticks": int(render)})
    if not frequency or len(cases) != expected_cases or any(
        sorted(s["repeat"] for s in c["samples"]) != [0, 1, 2, 3] for c in cases.values()
    ) or len(checks) != expected_checks:
        raise AssertionError("Incomplete benchmark samples")
    reference = json.loads(args.reference.read_text()) if args.reference else None
    for number, case in cases.items():
        measured = [s for s in case["samples"] if s["repeat"] > 0]
        for phase in ("parse", "eval", "print"):
            case[phase + "_ticks"] = statistics.median(s[phase + "_ticks"] for s in measured)
        if reference:
            expected = reference["cases"][number]
            if (case["expression"], case["result"]) != (expected["expression"], expected["result"]):
                raise AssertionError(f"Result changed for {case['expression']}")
            case["eval_ratio"] = case["eval_ticks"] / max(1, expected["eval_ticks"])
        print(f"{number:>2} {case['eval_ticks'] / frequency * 1000:9.3f} ms  {case['expression']}")
    result = {"app": str(args.app.resolve()), "sha256": hashlib.sha256(args.app.read_bytes()).hexdigest(),
              "rom_sha256": hashlib.sha256(args.rom.read_bytes()).hexdigest(),
              "prepared_rom_sha256": hashlib.sha256(prepared_rom.read_bytes()).hexdigest(),
              "removed_apps": removed_apps,
              "clock_hz": frequency, "cases": cases, "exact_zero_checks": len(checks),
              "results_match_reference": bool(reference)}
    (args.out / "results.json").write_text(json.dumps(result, indent=2) + "\n")


if __name__ == "__main__":
    main()
