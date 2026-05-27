# From Classic 5-Stage To DAE

A classic five-stage CPU is usually taught as:

```text
IF -> ID -> EX -> MEM -> WB
```

That model is excellent for learning the basics, but it becomes brittle when the
same control signals must handle:

- cache misses,
- branch redirects,
- traps,
- CSRs,
- stores,
- forwarding,
- read-during-write register file behavior.

The DAE pipeline keeps the core in-order, but changes the control structure.

## Problem With Stage-Centric Control

In a simple pipeline, a branch in EX might mean:

```text
flush IF and ID
```

A trap in MEM might mean:

```text
flush IF, ID, EX, MEM, and maybe the scoreboard
```

A D-cache miss might mean:

```text
hold EX and MEM, let WB drain, maybe hold ID, maybe hold IF
```

Each rule can be correct by itself, but the design becomes fragile when every
stage needs to know where every other event is produced.

## DAE Direction

DAE splits the problem:

```text
Frontend produces decoded IR.
IR Queue absorbs frontend/backend rate mismatch.
Backend issues strictly in order using a scoreboard.
Instruction tokens carry identity and validity.
Epochs invalidate stale frontend work.
```

This still uses pipeline registers.  It is not a dataflow machine.  The
important shift is that a stage no longer has to physically clear every younger
frontend storage element on redirect.  A redirect changes the epoch, and stale
instructions naturally lose validity.

## What Is Still Classic?

Several parts intentionally remain close to a classic teaching CPU:

- one instruction issues per cycle,
- EXE computes integer and branch results,
- MEM performs data memory access,
- WB writes the GPR file,
- forwarding is from MEM/WB paths,
- the scoreboard is tied to EXE/MEM/WB rows.

This makes the implementation readable while still showing more scalable
control discipline.

## Teaching Takeaway

The lesson is not that five-stage pipelines are bad.  The lesson is that once a
pipeline grows real control hazards and precise side effects, it helps to name
instruction identity and lifetime explicitly.

