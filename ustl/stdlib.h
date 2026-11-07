// This forwarding header can be re-entered by the SDK's <cstdlib>.
#if defined(TICE) && defined(__cplusplus) && !defined(KHICAS_LOADING_SDK_CSTDLIB)
#include <cstdlib>
#else
#include_next <stdlib.h>
#endif
