"""Shared temporary-project fixture for CEdev build regression tests."""

from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


@unittest.skipUnless(shutil.which("cedev-config"), "CEdev must be on PATH")
class BuildFixture(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="khicas-inline-")
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name)
        (self.path / "app_tools").symlink_to(ROOT / "app_tools", target_is_directory=True)
        (self.path / "src").mkdir()
        (self.path / "src/caller.c").write_text(
            "extern int callee(int);\n"
            "int caller(int x) { return callee(x) + callee(x + 1); }\n"
        )
        (self.path / "src/callee.cc").write_text(
            'extern "C" int opaque(int);\n'
            'extern "C" int callee(int x) {\n'
            "  return opaque(x) + opaque(x + 1) + opaque(x + 2);\n}\n"
        )
        (self.path / "src/local.c").write_text(
            "extern int opaque(int);\n"
            "static int helper(int x) {\n"
            "  return opaque(x) + opaque(x + 1) + opaque(x + 2);\n}\n"
            "int local(int x) { return helper(x) + helper(x + 1); }\n"
        )

    def make(self, *options, target="obj/lto.s", check=True):
        result = subprocess.run(
            ["make", "-f", str(ROOT / "makefile"), target,
             *options],
            cwd=self.path, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        )
        if check:
            self.assertEqual(result.returncode, 0, result.stdout)
        return result

    def caller_assembly(self):
        asm = (self.path / "obj/lto.s").read_text()
        return asm.split("_caller:", 1)[1].split("; -- End function", 1)[0]

    def assert_callee_calls(self, count):
        self.assertEqual(len(re.findall(r"\bcall\s+_callee\b", self.caller_assembly())), count)
