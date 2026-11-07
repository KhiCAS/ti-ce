"""Compile uSTL's math bridge against the CEdev selected on PATH."""

from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


@unittest.skipUnless(shutil.which("cedev-config"), "CEdev must be on PATH")
class MathHeaderTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.sdk = Path(subprocess.check_output(
            ["cedev-config", "--prefix"], text=True).strip())

    def compile(self, source, standard="c++14", language="c++"):
        with tempfile.TemporaryDirectory(prefix="khicas-math-") as tmp:
            path = Path(tmp) / ("probe.c" if language == "c" else "probe.cc")
            path.write_text(source)
            command = [str(self.sdk / "bin/ez80-clang"), "-x", language,
                       "-std=" + standard, "-DTICE", "-D__TICE__=1", "-DNDEBUG=1",
                       "-nostdinc", "-I" + str(ROOT / "ustl"),
                       "-I" + str(ROOT / "src")]
            if language == "c++":
                command += ["-fno-rtti", "-isystem", str(self.sdk / "include/c++")]
            command += ["-isystem", str(self.sdk / "include"),
                        "-fsyntax-only", str(path)]
            result = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_cpp_include_orders_and_namespace_mapping(self):
        for standard in ("c++14", "c++17", "c++23"):
            for first in ("math.h", "cmath"):
                for mapped in (False, True):
                    for dependencies_first in (False, True):
                        with self.subTest(standard=standard, first=first,
                                          mapped=mapped, dependencies_first=dependencies_first):
                            source = "#define std ustl\n" if mapped else ""
                            if dependencies_first:
                                source += "#include <cstddef>\n#include <stdlib.h>\n#include <cstdlib>\n#include <limits>\n"
                            source += f"#include <{first}>\n"
                            source += "#include <cmath>\n#include <math.h>\n"
                            if mapped:
                                source += "#ifndef std\n#error std macro lost\n#endif\n#undef std\n"
                            else:
                                source += "#ifdef std\n#error unexpected std macro\n#endif\n"
                            source += r'''
#include <cstddef>
#include <type_traits>
static_assert(std::is_same<decltype(std::sin(0.5)), double>::value, "SDK math");
static_assert(std::is_same<decltype(ustl::sin(0.5)), double>::value, "uSTL scalar math");
static_assert(std::is_same<decltype(ustl::sin(0.5f)), float>::value, "float overload");
static_assert(std::is_same<ustl::size_t, std::size_t>::value, "size_t alias");
static_assert(std::is_same<decltype(ustl::abs(0.5)), double>::value, "scalar abs overload");
static_assert(std::is_same<decltype(ustl::div(4L, 2L)), ldiv_t>::value, "div overload");
static_assert(std::is_same<decltype(ustl::malloc(1)), void*>::value, "C library alias");
static_assert(std::is_same<decltype(ustl::rand()), int>::value, "rand declaration");
#ifdef _LIBCPP_VERSION
static_assert(std::numeric_limits<double>::max_exponent > 0, "SDK limits");
#endif
#define std ustl
#include <complex>
#include <limits>
static_assert(std::numeric_limits<int>::is_integer, "uSTL limits");
#undef std
static_assert(std::is_same<decltype(ustl::sin(ustl::complex<double>())),
                           ustl::complex<double>>::value, "complex overload");
// Instantiate scalar/complex implementations as well as checking overload resolution.
ustl::complex<double> complex_math(ustl::complex<double> x) {
    return ustl::sin(x) + ustl::cos(x) + ustl::sqrt(x) + ustl::exp(x);
}
double scalar_math(double x) {
    return ustl::hypot(x, x) + ustl::pow(x, 2.0) + ustl::round(x);
}
'''
                            self.compile(source, standard)

    def test_c_math_header(self):
        self.compile("#include <stdlib.h>\n#include <math.h>\n#include <math.h>\n"
                     "double scalar_math(double x) { return sin(x) + sqrt(x); }\n",
                     standard="c11", language="c")


if __name__ == "__main__":
    unittest.main()
