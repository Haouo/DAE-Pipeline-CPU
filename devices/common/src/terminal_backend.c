/* Terminal-I/O backend — see terminal_backend.h for the contract.
 *
 * Owns the host terminal: puts stdin into raw, non-canonical, no-echo
 * mode (when stdin is a TTY), sets O_NONBLOCK so reads never block,
 * and registers an atexit() handler that restores the original termios.
 *
 * Introduced 2026-05-25 in the TUI pivot — replaces the SDL2 backend.
 * See plans/snake-soc.md §4. */

#include "terminal_backend.h"

#include "common.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

struct TerminalBackend {
    bool           initialized;
    bool           is_tty;
    bool           termios_saved;
    struct termios saved_termios;
};

static struct TerminalBackend g_backend = {
    .initialized   = false,
    .is_tty        = false,
    .termios_saved = false,
};

static void terminal_backend_destroy(void) {
    if (g_backend.termios_saved) {
        /* Best-effort restore; ignore errors during exit. */
        (void)tcsetattr(STDIN_FILENO, TCSANOW, &g_backend.saved_termios);
        g_backend.termios_saved = false;
    }
}

static void terminal_backend_init(void) {
    if (g_backend.initialized) return;
    g_backend.initialized = true;

    /* TTY-only: enter raw mode so arrow-key bytes (ESC [ A/B/C/D) and
     * Ctrl-C (0x03) arrive verbatim instead of being cooked by the line
     * discipline. On non-TTY stdin (CI / piped input) skip termios. */
    g_backend.is_tty = (isatty(STDIN_FILENO) == 1);
    if (g_backend.is_tty) {
        if (tcgetattr(STDIN_FILENO, &g_backend.saved_termios) == 0) {
            g_backend.termios_saved = true;
            struct termios raw = g_backend.saved_termios;
            cfmakeraw(&raw);
            raw.c_cc[VMIN]  = 0;
            raw.c_cc[VTIME] = 0;
            (void)tcsetattr(STDIN_FILENO, TCSANOW, &raw);
        }
    }

    /* Non-blocking read at the fd level — applies whether or not stdin
     * is a TTY. EAGAIN / EWOULDBLOCK becomes our "no byte" signal. */
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (flags != -1) {
        (void)fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    }

    /* Restore-on-exit covers most halt paths (EBREAK voluntary halt,
     * bus-error exit(N), illegal-instruction exit(130)). _exit / uncaught
     * SIGSEGV bypass atexit and leave the terminal in raw mode. */
    (void)atexit(terminal_backend_destroy);
}

TerminalBackend *terminal_backend_get_singleton(void) {
    terminal_backend_init();
    return &g_backend;
}

bool terminal_backend_try_read(TerminalBackend *self, uint8_t *out_byte) {
    DEV_ASSERT(self != NULL, "terminal_backend_try_read: self NULL");
    DEV_ASSERT(out_byte != NULL, "terminal_backend_try_read: out_byte NULL");
    ssize_t n = read(STDIN_FILENO, out_byte, 1);
    if (n == 1) return true;
    /* n == 0 (EOF) or n < 0 (EAGAIN / EWOULDBLOCK / other) → no byte. */
    return false;
}
