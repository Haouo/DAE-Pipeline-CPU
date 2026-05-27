`include "uarch.svh"

module dae_wb_stage (
    input  uarch_pkg::inst_token_t wb_i,
    input  logic [31:0]          x10_i,
    input  logic [31:0]          csr_mstatus_i,
    input  logic [31:0]          csr_mtvec_i,
    input  logic [31:0]          csr_mepc_i,
    input  logic [31:0]          csr_mcause_i,
    input  logic [31:0]          csr_mtval_i,
    input  logic [31:0]          csr_mscratch_i,
    input  logic [31:0]          csr_mie_i,

    output logic                 rf_we_o,
    output logic [4:0]           rf_rd_o,
    output logic [31:0]          rf_wdata_o,
    output logic                 commit_valid_o,
    output uarch_pkg::commit_packet_t commit_o
);
    import uarch_pkg::*;

    assign commit_valid_o = wb_i.valid && !wb_i.killed;
    assign rf_we_o = commit_valid_o && !wb_i.trap_pending && !wb_i.halt_observed &&
                     wb_i.ir.rd_to_write && wb_i.ir.rd_idx != 5'd0;
    assign rf_rd_o = wb_i.ir.rd_idx;
    assign rf_wdata_o = wb_i.rd_value;

    always_comb begin
        commit_o = '0;
        commit_o.pc = wb_i.ir.pc;
        commit_o.rd = rf_we_o ? wb_i.ir.rd_idx : 5'd0;
        commit_o.rd_value = rf_we_o ? wb_i.rd_value : 32'd0;
        commit_o.store_valid = commit_valid_o && !wb_i.trap_pending && !wb_i.halt_observed &&
                               wb_i.valid && is_store(wb_i.ir);
        commit_o.store_addr = wb_i.store_addr;
        commit_o.store_data = wb_i.store_data;
        commit_o.store_strb = wb_i.store_strb;
        commit_o.halt_observed = wb_i.halt_observed;
        commit_o.halt_kind = wb_i.halt_kind;
        commit_o.halt_exit_code = halt_exit_code(wb_i.halt_kind, x10_i);
        commit_o.trap_taken = wb_i.trap_pending;
        commit_o.trap_cause = wb_i.trap_cause;
        commit_o.trap_mepc = wb_i.ir.pc;
        commit_o.trap_mtval = wb_i.trap_tval;
        commit_o.csr_mstatus = csr_mstatus_i;
        commit_o.csr_mtvec = csr_mtvec_i;
        commit_o.csr_mepc = csr_mepc_i;
        commit_o.csr_mcause = csr_mcause_i;
        commit_o.csr_mtval = csr_mtval_i;
        commit_o.csr_mscratch = csr_mscratch_i;
        commit_o.csr_mie = csr_mie_i;
    end
endmodule
