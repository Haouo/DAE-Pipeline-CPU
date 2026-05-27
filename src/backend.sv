`include "uarch.svh"

module dae_backend #(
    parameter bit MEM_FORWARDING = 1'b1,
    parameter bit WB_FORWARDING  = 1'b1
) (
    input  logic                       clk_i,
    input  logic                       rst_i,

    input  uarch_pkg::decoded_ir_t     ir_i,
    input  logic                       ir_valid_i,
    output logic                       ir_ready_o,

    output uarch_pkg::mem_req_t        dmem_req_o,
    input  logic                       dmem_req_ready_i,
    input  uarch_pkg::mem_resp_t       dmem_resp_i,

    input  logic                       msip_i,
    input  logic                       mtip_i,
    input  logic                       meip_i,

    output logic                       redirect_valid_o,
    output logic [31:0]                redirect_pc_o,
    output logic                       trap_flush_o,

    output logic                       commit_valid_o,
    output uarch_pkg::commit_packet_t  commit_o
);
    import uarch_pkg::*;

    inst_token_t exe_q, mem_q, wb_q;
    inst_token_t issue_pkt, exe_calc_pkt, mem_calc_pkt;
    inst_token_t next_exe_pkt, next_mem_pkt, next_wb_pkt;
    kill_event_t kill_event;

    logic [4:0] rf_rs1, rf_rs2, rf_rd;
    logic [31:0] rf_rs1_val, rf_rs2_val, rf_wdata, rf_x10;
    logic rf_we;

    logic issue_valid;
    decoded_ir_t issue_ir;
    logic [31:0] issue_rs1_val, issue_rs2_val;
    /* verilator lint_off UNUSEDSIGNAL */
    scoreboard_row_t sb_exe_row, sb_mem_row, sb_wb_row;
    /* verilator lint_on UNUSEDSIGNAL */

    logic [31:0] csr_source, csr_old, csr_new;
    logic csr_illegal;
    logic [31:0] csr_mstatus, csr_mtvec, csr_mepc, csr_mcause, csr_mtval;
    logic [31:0] csr_mscratch, csr_mie, csr_mip, csr_mtvec_base;
    logic exe_redirect_valid, exe_csr_valid, exe_csr_commit, exe_mret_commit;
    logic exe_rd_ready, mem_rd_ready;
    logic mem_stall, mem_take_trap;
    logic [31:0] mem_trap_cause, mem_trap_tval;
    logic last_step_entered_trap_q;
    logic backend_flush;
    logic pipeline_advance;
    logic [31:0] redirect_pc_from_exe;

    assign trap_flush_o = backend_flush;
    assign backend_flush = mem_take_trap && !mem_stall;
    assign redirect_valid_o = kill_event.valid;
    assign redirect_pc_o = backend_flush ? csr_mtvec_base : redirect_pc_from_exe;

    regfile u_regfile (
        .clk_i(clk_i),
        .rst_i(rst_i),
        .rs1_i(rf_rs1),
        .rs2_i(rf_rs2),
        .rs1_o(rf_rs1_val),
        .rs2_o(rf_rs2_val),
        .we_i(rf_we),
        .rd_i(rf_rd),
        .wdata_i(rf_wdata),
        .x10_o(rf_x10)
    );

    scoreboard #(
        .MEM_FORWARDING(MEM_FORWARDING),
        .WB_FORWARDING(WB_FORWARDING)
    ) u_scoreboard (
        .clk(clk_i),
        .rst_n(!rst_i),
        .kill_i(kill_event),
        .stall(mem_stall),
        .candidate(ir_i),
        .candidate_valid(ir_valid_i),
        .gpr_rs1_idx(rf_rs1),
        .gpr_rs2_idx(rf_rs2),
        .gpr_rs1_val(rf_rs1_val),
        .gpr_rs2_val(rf_rs2_val),
        .mem_result_bus(mem_q.rd_value),
        .wb_result_bus(wb_q.rd_value),
        .exe_rd_ready(exe_rd_ready),
        .mem_rd_ready(mem_rd_ready),
        .issue_valid(issue_valid),
        .issue_ir(issue_ir),
        .issue_rs1_val(issue_rs1_val),
        .issue_rs2_val(issue_rs2_val),
        .exe_row_o(sb_exe_row),
        .mem_row_o(sb_mem_row),
        .wb_row_o(sb_wb_row)
    );

    assign ir_ready_o = issue_valid;

    always_comb begin
        issue_pkt = '0;
        issue_pkt.valid = issue_valid;
        issue_pkt.epoch = issue_ir.epoch;
        issue_pkt.seq_id = issue_ir.seq_id;
        issue_pkt.ir = issue_ir;
        issue_pkt.rs1_val = issue_rs1_val;
        issue_pkt.rs2_val = issue_rs2_val;
    end

    CSRFile u_csr (
        .clk_i(clk_i),
        .rst_i(rst_i),
        .msip_i(msip_i),
        .mtip_i(mtip_i),
        .meip_i(meip_i),
        .csr_valid_i(exe_csr_commit),
        .csr_op_i(exe_q.ir.sys_kind),
        .csr_addr_i(exe_q.ir.csr_addr),
        .csr_source_i(csr_source),
        .csr_writes_i(exe_q.ir.csr_writes),
        .csr_old_o(csr_old),
        .csr_new_o(csr_new),
        .csr_illegal_o(csr_illegal),
        .trap_entry_i(backend_flush),
        .trap_cause_i(mem_trap_cause),
        .trap_epc_i(mem_q.ir.pc),
        .trap_tval_i(mem_trap_tval),
        .mret_i(exe_mret_commit),
        .mstatus_o(csr_mstatus),
        .mtvec_o(csr_mtvec),
        .mepc_o(csr_mepc),
        .mcause_o(csr_mcause),
        .mtval_o(csr_mtval),
        .mscratch_o(csr_mscratch),
        .mie_o(csr_mie),
        .mip_o(csr_mip),
        .mtvec_base_o(csr_mtvec_base)
    );

    dae_exe_stage u_exe (
        .exe_i(exe_q),
        .mem_stall_i(mem_stall),
        .trap_pending_i(mem_take_trap),
        .csr_mepc_i(csr_mepc),
        .csr_old_i(csr_old),
        .csr_new_i(csr_new),
        .csr_illegal_i(csr_illegal),
        .exe_o(exe_calc_pkt),
        .redirect_valid_o(exe_redirect_valid),
        .redirect_pc_o(redirect_pc_from_exe),
        .csr_valid_o(exe_csr_valid),
        .csr_commit_o(exe_csr_commit),
        .mret_commit_o(exe_mret_commit),
        .csr_source_o(csr_source),
        .rd_ready_o(exe_rd_ready)
    );

    assign pipeline_advance = !mem_stall;

    dae_mem_stage u_mem (
        .clk_i(clk_i),
        .rst_i(rst_i),
        .clear_i(backend_flush),
        .advance_i(pipeline_advance),
        .mem_i(mem_q),
        .mem_o(mem_calc_pkt),
        .dmem_req_o(dmem_req_o),
        .dmem_req_ready_i(dmem_req_ready_i),
        .dmem_resp_i(dmem_resp_i),
        .csr_mstatus_i(csr_mstatus),
        .csr_mie_i(csr_mie),
        .csr_mip_i(csr_mip),
        .csr_mtvec_base_i(csr_mtvec_base),
        .last_step_entered_trap_i(last_step_entered_trap_q),
        .stall_o(mem_stall),
        .take_trap_o(mem_take_trap),
        .trap_cause_o(mem_trap_cause),
        .trap_tval_o(mem_trap_tval),
        .rd_ready_o(mem_rd_ready)
    );

    always_comb begin
        kill_event = '0;
        if (backend_flush) begin
            kill_event.valid = 1'b1;
            kill_event.epoch = mem_q.epoch;
            kill_event.seq_id = mem_q.seq_id;
        end else if (exe_redirect_valid) begin
            kill_event.valid = 1'b1;
            kill_event.epoch = exe_q.epoch;
            kill_event.seq_id = exe_q.seq_id;
        end

        next_wb_pkt = kill_token_if_younger(mem_calc_pkt, kill_event);
        next_wb_pkt.valid = mem_q.valid;

        next_mem_pkt = kill_token_if_younger(exe_calc_pkt, kill_event);
        next_mem_pkt.valid = exe_q.valid;

        next_exe_pkt = kill_token_if_younger(issue_pkt, kill_event);
    end

    dae_wb_stage u_wb (
        .wb_i(wb_q),
        .x10_i(rf_x10),
        .csr_mstatus_i(csr_mstatus),
        .csr_mtvec_i(csr_mtvec),
        .csr_mepc_i(csr_mepc),
        .csr_mcause_i(csr_mcause),
        .csr_mtval_i(csr_mtval),
        .csr_mscratch_i(csr_mscratch),
        .csr_mie_i(csr_mie),
        .rf_we_o(rf_we),
        .rf_rd_o(rf_rd),
        .rf_wdata_o(rf_wdata),
        .commit_valid_o(commit_valid_o),
        .commit_o(commit_o)
    );

    always_ff @(posedge clk_i) begin
        if (rst_i) begin
            exe_q <= '0;
            mem_q <= '0;
            wb_q <= '0;
            last_step_entered_trap_q <= 1'b0;
        end else begin
            if (mem_stall) begin
                wb_q <= '0;
            end else begin
                wb_q <= next_wb_pkt;
                mem_q <= next_mem_pkt;
                exe_q <= next_exe_pkt;
            end

            if (commit_valid_o) begin
                last_step_entered_trap_q <= wb_q.trap_pending;
            end
        end
    end

`ifndef SYNTHESIS
    always_ff @(posedge clk_i) begin
        if (!rst_i) begin
            assert (!(dmem_req_o.req_valid && dmem_req_o.we && mem_take_trap));
            assert (!(ir_valid_i && backend_flush));
        end
    end
`endif
endmodule
