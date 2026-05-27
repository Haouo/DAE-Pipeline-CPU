`include "uarch.svh"

module load_data_filter (
    input  logic [31:0]         addr_i,
    input  logic [31:0]         rdata_i,
    input  uarch_pkg::mem_len_e mem_len_i,
    input  logic                signext_i,
    output logic [31:0]         value_o
);
    import uarch_pkg::*;

    logic [7:0] byte_v;
    logic [15:0] half_v;

    always_comb begin
        unique case (addr_i[1:0])
            2'd0: byte_v = rdata_i[7:0];
            2'd1: byte_v = rdata_i[15:8];
            2'd2: byte_v = rdata_i[23:16];
            default: byte_v = rdata_i[31:24];
        endcase

        half_v = addr_i[1] ? rdata_i[31:16] : rdata_i[15:0];

        unique case (mem_len_i)
            MEM_LEN_1: value_o = signext_i ? {{24{byte_v[7]}}, byte_v} : {24'd0, byte_v};
            MEM_LEN_2: value_o = signext_i ? {{16{half_v[15]}}, half_v} : {16'd0, half_v};
            default:   value_o = rdata_i;
        endcase
    end
endmodule
