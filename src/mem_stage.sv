`include "uarch.svh"

module dae_mem_stage (
    input  logic                 clk_i,
    input  logic                 rst_i,
    input  logic                 clear_i,
    input  logic                 advance_i,

    input  uarch_pkg::inst_token_t mem_i,
    output uarch_pkg::inst_token_t mem_o,

    output uarch_pkg::mem_req_t  dmem_req_o,
    input  logic                 dmem_req_ready_i,
    input  uarch_pkg::mem_resp_t dmem_resp_i,

    input  logic [31:0]          csr_mstatus_i,
    input  logic [31:0]          csr_mie_i,
    input  logic [31:0]          csr_mip_i,
    input  logic [31:0]          csr_mtvec_base_i,
    input  logic                 last_step_entered_trap_i,

    output logic                 stall_o,
    output logic                 take_trap_o,
    output logic [31:0]          trap_cause_o,
    output logic [31:0]          trap_tval_o,
    output logic                 rd_ready_o
);
    import riscv_pkg::*;
    import uarch_pkg::*;

    logic req_sent_q;
    logic [3:0] be;
    logic [31:0] wdata, load_value;
    logic misaligned;
    logic is_memory, is_load_inst, is_store_inst;
    logic pre_halt, take_halt;
    logic async_trap, sync_trap;
    logic [31:0] async_cause;
    logic req_fire, waiting;
    logic token_alive;

    lsu u_lsu (
        .addr_i(mem_i.mem_addr),
        .store_data_i(mem_i.rs2_val),
        .mem_len_i(mem_i.ir.mem_len),
        .be_o(be),
        .wdata_o(wdata),
        .misaligned_o(misaligned)
    );

    load_data_filter u_load_filter (
        .addr_i(mem_i.mem_addr),
        .rdata_i(dmem_resp_i.rdata),
        .mem_len_i(mem_i.ir.mem_len),
        .signext_i(mem_i.ir.load_signext),
        .value_o(load_value)
    );

    assign token_alive = mem_i.valid && !mem_i.killed;
    assign is_memory = token_alive && is_mem(mem_i.ir);
    assign is_load_inst = token_alive && is_load(mem_i.ir);
    assign is_store_inst = token_alive && is_store(mem_i.ir);

    always_comb begin
        async_cause = 32'd0;
        if ((csr_mstatus_i & MSTATUS_MIE) != 32'd0) begin
            if ((csr_mie_i & csr_mip_i & MIP_MEIP) != 32'd0) async_cause = 32'h8000_000b;
            else if ((csr_mie_i & csr_mip_i & MIP_MSIP) != 32'd0) async_cause = 32'h8000_0003;
            else if ((csr_mie_i & csr_mip_i & MIP_MTIP) != 32'd0) async_cause = 32'h8000_0007;
        end

        pre_halt = 1'b0;
        take_halt = 1'b0;
        async_trap = 1'b0;
        sync_trap = 1'b0;
        take_trap_o = 1'b0;
        trap_cause_o = 32'd0;
        trap_tval_o = 32'd0;

        if (token_alive) begin
            pre_halt = mem_i.halt_observed || mem_i.ir.if_bus_error || mem_i.ir.illegal ||
                       (mem_i.ir.sys_kind == SYS_EBREAK) ||
                       (is_memory && misaligned);
            async_trap = (async_cause != 32'd0);
            sync_trap = (mem_i.ir.sys_kind == SYS_ECALL);

            if (pre_halt) begin
                take_halt = 1'b1;
            end else if (async_trap) begin
                take_trap_o = 1'b1;
                trap_cause_o = async_cause;
            end else if (sync_trap) begin
                take_trap_o = 1'b1;
                trap_cause_o = 32'd11;
            end

            if (last_step_entered_trap_i && (mem_i.ir.pc == csr_mtvec_base_i) &&
                (async_trap || sync_trap) && !pre_halt) begin
                take_trap_o = 1'b0;
                take_halt = 1'b1;
            end
        end
    end

    assign req_fire = dmem_req_o.req_valid && dmem_req_ready_i;
    assign waiting = is_memory && !misaligned && !take_trap_o && !take_halt &&
                     !(req_sent_q && dmem_resp_i.resp_valid);
    assign stall_o = token_alive && waiting;
    assign rd_ready_o = token_alive && (!is_load(mem_i.ir) || (req_sent_q && dmem_resp_i.resp_valid));

    always_comb begin
        dmem_req_o = '0;
        dmem_req_o.addr = mem_i.mem_addr;
        dmem_req_o.wdata = wdata;
        dmem_req_o.be = be;
        dmem_req_o.we = is_store_inst && !take_trap_o && !take_halt && !misaligned;
        dmem_req_o.req_valid = is_memory && !req_sent_q &&
                               !take_trap_o && !take_halt && !misaligned;
    end

    always_comb begin
        mem_o = mem_i;
        mem_o.store_strb = be;
        mem_o.store_data = mem_i.rs2_val;
        mem_o.store_addr = mem_i.mem_addr;

        if (is_load_inst && dmem_resp_i.resp_valid) begin
            mem_o.rd_value = load_value;
        end

        if (take_trap_o) begin
            mem_o.trap_pending = 1'b1;
            mem_o.trap_cause = trap_cause_o;
            mem_o.trap_tval = trap_tval_o;
        end

        if (take_halt) begin
            mem_o.halt_observed = 1'b1;
            if (last_step_entered_trap_i && (mem_i.ir.pc == csr_mtvec_base_i) &&
                (async_trap || sync_trap) && !pre_halt) begin
                mem_o.halt_kind = HALT_DOUBLE_TRAP;
            end else if (mem_i.halt_observed) begin
                mem_o.halt_kind = mem_i.halt_kind;
            end else if (mem_i.ir.if_bus_error) begin
                mem_o.halt_kind = HALT_BUS_ERROR_IF;
            end else if (mem_i.ir.illegal) begin
                mem_o.halt_kind = HALT_ILLEGAL;
            end else if (mem_i.ir.sys_kind == SYS_EBREAK) begin
                mem_o.halt_kind = HALT_VOLUNTARY;
            end else if (is_memory && misaligned) begin
                mem_o.halt_kind = is_store_inst ? HALT_MISALIGN_ST : HALT_MISALIGN_LD;
            end else begin
                mem_o.halt_kind = HALT_NONE;
            end
        end

        if (is_memory && req_sent_q && dmem_resp_i.resp_valid && dmem_resp_i.error) begin
            mem_o.halt_observed = 1'b1;
            mem_o.halt_kind = is_store_inst ? HALT_BUS_ERROR_ST : HALT_BUS_ERROR_LD;
        end
    end

    always_ff @(posedge clk_i) begin
        if (rst_i || clear_i || advance_i) begin
            req_sent_q <= 1'b0;
        end else if (req_fire) begin
            req_sent_q <= 1'b1;
        end
    end
endmodule
