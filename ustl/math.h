#pragma once

#if defined(TICE) && defined(__cplusplus)
// giac maps std to ustl. Keep the CE math/type_traits headers in their actual
// namespace so their generic swap does not collide with uSTL's implementation.
#pragma push_macro("std")
#undef std
#include_next <math.h>

namespace ustl {
using ::rand;
// Keep scalar and complex overloads in the same overload set.
using std::acos;
using std::asin;
using std::atan;
using std::atan2;
using std::ceil;
using std::cos;
using std::cosh;
using std::exp;
using std::fabs;
using std::floor;
using std::fmod;
using std::frexp;
using std::hypot;
using std::ldexp;
using std::log;
using std::log10;
using std::pow;
using std::round;
using std::sin;
using std::sinh;
using std::sqrt;
using std::tan;
using std::tanh;
using std::trunc;
}
#pragma pop_macro("std")
#else
#include_next <math.h>
#endif
