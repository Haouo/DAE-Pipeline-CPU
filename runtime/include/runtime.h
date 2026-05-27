/* Convenience umbrella header — pulls in every public runtime surface.
 *
 * Pedagogical note: the split below mirrors the HAL ⊂ Runtime layering pillar
 * (CLAUDE.md §5). HAL headers describe hardware-dependent surface; stdlib
 * headers describe the software stack built on top of that surface.
 */
#ifndef RUNTIME_RUNTIME_H
#define RUNTIME_RUNTIME_H

#include "hal/halt.h"
#include "hal/uart.h"

#include "stdlib/string.h"
#include "stdlib/stdio.h"
#include "stdlib/stdlib.h"
#include "stdlib/assert.h"

#endif /* RUNTIME_RUNTIME_H */
