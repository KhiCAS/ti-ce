"""Exercise global LTO controls and compiler capability detection."""

import unittest

from build_fixture import BuildFixture


class LTOTuningTests(BuildFixture):
    def test_global_flags_and_incremental_rebuilds(self):
        self.make()
        self.assert_callee_calls(2)
        tracked = [self.path / name for name in (
            "obj/.khicas-build-config", "obj/src/caller.c.bc",
            "obj/src/callee.cc.bc", "obj/lto.bc", "obj/lto.s",
        )]
        times = [p.stat().st_mtime_ns for p in tracked]
        self.make()
        self.assertEqual([p.stat().st_mtime_ns for p in tracked], times)

        self.make("LTO_INLINE_THRESHOLD=1000")
        self.assert_callee_calls(0)
        self.make("LTOFLAGS=-Oz -mllvm -inline-threshold=1000")
        self.assert_callee_calls(0)
        self.make("LTO_TUNING=NO")
        self.assert_callee_calls(2)
        config = (self.path / "obj/.khicas-build-config").read_text()
        self.assertIn("-inline-threshold=-10", config)
        self.assertNotIn("-z80-internal-regargs-on-merged-ir", config)

    def test_unsupported_compiler_falls_back(self):
        compiler = self.path / "compiler-without-tuning"
        compiler.write_text("#!/bin/sh\nexit 0\n")
        compiler.chmod(0o755)
        result = self.make("-n", "CC=" + str(compiler))
        self.assertIn("-inline-threshold=-10", result.stdout)
        self.assertNotIn("-z80-machine-outliner-min-sequence-bytes", result.stdout)
        self.assertNotIn("-z80-internal-regargs-on-merged-ir", result.stdout)
        result = self.make("-n", "CC=" + str(compiler),
                           "LTO_TUNING=YES", check=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("compiler lacks", result.stdout)



if __name__ == "__main__":
    unittest.main()
