`include "uarch.svh"

module scoreboard #(
    parameter bit MEM_FORWARDING = 1'b1,
    parameter bit WB_FORWARDING  = 1'b1
) (
    input  logic clk,
    input  logic rst_n,
    input  uarch_pkg::kill_event_t kill_i,
    input  logic stall,

    input  uarch_pkg::decoded_ir_t candidate,
    input  logic                   candidate_valid,

    output logic [4:0]             gpr_rs1_idx,
    output logic [4:0]             gpr_rs2_idx,
    input  logic [31:0]            gpr_rs1_val,
    input  logic [31:0]            gpr_rs2_val,

    input  logic [31:0]            mem_result_bus,
    input  logic [31:0]            wb_result_bus,
    input  logic                   exe_rd_ready,
    input  logic                   mem_rd_ready,

    output logic                   issue_valid,
    output uarch_pkg::decoded_ir_t issue_ir,
    output logic [31:0]            issue_rs1_val,
    output logic [31:0]            issue_rs2_val,

    output uarch_pkg::scoreboard_row_t exe_row_o,
    output uarch_pkg::scoreboard_row_t mem_row_o,
    output uarch_pkg::scoreboard_row_t wb_row_o
);
    import uarch_pkg::*;

    scoreboard_row_t exe_row_q, mem_row_q, wb_row_q;
    scoreboard_row_t mem_shift_row, wb_shift_row;
    logic rs1_hazard, rs2_hazard;
    logic rs1_fwd_mem, rs1_fwd_wb, rs2_fwd_mem, rs2_fwd_wb;

    assign exe_row_o = exe_row_q;
    assign mem_row_o = mem_row_q;
    assign wb_row_o = wb_row_q;
    assign gpr_rs1_idx = candidate.rs1_idx;
    assign gpr_rs2_idx = candidate.rs2_idx;
    assign issue_ir = candidate;

    function automatic logic rd_match(input scoreboard_row_t sb_row, input logic src_use, input logic [4:0] idx);
        return sb_row.valid && !sb_row.killed && sb_row.rd_to_write && src_use &&
               idx != 5'd0 && sb_row.rd_idx == idx;
    endfunction

    always_comb begin
        rs1_hazard = 1'b0;
        rs2_hazard = 1'b0;
        rs1_fwd_mem = 1'b0;
        rs1_fwd_wb = 1'b0;
        rs2_fwd_mem = 1'b0;
        rs2_fwd_wb = 1'b0;

        if (rd_match(exe_row_q, candidate.rs1_use, candidate.rs1_idx)) begin
            rs1_hazard = 1'b1;
        end else if (rd_match(mem_row_q, candidate.rs1_use, candidate.rs1_idx)) begin
            if (MEM_FORWARDING && mem_row_q.rd_ready) rs1_fwd_mem = 1'b1;
            else rs1_hazard = 1'b1;
        end else if (rd_match(wb_row_q, candidate.rs1_use, candidate.rs1_idx)) begin
            if (WB_FORWARDING && wb_row_q.rd_ready) rs1_fwd_wb = 1'b1;
            else rs1_hazard = 1'b1;
        end

        if (rd_match(exe_row_q, candidate.rs2_use, candidate.rs2_idx)) begin
            rs2_hazard = 1'b1;
        end else if (rd_match(mem_row_q, candidate.rs2_use, candidate.rs2_idx)) begin
            if (MEM_FORWARDING && mem_row_q.rd_ready) rs2_fwd_mem = 1'b1;
            else rs2_hazard = 1'b1;
        end else if (rd_match(wb_row_q, candidate.rs2_use, candidate.rs2_idx)) begin
            if (WB_FORWARDING && wb_row_q.rd_ready) rs2_fwd_wb = 1'b1;
            else rs2_hazard = 1'b1;
        end

        issue_valid = candidate_valid && !kill_i.valid && !stall && !rs1_hazard && !rs2_hazard;
        issue_rs1_val = rs1_fwd_mem ? mem_result_bus :
                        rs1_fwd_wb  ? wb_result_bus  : gpr_rs1_val;
        issue_rs2_val = rs2_fwd_mem ? mem_result_bus :
                        rs2_fwd_wb  ? wb_result_bus  : gpr_rs2_val;
    end

    function automatic scoreboard_row_t row_from_ir(input decoded_ir_t ir);
        scoreboard_row_t new_row;
        new_row = '0;
        new_row.valid = 1'b1;
        new_row.epoch = ir.epoch;
        new_row.seq_id = ir.seq_id;
        new_row.killed = 1'b0;
        new_row.rs1_use = ir.rs1_use;
        new_row.rs1_idx = ir.rs1_idx;
        new_row.rs2_use = ir.rs2_use;
        new_row.rs2_idx = ir.rs2_idx;
        new_row.rd_to_write = ir.rd_to_write && ir.rd_idx != 5'd0;
        new_row.rd_idx = ir.rd_idx;
        new_row.rd_ready = 1'b0;
        new_row.rd_written_back = 1'b0;
        return new_row;
    endfunction

    always_comb begin
        wb_shift_row = kill_scoreboard_row_if_younger(mem_row_q, kill_i);
        wb_shift_row.rd_ready = mem_row_q.rd_ready || mem_rd_ready;
        wb_shift_row.rd_written_back = 1'b0;

        mem_shift_row = kill_scoreboard_row_if_younger(exe_row_q, kill_i);
        mem_shift_row.rd_ready = exe_row_q.rd_ready || exe_rd_ready;
        mem_shift_row.rd_written_back = 1'b0;
    end

    always_ff @(posedge clk) begin
        if (!rst_n) begin
            exe_row_q <= '0;
            mem_row_q <= '0;
            wb_row_q <= '0;
        end else if (stall) begin
            exe_row_q <= kill_scoreboard_row_if_younger(exe_row_q, kill_i);
            mem_row_q <= kill_scoreboard_row_if_younger(mem_row_q, kill_i);
            wb_row_q <= '0;
        end else begin
            wb_row_q <= wb_shift_row;
            mem_row_q <= mem_shift_row;

            if (issue_valid) begin
                exe_row_q <= kill_scoreboard_row_if_younger(row_from_ir(candidate), kill_i);
            end else begin
                exe_row_q <= '0;
            end
        end
    end
endmodule
