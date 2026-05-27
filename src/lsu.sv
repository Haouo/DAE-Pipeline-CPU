`include "uarch.svh"

module lsu (
    input  logic [31:0]          addr_i,
    input  logic [31:0]          store_data_i,
    input  uarch_pkg::mem_len_e  mem_len_i,
    output logic [3:0]           be_o,
    output logic [31:0]          wdata_o,
    output logic                 misaligned_o
);
    import uarch_pkg::*;

    always_comb begin
        be_o = 4'b0000;
        wdata_o = 32'd0;
        misaligned_o = 1'b0;

        unique case (mem_len_i)
            MEM_LEN_1: begin
                be_o = 4'b0001 << addr_i[1:0];
                wdata_o = {4{store_data_i[7:0]}} << (8 * addr_i[1:0]);
            end
            MEM_LEN_2: begin
                misaligned_o = addr_i[0];
                be_o = addr_i[1] ? 4'b1100 : 4'b0011;
                wdata_o = addr_i[1] ? {store_data_i[15:0], 16'd0} : {16'd0, store_data_i[15:0]};
            end
            default: begin
                misaligned_o = |addr_i[1:0];
                be_o = 4'b1111;
                wdata_o = store_data_i;
            end
        endcase
    end
endmodule
