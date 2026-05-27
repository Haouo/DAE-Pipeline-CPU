#ifndef DEVICES_AXI_RESP_H
#define DEVICES_AXI_RESP_H

/* AXI4-Lite response codes (xRESP / BRESP / RRESP), 2-bit encoding.
 *
 * Used by every bus-access API on the C side — MmioDevice callbacks,
 * MemoryMap dispatchers, and DPI subordinate wrappers. The integer values
 * match the AXI wire encoding verbatim so the DPI bridge is a pass-through
 * cast with no translation table.
 *
 * EXOKAY (2'b01) is defined for completeness but is forbidden by the
 * AXI4-Lite specification — exclusive access is not supported.
 *
 * See plans/snake-soc.md §2.5 for the full cross-layer contract.
 */

typedef enum {
    AXI_RESP_OKAY   = 0, /* 2'b00 — normal access success */
    AXI_RESP_EXOKAY = 1, /* 2'b01 — forbidden in AXI4-Lite */
    AXI_RESP_SLVERR = 2, /* 2'b10 — subordinate-level error */
    AXI_RESP_DECERR = 3, /* 2'b11 — decode error; fabric-only */
} axi_resp_e;

/* Human-readable name for log/diagnostic messages. */
static inline const char *axi_resp_name(axi_resp_e r) {
    switch (r) {
    case AXI_RESP_OKAY: return "OKAY";
    case AXI_RESP_EXOKAY: return "EXOKAY";
    case AXI_RESP_SLVERR: return "SLVERR";
    case AXI_RESP_DECERR: return "DECERR";
    default: return "?";
    }
}

#endif /* DEVICES_AXI_RESP_H */
