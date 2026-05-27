`include "uarch.svh"

module decoder (
    input  logic [31:0]             pc_i,
    input  logic [31:0]             inst_i,
    input  logic                    if_bus_error_i,
    output uarch_pkg::decoded_ir_t  decoded_o
);
    import riscv_pkg::*;
    import uarch_pkg::*;

    logic [6:0]  opcode;
    logic [2:0]  funct3;
    logic [6:0]  funct7;
    logic [11:0] funct12;
    logic [4:0]  rs1;
    logic [4:0]  rs2;
    logic [4:0]  rd;

    assign opcode  = inst_i[6:0];
    assign rd      = inst_i[11:7];
    assign funct3  = inst_i[14:12];
    assign rs1     = inst_i[19:15];
    assign rs2     = inst_i[24:20];
    assign funct7  = inst_i[31:25];
    assign funct12 = inst_i[31:20];

    function automatic logic [31:0] sext12(input logic [11:0] x);
        return {{20{x[11]}}, x};
    endfunction

    always_comb begin
        decoded_o = '0;
        decoded_o.pc = pc_i;
        decoded_o.raw_inst = inst_i;
        decoded_o.if_bus_error = if_bus_error_i;
        decoded_o.fu_type = FU_INT;
        decoded_o.alu_op = ALU_ADD;
        decoded_o.alu_op1_sel = ALU_OP1_RS1;
        decoded_o.alu_op2_sel = ALU_OP2_RS2;
        decoded_o.branch_op = BR_JUMP;
        decoded_o.sys_kind = SYS_NOT;
        decoded_o.mem_len = MEM_LEN_4;
        decoded_o.rs1_idx = rs1;
        decoded_o.rs2_idx = rs2;
        decoded_o.rd_idx = rd;
        decoded_o.illegal = 1'b0;

        unique case (opcode)
            OPCODE_OP: begin
                decoded_o.rs1_use = 1'b1;
                decoded_o.rs2_use = 1'b1;
                decoded_o.rd_to_write = (rd != 5'd0);
                unique case (funct3)
                    FUNCT3_ADD_SUB: decoded_o.alu_op = funct7[5] ? ALU_SUB : ALU_ADD;
                    FUNCT3_SLL:     decoded_o.alu_op = ALU_SLL;
                    FUNCT3_SLT:     decoded_o.alu_op = ALU_SLT;
                    FUNCT3_SLTU:    decoded_o.alu_op = ALU_SLTU;
                    FUNCT3_XOR:     decoded_o.alu_op = ALU_XOR;
                    FUNCT3_SRL_SRA: decoded_o.alu_op = funct7[5] ? ALU_SRA : ALU_SRL;
                    FUNCT3_OR:      decoded_o.alu_op = ALU_OR;
                    FUNCT3_AND:     decoded_o.alu_op = ALU_AND;
                    default:        decoded_o.illegal = 1'b1;
                endcase
                if (funct7 != 7'h00 && funct7 != 7'h20) decoded_o.illegal = 1'b1;
            end
            OPCODE_OP_IMM: begin
                decoded_o.rs1_use = 1'b1;
                decoded_o.rd_to_write = (rd != 5'd0);
                decoded_o.imm = sext12(inst_i[31:20]);
                decoded_o.alu_op2_sel = ALU_OP2_IMM;
                unique case (funct3)
                    FUNCT3_ADD_SUB: decoded_o.alu_op = ALU_ADD;
                    FUNCT3_SLL: begin
                        decoded_o.alu_op = ALU_SLL;
                        if (funct7 != 7'h00) decoded_o.illegal = 1'b1;
                    end
                    FUNCT3_SLT: decoded_o.alu_op = ALU_SLT;
                    FUNCT3_SLTU: decoded_o.alu_op = ALU_SLTU;
                    FUNCT3_XOR: decoded_o.alu_op = ALU_XOR;
                    FUNCT3_SRL_SRA: begin
                        decoded_o.alu_op = funct7[5] ? ALU_SRA : ALU_SRL;
                        if (funct7 != 7'h00 && funct7 != 7'h20) decoded_o.illegal = 1'b1;
                    end
                    FUNCT3_OR: decoded_o.alu_op = ALU_OR;
                    FUNCT3_AND: decoded_o.alu_op = ALU_AND;
                    default: decoded_o.illegal = 1'b1;
                endcase
            end
            OPCODE_LOAD: begin
                decoded_o.fu_type = FU_LOAD;
                decoded_o.rs1_use = 1'b1;
                decoded_o.rd_to_write = (rd != 5'd0);
                decoded_o.imm = sext12(inst_i[31:20]);
                decoded_o.alu_op2_sel = ALU_OP2_IMM;
                unique case (funct3)
                    FUNCT3_LB:  begin decoded_o.mem_len = MEM_LEN_1; decoded_o.load_signext = 1'b1; end
                    FUNCT3_LH:  begin decoded_o.mem_len = MEM_LEN_2; decoded_o.load_signext = 1'b1; end
                    FUNCT3_LW:  begin decoded_o.mem_len = MEM_LEN_4; decoded_o.load_signext = 1'b1; end
                    FUNCT3_LBU: begin decoded_o.mem_len = MEM_LEN_1; decoded_o.load_signext = 1'b0; end
                    FUNCT3_LHU: begin decoded_o.mem_len = MEM_LEN_2; decoded_o.load_signext = 1'b0; end
                    default: decoded_o.illegal = 1'b1;
                endcase
            end
            OPCODE_STORE: begin
                decoded_o.fu_type = FU_STORE;
                decoded_o.rs1_use = 1'b1;
                decoded_o.rs2_use = 1'b1;
                decoded_o.imm = {{20{inst_i[31]}}, inst_i[31:25], inst_i[11:7]};
                decoded_o.alu_op2_sel = ALU_OP2_IMM;
                unique case (funct3)
                    FUNCT3_SB: decoded_o.mem_len = MEM_LEN_1;
                    FUNCT3_SH: decoded_o.mem_len = MEM_LEN_2;
                    FUNCT3_SW: decoded_o.mem_len = MEM_LEN_4;
                    default: decoded_o.illegal = 1'b1;
                endcase
            end
            OPCODE_BRANCH: begin
                decoded_o.fu_type = FU_BRANCH;
                decoded_o.rs1_use = 1'b1;
                decoded_o.rs2_use = 1'b1;
                decoded_o.imm = {{19{inst_i[31]}}, inst_i[31], inst_i[7], inst_i[30:25], inst_i[11:8], 1'b0};
                unique case (funct3)
                    FUNCT3_BEQ:  decoded_o.branch_op = BR_EQ;
                    FUNCT3_BNE:  decoded_o.branch_op = BR_NE;
                    FUNCT3_BLT:  decoded_o.branch_op = BR_LT;
                    FUNCT3_BGE:  decoded_o.branch_op = BR_GE;
                    FUNCT3_BLTU: decoded_o.branch_op = BR_LTU;
                    FUNCT3_BGEU: decoded_o.branch_op = BR_GEU;
                    default: decoded_o.illegal = 1'b1;
                endcase
            end
            OPCODE_JAL: begin
                decoded_o.fu_type = FU_BRANCH;
                decoded_o.imm = {{11{inst_i[31]}}, inst_i[31], inst_i[19:12], inst_i[20], inst_i[30:21], 1'b0};
                decoded_o.branch_op = BR_JUMP;
                decoded_o.rd_to_write = (rd != 5'd0);
            end
            OPCODE_JALR: begin
                decoded_o.fu_type = FU_BRANCH;
                decoded_o.rs1_use = 1'b1;
                decoded_o.imm = sext12(inst_i[31:20]);
                decoded_o.branch_op = BR_JUMP;
                decoded_o.rd_to_write = (rd != 5'd0);
                if (funct3 != 3'h0) decoded_o.illegal = 1'b1;
            end
            OPCODE_AUIPC: begin
                decoded_o.imm = {inst_i[31:12], 12'd0};
                decoded_o.alu_op1_sel = ALU_OP1_PC;
                decoded_o.alu_op2_sel = ALU_OP2_IMM;
                decoded_o.rd_to_write = (rd != 5'd0);
            end
            OPCODE_LUI: begin
                decoded_o.imm = {inst_i[31:12], 12'd0};
                decoded_o.alu_op1_sel = ALU_OP1_ZERO;
                decoded_o.alu_op2_sel = ALU_OP2_IMM;
                decoded_o.rd_to_write = (rd != 5'd0);
            end
            OPCODE_MISC_MEM: begin
                if (funct3 != 3'h0) decoded_o.illegal = 1'b1;
            end
            OPCODE_SYSTEM: begin
                decoded_o.fu_type = FU_SYSTEM;
                if (funct3 == FUNCT3_SYSTEM_PRIV) begin
                    if (rs1 != 5'd0 || rd != 5'd0) decoded_o.illegal = 1'b1;
                    unique case (funct12)
                        FUNCT12_ECALL:  decoded_o.sys_kind = SYS_ECALL;
                        FUNCT12_EBREAK: decoded_o.sys_kind = SYS_EBREAK;
                        FUNCT12_MRET:   decoded_o.sys_kind = SYS_MRET;
                        FUNCT12_WFI:    decoded_o.sys_kind = SYS_WFI;
                        default: decoded_o.illegal = 1'b1;
                    endcase
                end else if (funct3 == 3'h4) begin
                    decoded_o.illegal = 1'b1;
                end else begin
                    decoded_o.fu_type = FU_CSR;
                    decoded_o.csr_addr = funct12;
                    decoded_o.rd_to_write = (rd != 5'd0);
                    unique case (funct3)
                        FUNCT3_CSRRW: begin
                            decoded_o.rs1_use = 1'b1;
                            decoded_o.sys_kind = SYS_CSR_RW;
                            decoded_o.csr_writes = 1'b1;
                        end
                        FUNCT3_CSRRS: begin
                            decoded_o.rs1_use = 1'b1;
                            decoded_o.sys_kind = SYS_CSR_RS;
                            decoded_o.csr_writes = (rs1 != 5'd0);
                        end
                        FUNCT3_CSRRC: begin
                            decoded_o.rs1_use = 1'b1;
                            decoded_o.sys_kind = SYS_CSR_RC;
                            decoded_o.csr_writes = (rs1 != 5'd0);
                        end
                        FUNCT3_CSRRWI: begin
                            decoded_o.sys_kind = SYS_CSR_RW;
                            decoded_o.csr_source_is_imm = 1'b1;
                            decoded_o.csr_source_imm = {27'd0, rs1};
                            decoded_o.csr_writes = 1'b1;
                        end
                        FUNCT3_CSRRSI: begin
                            decoded_o.sys_kind = SYS_CSR_RS;
                            decoded_o.csr_source_is_imm = 1'b1;
                            decoded_o.csr_source_imm = {27'd0, rs1};
                            decoded_o.csr_writes = (rs1 != 5'd0);
                        end
                        FUNCT3_CSRRCI: begin
                            decoded_o.sys_kind = SYS_CSR_RC;
                            decoded_o.csr_source_is_imm = 1'b1;
                            decoded_o.csr_source_imm = {27'd0, rs1};
                            decoded_o.csr_writes = (rs1 != 5'd0);
                        end
                        default: decoded_o.illegal = 1'b1;
                    endcase
                end
            end
            default: decoded_o.illegal = 1'b1;
        endcase
    end
endmodule
