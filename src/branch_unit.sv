`include "uarch.svh"

module branch_unit (
    input  uarch_pkg::branch_op_e op,
    input  logic [31:0]           pc,
    input  logic [31:0]           rs1_val,
    input  logic [31:0]           rs2_val,
    input  logic [31:0]           imm,
    output logic                  taken,
    output logic [31:0]           target_pc,
    output logic [31:0]           link_val,
    output logic                  mispredict
);
    import uarch_pkg::*;

    always_comb begin
        unique case (op)
            BR_JUMP: taken = 1'b1;
            BR_EQ:   taken = (rs1_val == rs2_val);
            BR_NE:   taken = (rs1_val != rs2_val);
            BR_LT:   taken = ($signed(rs1_val) < $signed(rs2_val));
            BR_LTU:  taken = (rs1_val < rs2_val);
            BR_GE:   taken = ($signed(rs1_val) >= $signed(rs2_val));
            BR_GEU:  taken = (rs1_val >= rs2_val);
            default: taken = 1'b0;
        endcase

        target_pc = pc + imm;
        link_val = pc + 32'd4;
        mispredict = taken;
    end
endmodule
