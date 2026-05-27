`include "uarch.svh"

module alu (
    input  uarch_pkg::alu_op_e op_i,
    input  logic [31:0]        lhs_i,
    input  logic [31:0]        rhs_i,
    output logic [31:0]        result_o
);
    import uarch_pkg::*;

    always_comb begin
        unique case (op_i)
            ALU_ADD:  result_o = lhs_i + rhs_i;
            ALU_SLL:  result_o = lhs_i << rhs_i[4:0];
            ALU_SLT:  result_o = ($signed(lhs_i) < $signed(rhs_i)) ? 32'd1 : 32'd0;
            ALU_SLTU: result_o = (lhs_i < rhs_i) ? 32'd1 : 32'd0;
            ALU_XOR:  result_o = lhs_i ^ rhs_i;
            ALU_SRL:  result_o = lhs_i >> rhs_i[4:0];
            ALU_OR:   result_o = lhs_i | rhs_i;
            ALU_AND:  result_o = lhs_i & rhs_i;
            ALU_SUB:  result_o = lhs_i - rhs_i;
            ALU_SRA:  result_o = $signed(lhs_i) >>> rhs_i[4:0];
            default:  result_o = 32'd0;
        endcase
    end
endmodule
