#include "ticker.h"

#include "common.h"

#include <stdlib.h>

struct Ticker {
    size_t     n_tickables;
    size_t     cap;
    Tickable **tickables; /* non-owning views */
};

int Ticker_ctor(Ticker **self) {
    DEV_ASSERT(self != NULL, "Ticker_ctor: self NULL");
    Ticker *t = calloc(1, sizeof(*t));
    if (t == NULL) {
        return -ENOMEM;
    }
    *self = t;
    return 0;
}

void Ticker_dtor(Ticker **self) {
    if (self == NULL || *self == NULL) {
        return;
    }
    free((*self)->tickables);
    free(*self);
    *self = NULL;
}

int Ticker_add(Ticker *self, Tickable *t) {
    DEV_ASSERT(self != NULL, "Ticker_add: self NULL");
    DEV_ASSERT(t != NULL, "Ticker_add: t NULL");

    if (self->n_tickables == self->cap) {
        size_t     new_cap = (self->cap == 0) ? 4 : self->cap * 2;
        Tickable **new_arr = realloc(self->tickables, new_cap * sizeof(*new_arr));
        if (new_arr == NULL) {
            return -ENOMEM;
        }
        self->tickables = new_arr;
        self->cap       = new_cap;
    }
    self->tickables[self->n_tickables++] = t;
    return 0;
}

void Ticker_tick_all(Ticker *self) {
    DEV_ASSERT(self != NULL, "Ticker_tick_all: self NULL");
    for (size_t i = 0; i < self->n_tickables; i++) {
        Tickable_tick(self->tickables[i]);
    }
}
