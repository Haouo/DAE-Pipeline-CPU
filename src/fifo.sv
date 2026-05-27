`include "uarch.svh"

module fifo #(
    parameter type T = logic [31:0],
    parameter int  DEPTH = 4
) (
    input  logic clk,
    input  logic rst_n,
    input  logic flush,

    input  T     in_data,
    input  logic in_valid,
    output logic in_ready,

    output T     out_data,
    output logic out_valid,
    input  logic out_ready
);
    localparam int COUNT_W = (DEPTH <= 1) ? 1 : $clog2(DEPTH + 1);
    localparam int PTR_W = (DEPTH <= 1) ? 1 : $clog2(DEPTH);
    localparam logic [COUNT_W-1:0] DEPTH_COUNT = COUNT_W'(DEPTH);
    localparam logic [PTR_W-1:0] LAST_PTR = PTR_W'(DEPTH - 1);

    T mem_q [DEPTH];
    logic [PTR_W-1:0] rd_ptr_q, wr_ptr_q;
    logic [COUNT_W-1:0] count_q;

    logic push;
    logic pop;

    assign in_ready = (count_q != DEPTH_COUNT);
    assign out_valid = (count_q != '0) && !flush;
    assign out_data = mem_q[rd_ptr_q];
    assign push = in_valid && in_ready && !flush;
    assign pop = out_valid && out_ready;

    function automatic logic [PTR_W-1:0] ptr_next(input logic [PTR_W-1:0] ptr);
        if (DEPTH == 1) begin
            return '0;
        end else if (ptr == LAST_PTR) begin
            return '0;
        end else begin
            return ptr + {{(PTR_W-1){1'b0}}, 1'b1};
        end
    endfunction

    always_ff @(posedge clk) begin
        if (!rst_n || flush) begin
            rd_ptr_q <= '0;
            wr_ptr_q <= '0;
            count_q <= '0;
        end else begin
            if (push) begin
                mem_q[wr_ptr_q] <= in_data;
                wr_ptr_q <= ptr_next(wr_ptr_q);
            end
            if (pop) begin
                rd_ptr_q <= ptr_next(rd_ptr_q);
            end

            unique case ({push, pop})
                2'b10: count_q <= count_q + {{(COUNT_W-1){1'b0}}, 1'b1};
                2'b01: count_q <= count_q - {{(COUNT_W-1){1'b0}}, 1'b1};
                default: count_q <= count_q;
            endcase
        end
    end
endmodule
