// This forwarding header can be re-entered by the SDK's <cmath>.
#if defined(TICE) && defined(__cplusplus) && !defined(KHICAS_LOADING_SDK_CMATH)
#include <cmath>
#else
#include_next <math.h>
#endif
