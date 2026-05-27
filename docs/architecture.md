# Architecture Walkthrough

This note gives a high-level map of the DAE Pipeline CPU as implemented today.
For exact signal-level behavior, see `../spec.md`.

## Pipeline Shape

```text
             Frontend                              Backend

  +-----+     +-----+     +----------+      +-------------+
  | IF  | --> | ID  | --> | IR Queue | ---> | IS/Scorebd  |
  +-----+     +-----+     +----------+      +-------------+
                                                 |
                                                 v
                                            +---------+
                                            |  EXE    |
                                            +---------+
                                                 |
                                                 v
                                            +---------+
                                            |  MEM    |
                                            +---------+
                                                 |
                                                 v
                                            +---------+
                                            |  WB     |
                                            +---------+
```

The frontend is allowed to run ahead while the backend is stalled.  The backend
is allowed to drain while the frontend is waiting for instruction memory.  The
IR queue is the boundary between those two worlds.

## Frontend

The frontend fetches and decodes instructions into `decoded_ir_t`.

IF owns:

- the PC,
- one outstanding I-memory request,
- request PC,
- request epoch,
- request sequence ID.

ID wraps the decoder and preserves epoch/sequence identity.

The IR queue stores decoded instructions.  It is not physically cleared on every
redirect.  Instead, each entry carries an epoch.  If the head entry has an old
epoch, it is popped and ignored.

## Backend

The backend issues one instruction at a time from the IR queue head.  Issue is
strictly in order.

The backend contains:

- a 32-entry GPR file,
- a three-row scoreboard,
- a CSR file,
- EXE/MEM/WB token registers.

The backend pipeline payload is `inst_token_t`.  A token carries decoded IR,
operand values, computed results, memory metadata, CSR metadata, trap state,
halt state, epoch, and sequence ID.

Backend redirects create a kill boundary from the redirecting token's epoch and
sequence ID.  Younger in-flight backend tokens are marked killed and then drain
without side effects.

## Control Events

There are three main events.

### MEM Stall

MEM stalls when a load or store is waiting for D-memory response.  During this
stall:

- scoreboard state is held,
- EXE/MEM are held,
- WB drains with a bubble,
- frontend can continue until the IR queue fills.

### EXE Redirect

EXE redirects on taken branches, jumps, and MRET.  The redirect:

- updates IF PC,
- increments the frontend epoch,
- lets stale frontend entries drain by epoch mismatch,
- does not flush the backend scoreboard.

### MEM Trap Redirect

MEM trap redirect has priority over EXE redirect.  It:

- updates trap CSRs,
- redirects IF to `mtvec_base`,
- increments frontend epoch,
- marks younger backend tokens killed,
- marks younger scoreboard rows killed so they no longer create hazards.

## Why This Shape?

The design tries to separate three ideas that are often tangled together in a
classic five-stage teaching CPU:

- fetching future instructions,
- deciding whether an instruction may issue,
- deciding whether a token is still allowed to cause side effects.

That separation is the main teaching value of this core.
