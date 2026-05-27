#include "mem_map.h"

#include "common.h"

#include "axi_resp.h"

#include <stdlib.h>
#include <string.h>

struct MemoryMap {
    size_t          n_units;
    size_t          cap;
    mem_map_unit_t *units;
};

int MemoryMap_ctor(MemoryMap **self) {
    DEV_ASSERT(self != NULL, "MemoryMap_ctor: self must not be NULL");
    MemoryMap *map = calloc(1, sizeof(*map));
    if (map == NULL) {
        return -ENOMEM;
    }
    *self = map;
    return 0;
}

void MemoryMap_dtor(MemoryMap **self) {
    if (self == NULL || *self == NULL) {
        return;
    }
    free((*self)->units);
    free(*self);
    *self = NULL;
}

static bool ranges_overlap(addr_t a_base, size_t a_size, addr_t b_base, size_t b_size) {
    /* [a_base, a_end) overlaps [b_base, b_end) iff a_base < b_end AND
     * b_base < a_end. Half-open intervals so adjacent ranges don't
     * collide. */
    addr_t a_end = a_base + (addr_t)a_size;
    addr_t b_end = b_base + (addr_t)b_size;
    return (a_base < b_end) && (b_base < a_end);
}

int MemoryMap_add_device(MemoryMap *self, mem_map_unit_t unit) {
    DEV_ASSERT(self != NULL, "MemoryMap_add_device: self NULL");
    DEV_ASSERT(unit.device != NULL, "MemoryMap_add_device: unit.device NULL");
    DEV_ASSERT(unit.size > 0, "MemoryMap_add_device: unit.size 0");

    for (size_t i = 0; i < self->n_units; i++) {
        if (ranges_overlap(unit.base, unit.size, self->units[i].base, self->units[i].size)) {
            return -EEXIST;
        }
    }

    if (self->n_units == self->cap) {
        size_t          new_cap   = (self->cap == 0) ? 4 : self->cap * 2;
        mem_map_unit_t *new_units = realloc(self->units, new_cap * sizeof(*new_units));
        if (new_units == NULL) {
            return -ENOMEM;
        }
        self->units = new_units;
        self->cap   = new_cap;
    }
    self->units[self->n_units++] = unit;
    return 0;
}

/* Linear scan; 6 devices, hot-path cost negligible. */
static mem_map_unit_t *find_unit(MemoryMap *self, addr_t addr, size_t width) {
    for (size_t i = 0; i < self->n_units; i++) {
        mem_map_unit_t *u     = &self->units[i];
        addr_t          u_end = u->base + (addr_t)u->size;
        if (addr >= u->base && addr + (addr_t)width <= u_end) {
            return u;
        }
    }
    return NULL;
}

axi_resp_e MemoryMap_read(MemoryMap *self, addr_t addr, size_t width, uint32_t *value_out) {
    DEV_ASSERT(self != NULL, "MemoryMap_read: self NULL");
    mem_map_unit_t *u = find_unit(self, addr, width);
    if (u == NULL) {
        /* No subordinate covers this range — fabric-level decode error. */
        if (value_out != NULL) {
            *value_out = 0;
        }
        return AXI_RESP_DECERR;
    }
    return MmioDevice_read(u->device, addr - u->base, width, value_out);
}

axi_resp_e MemoryMap_write(MemoryMap *self,
                           addr_t     addr,
                           size_t     width,
                           uint32_t   value,
                           uint8_t    strb) {
    DEV_ASSERT(self != NULL, "MemoryMap_write: self NULL");
    mem_map_unit_t *u = find_unit(self, addr, width);
    if (u == NULL) {
        return AXI_RESP_DECERR;
    }
    return MmioDevice_write(u->device, addr - u->base, width, value, strb);
}
