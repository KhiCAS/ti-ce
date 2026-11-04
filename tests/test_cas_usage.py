#!/usr/bin/env python3
"""Exercise 2D editor entry and graph menus in headless CEmu.

Use a French app and an application-signature-disabled ROM with no other apps,
or an equivalent clean CEmu state. ROM inputs are copied and existing apps
(including language packs) removed to satisfy the first-slot placement. This installs the supplied app, draws
plot(sin(x)), and checks that the curve and both axes return within one second
of releasing Clear. It also checks that Clear is consumed while held and that
holding an arrow still moves the trace cursor repeatedly. Each scenario uses
an independent saved graph state.

Example:
  python3 tests/test_cas_usage.py --runner /path/to/cemu-headless \
    --app bin/_KhiCAS.8ek --rom /path/to/no_app_verification.rom --out /tmp/cancel

--graph-image replays an already installed app's graph state instead; this is
useful for checking a frozen failing control. The supplied runner must support
the run/key/keys/send-file/screenshot/save-state headless protocol.
"""

import argparse
import hashlib
import json
from pathlib import Path
from benchmark_cas import prepare_rom
import struct
import subprocess


class CEmu:
    def __init__(self, runner, source, image, out):
        self.out = out
        out.mkdir(parents=True, exist_ok=True)
        self.log = (out / "protocol.log").open("w")
        self.stderr = (out / "stderr.log").open("w")
        self.proc = subprocess.Popen(
            [str(runner), "--image" if image else "--rom", str(source)],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=self.stderr,
            text=True, bufsize=1,
        )
        self.ready = self.proc.stdout.readline().strip()
        self.log.write(self.ready + "\n")
        if not self.ready.startswith("CEMU_HEADLESS_READY"):
            self.close()
            raise RuntimeError(self.ready)

    def command(self, command):
        self.proc.stdin.write(command + "\n")
        self.proc.stdin.flush()
        reply = self.proc.stdout.readline().strip()
        self.log.write(command + "\n" + reply + "\n")
        self.log.flush()
        if not reply.startswith("OK"):
            raise RuntimeError(f"{command}: {reply}")
        return reply

    def snapshot(self, name):
        path = self.out / f"{name}.bmp"
        self.command(f"screenshot {path}")
        return read_bmp(path)

    def close(self):
        if self.proc.poll() is None:
            try:
                self.command("quit")
                self.proc.wait(timeout=10)
            except (BrokenPipeError, RuntimeError, subprocess.TimeoutExpired):
                self.proc.kill()
                self.proc.wait()
        self.log.close()
        self.stderr.close()

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.close()


def read_bmp(path):
    data = path.read_bytes()
    width, height = struct.unpack_from("<ii", data, 18)
    depth, compression = struct.unpack_from("<HI", data, 28)
    if data[:2] != b"BM" or (width, height, depth, compression) != (320, 240, 24, 0):
        raise ValueError(f"Expected 320x240 uncompressed 24-bit BMP: {path}")
    offset = struct.unpack_from("<I", data, 10)[0]
    stride = (width * 3 + 3) & ~3
    return [
        [tuple(data[offset + (height - y - 1) * stride + x * 3 + 2 - c]
               for c in range(3)) for x in range(width)]
        for y in range(height)
    ]


def graph_metrics(pixels):
    # Exclude the status bar, bottom labels and graph softkeys. A stalled
    # redraw contains white here; sparse edge labels must not count as axes.
    rows = pixels[18:220]
    green = lambda p: p[1] > 150 and p[0] < 80 and p[2] < 80
    red = lambda p: p[0] > 150 and p[1] < 80 and p[2] < 80
    return {
        "horizontal_axis": max(sum(green(p) for p in row) for row in rows),
        "vertical_axis": max(sum(red(row[x]) for row in rows) for x in range(320)),
        "curve_pixels": sum(max(p) < 80 for row in rows for p in row),
    }


def require_graph(pixels, name):
    metrics = graph_metrics(pixels)
    if (metrics["horizontal_axis"] < 100 or metrics["vertical_axis"] < 50
            or metrics["curve_pixels"] < 200):
        raise AssertionError(f"{name}: incomplete graph after Clear release: {metrics}")
    return metrics


def graph_body(pixels):
    return pixels[18:220]


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--runner", type=Path, required=True)
    parser.add_argument("--app", type=Path)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--rom", type=Path)
    source.add_argument("--image", type=Path, help="clean state without installed apps")
    source.add_argument("--graph-image", type=Path, help="existing plot(sin(x)) graph state")
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--holds", type=int, nargs="+", default=[80, 200, 1000])
    parser.add_argument("--after-release-ms", type=int, default=1000)
    args = parser.parse_args()
    if not args.graph_image and not args.app:
        parser.error("--app is required when installing from --rom or --image")
    if any(hold <= 0 for hold in args.holds) or args.after_release_ms <= 0:
        parser.error("hold and redraw durations must be positive")
    args.out = args.out.resolve()
    args.runner = args.runner.resolve()
    source_path = (args.graph_image or args.image or args.rom).resolve()
    results = {"source": str(source_path), "runner": str(args.runner), "scenarios": []}
    if args.app:
        args.app = args.app.resolve()
        results["app"] = str(args.app)
        results["app_sha256"] = hashlib.sha256(args.app.read_bytes()).hexdigest()
    args.out.mkdir(parents=True, exist_ok=True)
    if args.rom:
        prepared = args.out / "benchmark-input.rom"
        results["removed_apps"] = prepare_rom(source_path, prepared)
        source_path = prepared
    graph_state = source_path if args.graph_image else args.out / "prepare" / "graph.ce"
    try:
        with CEmu(args.runner, source_path, bool(args.image or args.graph_image), args.out / "prepare") as emu:
            if not args.graph_image:
                emu.command("run 3000")
                emu.command(f"send-file auto {args.app}")
                emu.command("run 3000")
                if "USB transfer(s) finished" not in (emu.out / "stderr.log").read_text():
                    raise AssertionError("Application transfer did not finish; do not test a queued/stale app")
                emu.command("keys clear,delay:300,2nd,delay:200,cos,delay:1000,2,delay:2000,enter,delay:1500")
                emu.snapshot("console")
                emu.command(f"save-state {emu.out / 'console.ce'}")
                # Type plot(sin(x)) with the French KhiCAS physical key map.
                plot_keys = ["2nd", "alpha", "8", ")", "7", "4", "alpha", "(",
                             "2nd", "alpha", "ln", "^2", "log", "alpha", "(",
                             "xton", ")", ")"]
                emu.command("keys " + ",delay:1000,".join(plot_keys) + ",delay:1000,enter,delay:1000,enter,delay:12000")
                emu.command(f"save-state {graph_state}")
            baseline = emu.snapshot("graph")
            results["baseline"] = require_graph(baseline, "initial plot")
        for menu in ("trace", "window", "zoom"):
            for hold in args.holds:
                name = f"{menu}-clear-{hold}ms"
                with CEmu(args.runner, graph_state, True, args.out / name) as emu:
                    emu.command(f"key {menu}")
                    emu.command("run 500")
                    popup = emu.snapshot("popup")
                    if graph_body(popup) == graph_body(baseline):
                        raise AssertionError(f"{name}: menu did not open")
                    emu.command(f"save-state {emu.out / 'popup.ce'}")
                    emu.command(f"key clear {hold}")
                    emu.command(f"run {args.after_release_ms}")
                    redrawn = emu.snapshot("after-clear")
                    metrics = require_graph(redrawn, name)
                    if graph_body(redrawn) != graph_body(baseline):
                        raise AssertionError(f"{name}: axes/curve differ from the pre-menu graph")
                    # A second menu proves that the event loop is responsive.
                    emu.command("key trace")
                    emu.command("run 500")
                    reopened = emu.snapshot("reopened")
                    if graph_body(reopened) == graph_body(redrawn):
                        raise AssertionError(f"{name}: event loop did not accept a new menu key")
                    results["scenarios"].append({"menu": menu, "clear_hold_ms": hold,
                                                 "after_release_ms": args.after_release_ms,
                                                 "metrics": metrics, "passed": True})
                    print(f"PASS {name}: axes, curve and event loop restored", flush=True)
        with CEmu(args.runner, graph_state, True, args.out / "clear-held") as emu:
            emu.command("key trace")
            emu.command("run 500")
            popup = emu.snapshot("popup")
            # Explicit key-down/up lets us inspect the menu during the hold.
            # The keys protocol advances 50 ms after each down/up event.
            emu.command("keys down:clear,delay:950")
            held = emu.snapshot("still-held")
            if graph_body(held) != graph_body(popup):
                raise AssertionError("Clear returned to drawing before physical key release")
            emu.command("keys up:clear,delay:950")
            metrics = require_graph(emu.snapshot("after-release"), "held Clear")
            results["scenarios"].append({"clear_held_consumed": True, "clear_hold_ms": 1000,
                                         "after_release_ms": 1000, "metrics": metrics,
                                         "passed": True})
        cursor_status = []
        for hold in (80, 1000):
            with CEmu(args.runner, graph_state, True, args.out / f"arrow-{hold}ms") as emu:
                emu.command(f"key right {hold}")
                emu.command("run 500")
                cursor = emu.snapshot("cursor")
                require_graph(cursor, f"right held {hold} ms")
                # The x/y readout occupies the left side of the status bar;
                # omit the clock and battery icons, which may change on idle.
                cursor_status.append([row[:130] for row in cursor[:16]])
        if cursor_status[0] == cursor_status[1]:
            raise AssertionError("Held Right did not move the trace cursor beyond a single press")
        results["scenarios"].append({"arrow_repeat_preserved": True, "passed": True})
        results["passed"] = True
    except Exception as error:
        results["passed"] = False
        results["error"] = str(error)
        raise
    finally:
        (args.out / "results.json").write_text(json.dumps(results, indent=2) + "\n")


if __name__ == "__main__":
    main()
