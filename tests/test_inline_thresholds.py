"""Exercise per-source inlining through CEdev's optimization stages."""

import unittest

from build_fixture import BuildFixture


class InlineThresholdTests(BuildFixture):
    def test_lto_overrides_and_incremental_rebuilds(self):
        # No per-source override: use the global size threshold.
        self.make("INLINE_THRESHOLDS=")
        self.assert_callee_calls(2)
        default_assembly = (self.path / "obj/lto.s").read_bytes()

        # A C caller's call-site policy survives the bitcode merge.
        policy = "src/caller.c=1000"
        self.make("INLINE_THRESHOLDS=" + policy)
        self.assert_callee_calls(0)
        tracked = [self.path / name for name in (
            "obj/.khicas-build-config", "obj/src/caller.c.bc",
            "obj/src/callee.cc.bc", "obj/lto.bc", "obj/lto.s",
        )]
        times = [p.stat().st_mtime_ns for p in tracked]
        self.make("INLINE_THRESHOLDS=" + policy)
        self.assertEqual([p.stat().st_mtime_ns for p in tracked], times)

        # C++ callee policy is the fallback when the caller has no override.
        self.make("INLINE_THRESHOLDS=src/callee.cc=1000")
        self.assert_callee_calls(0)
        # An explicit call-site threshold takes precedence over the callee.
        self.make("INLINE_THRESHOLDS=src/callee.cc=1000 src/caller.c=0")
        self.assert_callee_calls(2)

        self.make("INLINE_THRESHOLDS=")
        self.assertEqual((self.path / "obj/lto.s").read_bytes(), default_assembly)

    def test_without_lto(self):
        target = "obj/src/local.c.s"
        self.make("INLINE_THRESHOLDS=src/local.c=0", "LTO=NO", target=target)
        self.assertRegex((self.path / target).read_text(), r"\bcall\s+_helper\b")
        self.make("INLINE_THRESHOLDS=src/local.c=1000", "LTO=NO", target=target)
        self.assertNotRegex((self.path / target).read_text(), r"\bcall\s+_helper\b")


if __name__ == "__main__":
    unittest.main()
