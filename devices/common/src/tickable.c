#include "tickable.h"

#include "common.h"

/* Compile-time ABI sanity — see mmio_device.c for the rationale. */
_Static_assert(sizeof(Tickable) == sizeof(struct TickableVtbl const *),
               "Tickable must be exactly one vtable pointer; "
               "do not add data members to the trait struct");

static void default_tick(Tickable *self) {
    (void)self;
    DEV_PANIC("Tickable::tick called on an object whose vtable was not "
              "overridden — concrete ctor must set self->vtbl");
}

void Tickable_ctor(Tickable *self) {
    DEV_ASSERT(self != NULL, "Tickable_ctor: self must not be NULL");
    static struct TickableVtbl const default_vtbl = {
        .tick = &default_tick,
    };
    self->vtbl = &default_vtbl;
}

void Tickable_tick(Tickable *self) {
    DEV_ASSERT(self != NULL && self->vtbl != NULL, "Tickable_tick: self or vtbl is NULL");
    self->vtbl->tick(self);
}
