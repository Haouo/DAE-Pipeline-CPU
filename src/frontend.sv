`include "uarch.svh"

module dae_frontend #(
    parameter int IR_QUEUE_DEPTH = 4,
    parameter logic [31:0] RESET_PC = 32'h0000_0000
) (
    input  logic                 clk_i,
    input  logic                 rst_i,
    input  logic                 flush_i,
    input  logic                 redirect_valid_i,
    input  logic [31:0]          redirect_pc_i,

    output uarch_pkg::mem_req_t  imem_req_o,
    input  logic                 imem_req_ready_i,
    input  uarch_pkg::mem_resp_t imem_resp_i,

    output uarch_pkg::decoded_ir_t ir_o,
    output logic                   ir_valid_o,
    input  logic                   ir_ready_i
);
    import uarch_pkg::*;

    fetch_pkt_t fetch_pkt;
    decoded_ir_t decoded_ir, queue_ir;
    logic fetch_valid, fetch_ready;
    logic decoded_valid, decoded_ready;
    logic queue_valid, queue_stale;
    epoch_t current_epoch_q;
    seq_id_t next_seq_id_q;

    always_ff @(posedge clk_i) begin
        if (rst_i) begin
            current_epoch_q <= '0;
            next_seq_id_q <= '0;
        end else begin
            if (redirect_valid_i) begin
                current_epoch_q <= current_epoch_q + 1'b1;
            end
            if (imem_req_o.req_valid && imem_req_ready_i) begin
                next_seq_id_q <= next_seq_id_q + 1'b1;
            end
        end
    end

    dae_if_stage #(
        .RESET_PC(RESET_PC)
    ) u_if (
        .clk_i(clk_i),
        .rst_i(rst_i),
        .flush_i(flush_i),
        .redirect_valid_i(redirect_valid_i),
        .redirect_pc_i(redirect_pc_i),
        .current_epoch_i(current_epoch_q),
        .next_seq_id_i(next_seq_id_q),
        .imem_req_o(imem_req_o),
        .imem_req_ready_i(imem_req_ready_i),
        .imem_resp_i(imem_resp_i),
        .out_valid_o(fetch_valid),
        .out_ready_i(fetch_ready),
        .out_o(fetch_pkt)
    );

    dae_id_stage u_id (
        .flush_i(flush_i),
        .in_valid_i(fetch_valid),
        .in_ready_o(fetch_ready),
        .in_i(fetch_pkt),
        .out_valid_o(decoded_valid),
        .out_ready_i(decoded_ready),
        .out_o(decoded_ir)
    );

    fifo #(
        .T(decoded_ir_t),
        .DEPTH(IR_QUEUE_DEPTH)
    ) u_ir_queue (
        .clk(clk_i),
        .rst_n(!rst_i),
        .flush(1'b0),
        .in_data(decoded_ir),
        .in_valid(decoded_valid && !flush_i),
        .in_ready(decoded_ready),
        .out_data(queue_ir),
        .out_valid(queue_valid),
        .out_ready((ir_ready_i && ir_valid_o) || queue_stale)
    );

    assign queue_stale = queue_valid && queue_ir.epoch != current_epoch_q;
    assign ir_o = queue_ir;
    assign ir_valid_o = queue_valid && !flush_i && !queue_stale;
endmodule
