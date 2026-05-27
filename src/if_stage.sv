`include "uarch.svh"

module dae_if_stage #(
    parameter logic [31:0] RESET_PC = 32'h0000_0000
) (
    input  logic                 clk_i,
    input  logic                 rst_i,
    input  logic                 flush_i,
    input  logic                 redirect_valid_i,
    input  logic [31:0]          redirect_pc_i,
    input  uarch_pkg::epoch_t    current_epoch_i,
    input  uarch_pkg::seq_id_t   next_seq_id_i,

    output uarch_pkg::mem_req_t  imem_req_o,
    input  logic                 imem_req_ready_i,
    input  uarch_pkg::mem_resp_t imem_resp_i,

    output logic                 out_valid_o,
    input  logic                 out_ready_i,
    output uarch_pkg::fetch_pkt_t out_o
);
    import uarch_pkg::*;

    logic [31:0] pc_q, req_pc_q;
    epoch_t req_epoch_q;
    seq_id_t req_seq_id_q;
    logic outstanding_q, drop_q;
    fetch_pkt_t out_q;

    assign imem_req_o.we = 1'b0;
    assign imem_req_o.be = 4'b0000;
    assign imem_req_o.wdata = 32'd0;
    assign imem_req_o.addr = pc_q;
    assign imem_req_o.req_valid = !rst_i && !flush_i && !outstanding_q &&
                                  !out_q.valid && out_ready_i;

    assign out_valid_o = out_q.valid && !flush_i;
    assign out_o = out_q;

    always_ff @(posedge clk_i) begin
        if (rst_i) begin
            pc_q <= RESET_PC;
            req_pc_q <= RESET_PC;
            req_epoch_q <= '0;
            req_seq_id_q <= '0;
            outstanding_q <= 1'b0;
            drop_q <= 1'b0;
            out_q <= '0;
        end else begin
            if (redirect_valid_i) begin
                pc_q <= redirect_pc_i;
                out_q <= '0;
                drop_q <= outstanding_q;
            end else begin
                if (imem_req_o.req_valid && imem_req_ready_i) begin
                    req_pc_q <= pc_q;
                    req_epoch_q <= current_epoch_i;
                    req_seq_id_q <= next_seq_id_i;
                    pc_q <= pc_q + 32'd4;
                    outstanding_q <= 1'b1;
                end
                if (out_q.valid && out_ready_i) begin
                    out_q <= '0;
                end
            end

            if (outstanding_q && imem_resp_i.resp_valid) begin
                outstanding_q <= 1'b0;
                if (drop_q || redirect_valid_i || flush_i || req_epoch_q != current_epoch_i) begin
                    drop_q <= 1'b0;
                end else begin
                    out_q.valid <= 1'b1;
                    out_q.epoch <= req_epoch_q;
                    out_q.seq_id <= req_seq_id_q;
                    out_q.pc <= req_pc_q;
                    out_q.inst <= imem_resp_i.rdata;
                    out_q.error <= imem_resp_i.error;
                end
            end
        end
    end
endmodule
