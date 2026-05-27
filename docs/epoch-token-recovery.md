# Epoch And Token Recovery

This design uses a small epoch/token discipline to reduce frontend flush
coupling.

## Token

An instruction token is the data structure that represents one instruction as it
moves through the backend.  In this RTL, the token type is `inst_token_t`.

It carries:

- decoded instruction fields,
- epoch,
- sequence ID,
- operand values,
- computed results,
- memory metadata,
- trap/halt state,
- a `killed` bit.

Stages use:

```systemverilog
token_alive = token.valid && !token.killed;
```

before producing side effects or control events.

## Epoch

The frontend owns `current_epoch_q`.  Every fetch request records the current
epoch.  Every fetched and decoded instruction carries that epoch.

When a redirect occurs:

```text
current_epoch_q increments
IF PC becomes redirect target
```

Old-path work is not trusted anymore.

## I-Cache Response Filtering

IF supports one outstanding request.  If an I-cache response returns after a
redirect, IF compares the recorded request epoch against the current epoch.  If
they differ, the response is discarded.

This avoids needing a cancellation protocol for instruction memory.

## IR Queue Stale Entry Drain

The IR queue is not physically flushed on redirect.  Instead, the frontend looks
at the queue head:

```systemverilog
queue_stale = queue_valid && queue_ir.epoch != current_epoch_q;
```

Stale entries are popped and never presented as valid backend issue candidates.

## What Epoch Does Not Do Yet

Epoch is frontend-oriented.  The backend uses sequence IDs as kill boundaries
for younger in-flight tokens and scoreboard rows, but it does not use a ROB.

Branch redirects still rely on the fact that the backend is in order and the
branch itself is in EXE.  Trap redirects still come from MEM.  The difference is
that frontend cleanup is less tied to the number of frontend stages, and backend
cleanup is expressed as token kill rather than clearing exact stage registers.

## Teaching Takeaway

Physical flush says:

```text
clear these exact storage elements
```

Epoch recovery says:

```text
this generation is no longer valid
```

That is the conceptual improvement.  The hardware can still clear convenient
local state, but correctness does not depend on every future frontend buffer
getting a bespoke flush rule.
