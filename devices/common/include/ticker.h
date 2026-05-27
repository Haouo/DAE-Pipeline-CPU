#ifndef DEVICES_TICKER_H
#define DEVICES_TICKER_H

/* Ticker — registry of Tickable trait views.
 *
 * Non-owning. Tick order is registration order. By convention, register
 * peripheral tickables (CLINT, UART, IRQ-AGG, Lab 7+ DRAM) before Core
 * so Core observes post-tick state during fetch. Within the peripheral
 * set, register the producer of an edge wire (UART RX-valid) before its
 * consumer (IRQ-AGG) so the consumer's tick observes the up-to-date wire.
 *
 * See plans/snake-soc.md §3.3.
 */

#include "tickable.h"

typedef struct Ticker Ticker;

int  Ticker_ctor(Ticker **self);
void Ticker_dtor(Ticker **self); /* does NOT dtor registered tickables */

/* Register a tickable view. Returns 0 / -ENOMEM. */
int Ticker_add(Ticker *self, Tickable *t);

/* Call Tickable_tick on every registered view in registration order. */
void Ticker_tick_all(Ticker *self);

#endif /* DEVICES_TICKER_H */
