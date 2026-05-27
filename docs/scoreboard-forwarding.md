# Scoreboard And Forwarding

The DAE backend issues instructions strictly in order.  Only the head of the IR
queue can be considered for issue.

The scoreboard tracks the three backend rows:

```text
EXE row
MEM row
WB row
```

Each row records the in-flight instruction identity, whether it has been killed,
whether it will write an RD register, which RD it targets, and whether the result
is ready.

## Hazard Check

For the candidate instruction at the IR queue head:

1. Check RS1 against in-flight RD values.
2. Check RS2 against in-flight RD values.
3. If a match is in EXE, stall.  There is no EXE-to-IS forwarding.
4. If a match is in MEM, forward only when `MEM_FORWARDING` is enabled and the
   MEM row result is ready.
5. If a match is in WB, forward only when `WB_FORWARDING` is enabled and the WB
   row result is ready.

If either source has an unresolved dependency, issue is blocked and the IR queue
head remains in place.

Killed rows are ignored during hazard checks.  This lets backend logical flush
mark younger work dead without manufacturing false dependencies while those rows
drain through the simple stage-row scoreboard.

## Forwarding Parameters

The RTL exposes:

```systemverilog
parameter bit MEM_FORWARDING = 1'b1;
parameter bit WB_FORWARDING  = 1'b1;
```

All four combinations are intended to be architecturally correct.  Disabling a
path should only add stalls.

## Result Readiness

ALU, branch-link, and CSR results become ready when entering the MEM row.  Load
results become ready after the MEM access completes and the instruction advances
toward WB.

Stores and most branch instructions do not produce GPR results.

## Why This Is Educational

This scoreboard is still simple enough to inspect in one file, but it already
forces students to think about:

- producer/consumer dependencies,
- forwarding as an optimization rather than a semantic requirement,
- read-during-write register file behavior,
- load-use latency,
- why EXE-to-IS forwarding can be deliberately omitted to shorten timing.

The next architectural step would be replacing stage-row tracking with producer
tags and completion buses.
