#ifndef DEVICES_TERMINAL_BACKEND_H
#define DEVICES_TERMINAL_BACKEND_H

/* Terminal-I/O backend — owns the host terminal (raw-mode stdin + stdout
 * passthrough). Replaces the SDL2 backend after the 2026-05-25 TUI pivot.
 *
 * Lifetime: singleton, lazily initialized on first
 * terminal_backend_get_singleton() call. Restore-on-exit registered via
 * atexit() during init. When stdin is not a TTY (CI / piped input), the
 * termios setup is skipped — only fcntl(O_NONBLOCK) is applied.
 *
 * Threading: single-threaded. Every entry point must be called from the
 * thread that owns the ISS main loop (or, in Lab 4+, the Verilator
 * eval() loop). See /CLAUDE.md §8.
 *
 * See plans/snake-soc.md §4 for the full behavioral contract.
 */

#include <stdbool.h>
#include <stdint.h>

typedef struct TerminalBackend TerminalBackend;

/* Returns the singleton backend handle. Initializes on first call.
 * NEVER returns NULL — on init failure the singleton degrades to a
 * null object (try_read always returns false). */
TerminalBackend *terminal_backend_get_singleton(void);

/* Non-blocking single-byte read from stdin. Returns true iff a byte
 * was read into *out_byte. EAGAIN / EWOULDBLOCK / EOF → returns false. */
bool terminal_backend_try_read(TerminalBackend *self, uint8_t *out_byte);

#endif /* DEVICES_TERMINAL_BACKEND_H */
