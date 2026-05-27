# Suggested Lab Roadmap

This roadmap turns the DAE CPU into a teaching sequence.  Each lab should end
with a working simulation and a small set of tests.

## Lab 1: Types, FIFO, And Decoder

Goal: learn the packet vocabulary.

Build or study:

- `uarch.svh`
- `fifo.sv`
- `decoder.sv`

Questions:

- What information must decode preserve for later stages?
- Why should downstream stages avoid re-decoding raw instructions?
- What does `out_valid = !empty && !flush` protect against?

## Lab 2: Frontend

Goal: fetch and decode into an IR queue.

Build or study:

- `if_stage.sv`
- `id_stage.sv`
- `frontend.sv`

Experiments:

- Change `RESET_PC`.
- Add logging for fetch PC and epoch.
- Observe how the IR queue fills when backend issue is blocked.

## Lab 3: Register File And Scoreboard

Goal: implement strict in-order issue with hazards.

Build or study:

- `regfile.sv`
- `scoreboard.sv`
- IS logic in `backend.sv`

Experiments:

- Run with forwarding disabled.
- Run with only MEM forwarding.
- Run with only WB forwarding.
- Compare cycle counts.

## Lab 4: EXE And Branch Redirect

Goal: add ALU, branches, JAL/JALR, and redirect.

Build or study:

- `alu.sv`
- `branch_unit.sv`
- `exe_stage.sv`

Questions:

- Why is prediction always not-taken?
- Why does taken branch imply redirect?
- Why is EXE-to-IS forwarding not implemented?

## Lab 5: Memory And Stalls

Goal: handle loads, stores, byte enables, and memory stalls.

Build or study:

- `lsu.sv`
- `load_data_filter.sv`
- `mem_stage.sv`

Experiments:

- Add artificial D-memory latency.
- Watch WB drain while MEM stalls.
- Check load-use behavior with forwarding on and off.

## Lab 6: CSR, Traps, And Halt Model

Goal: understand side effects and trap entry.

Build or study:

- `CSRFile.sv`
- trap logic in `mem_stage.sv`
- commit packet generation in `wb_stage.sv`

Questions:

- Which events are modeled as traps?
- Which events are modeled as halts?
- Why are stores gated in MEM?

## Lab 7: Epoch And Token Recovery

Goal: replace frontend physical flush thinking with generation validity.

Build or study:

- epoch fields in `uarch.svh`
- epoch management in `frontend.sv`
- I-cache response filtering in `if_stage.sv`
- stale IR queue entry drain in `frontend.sv`

Experiments:

- Force branches to redirect often.
- Add debug prints for current epoch and packet epoch.
- Confirm stale queue entries do not issue.

## Lab 8: Future Extension Project

Choose one:

- replace stage-row scoreboard with producer tags,
- add a small branch predictor,
- move stores into a committed store buffer,
- move CSR writes to a unified commit boundary,
- add age-based event arbitration using `seq_id_t`,
- expand trap behavior toward the full privileged spec.

The point of this final lab is to feel why the current token fields are useful
even before the design becomes out-of-order.

