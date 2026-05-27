`include "uarch.svh"

module Top #(
    parameter bit MEM_FORWARDING = 1'b1,
    parameter bit WB_FORWARDING  = 1'b1,
    parameter int IR_QUEUE_DEPTH = 4,
    parameter logic [31:0] RESET_PC = 32'h0000_0000
) (
    input  logic                      clk_i,
    input  logic                      rst_i,

    output uarch_pkg::mem_req_t       imem_req_o,
    input  logic                      imem_req_ready_i,
    input  uarch_pkg::mem_resp_t      imem_resp_i,

    output uarch_pkg::mem_req_t       dmem_req_o,
    input  logic                      dmem_req_ready_i,
    input  uarch_pkg::mem_resp_t      dmem_resp_i,

    input  logic                      msip_i,
    input  logic                      mtip_i,
    input  logic                      meip_i,

    output logic                      commit_valid_o,
    output uarch_pkg::commit_packet_t commit_o
);
    import uarch_pkg::*;

    decoded_ir_t ir;
    logic ir_valid, ir_ready;
    logic redirect_valid;
    logic [31:0] redirect_pc;
    logic trap_flush;

    dae_frontend #(
        .IR_QUEUE_DEPTH(IR_QUEUE_DEPTH),
        .RESET_PC(RESET_PC)
    ) u_frontend (
        .clk_i(clk_i),
        .rst_i(rst_i),
        .flush_i(redirect_valid),
        .redirect_valid_i(redirect_valid),
        .redirect_pc_i(redirect_pc),
        .imem_req_o(imem_req_o),
        .imem_req_ready_i(imem_req_ready_i),
        .imem_resp_i(imem_resp_i),
        .ir_o(ir),
        .ir_valid_o(ir_valid),
        .ir_ready_i(ir_ready)
    );

    dae_backend #(
        .MEM_FORWARDING(MEM_FORWARDING),
        .WB_FORWARDING(WB_FORWARDING)
    ) u_backend (
        .clk_i(clk_i),
        .rst_i(rst_i),
        .ir_i(ir),
        .ir_valid_i(ir_valid),
        .ir_ready_o(ir_ready),
        .dmem_req_o(dmem_req_o),
        .dmem_req_ready_i(dmem_req_ready_i),
        .dmem_resp_i(dmem_resp_i),
        .msip_i(msip_i),
        .mtip_i(mtip_i),
        .meip_i(meip_i),
        .redirect_valid_o(redirect_valid),
        .redirect_pc_o(redirect_pc),
        .trap_flush_o(trap_flush),
        .commit_valid_o(commit_valid_o),
        .commit_o(commit_o)
    );

`ifndef SYNTHESIS
    always_ff @(posedge clk_i) begin
        if (!rst_i) begin
            assert (!(ir_valid && trap_flush));
        end
    end
`endif
endmodule
