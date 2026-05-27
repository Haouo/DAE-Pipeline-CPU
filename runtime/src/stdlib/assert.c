/* Translation unit kept so the RUNTIME_ASSERT macro has an obvious home in
 * the build graph. The macro itself lives in <stdlib/assert.h>; this file
 * exists so that adding a non-inline assertion helper later does not require
 * touching CMakeLists or the build skeleton. */
#include "stdlib/assert.h"
