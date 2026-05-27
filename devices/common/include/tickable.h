#ifndef DEVICES_TICKABLE_H
#define DEVICES_TICKABLE_H

/* Tickable trait — Rust-trait-style interface for modules that advance
 * simulator-internal time.
 *
 * Tickable is ORTHOGONAL to MmioDevice. Many tickables (Core, Lab 7+
 * DRAM controller) have no memory map; some MmioDevices (Boot ROM) have
 * no internal time. A concrete device that needs both (UART, CLINT,
 * IRQ-AGG in the post-TUI-pivot SoC) implements them independently by
 * embedding both trait structs and overriding both vtables.
 *
 * See plans/snake-soc.md §3.1.2 for the contract.
 */

#include "common.h"

struct TickableVtbl;

typedef struct {
    struct TickableVtbl const *vtbl;
} Tickable;

struct TickableVtbl {
    void (*tick)(Tickable *self);
};

/* Default constructor — sets vtable to a virtual stub that DEV_PANICs.
 * Concrete tickables MUST override `self->vtbl` after calling this. */
void Tickable_ctor(Tickable *self);

/* Public dispatcher — typically invoked by Ticker_tick_all. */
void Tickable_tick(Tickable *self);

/* Macros for naming concrete tick implementations.
 *
 * Pattern:
 *   DECLARE_TICKABLE_TICK(UART) { ... }
 *   ...
 *   static struct TickableVtbl const vtbl = {
 *       .tick = &SIGNATURE_TICKABLE_TICK(UART),
 *   };
 *   self->tick_super.vtbl = &vtbl;
 */
#define SIGNATURE_TICKABLE_TICK(cls) cls##_Tickable_tick
#define DECLARE_TICKABLE_TICK(cls)   void(SIGNATURE_TICKABLE_TICK(cls))(Tickable * self)

#endif /* DEVICES_TICKABLE_H */
