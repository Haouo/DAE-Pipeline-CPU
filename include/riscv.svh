/* verilator lint_off DECLFILENAME */
`ifndef DAE_PIPELINE_CPU_RISCV_SVH
`define DAE_PIPELINE_CPU_RISCV_SVH

package riscv_pkg;
    typedef enum logic [6:0] {
        OPCODE_LOAD     = 7'h03,
        OPCODE_MISC_MEM = 7'h0f,
        OPCODE_OP_IMM   = 7'h13,
        OPCODE_AUIPC    = 7'h17,
        OPCODE_STORE    = 7'h23,
        OPCODE_OP       = 7'h33,
        OPCODE_LUI      = 7'h37,
        OPCODE_BRANCH   = 7'h63,
        OPCODE_JALR     = 7'h67,
        OPCODE_JAL      = 7'h6f,
        OPCODE_SYSTEM   = 7'h73
    } opcode_e;

    localparam logic [2:0] FUNCT3_ADD_SUB = 3'h0;
    localparam logic [2:0] FUNCT3_SLL     = 3'h1;
    localparam logic [2:0] FUNCT3_SLT     = 3'h2;
    localparam logic [2:0] FUNCT3_SLTU    = 3'h3;
    localparam logic [2:0] FUNCT3_XOR     = 3'h4;
    localparam logic [2:0] FUNCT3_SRL_SRA = 3'h5;
    localparam logic [2:0] FUNCT3_OR      = 3'h6;
    localparam logic [2:0] FUNCT3_AND     = 3'h7;

    localparam logic [2:0] FUNCT3_BEQ  = 3'h0;
    localparam logic [2:0] FUNCT3_BNE  = 3'h1;
    localparam logic [2:0] FUNCT3_BLT  = 3'h4;
    localparam logic [2:0] FUNCT3_BGE  = 3'h5;
    localparam logic [2:0] FUNCT3_BLTU = 3'h6;
    localparam logic [2:0] FUNCT3_BGEU = 3'h7;

    localparam logic [2:0] FUNCT3_LB  = 3'h0;
    localparam logic [2:0] FUNCT3_LH  = 3'h1;
    localparam logic [2:0] FUNCT3_LW  = 3'h2;
    localparam logic [2:0] FUNCT3_LBU = 3'h4;
    localparam logic [2:0] FUNCT3_LHU = 3'h5;

    localparam logic [2:0] FUNCT3_SB = 3'h0;
    localparam logic [2:0] FUNCT3_SH = 3'h1;
    localparam logic [2:0] FUNCT3_SW = 3'h2;

    localparam logic [2:0] FUNCT3_SYSTEM_PRIV = 3'h0;
    localparam logic [2:0] FUNCT3_CSRRW       = 3'h1;
    localparam logic [2:0] FUNCT3_CSRRS       = 3'h2;
    localparam logic [2:0] FUNCT3_CSRRC       = 3'h3;
    localparam logic [2:0] FUNCT3_CSRRWI      = 3'h5;
    localparam logic [2:0] FUNCT3_CSRRSI      = 3'h6;
    localparam logic [2:0] FUNCT3_CSRRCI      = 3'h7;

    localparam logic [11:0] FUNCT12_ECALL  = 12'h000;
    localparam logic [11:0] FUNCT12_EBREAK = 12'h001;
    localparam logic [11:0] FUNCT12_MRET   = 12'h302;
    localparam logic [11:0] FUNCT12_WFI    = 12'h105;

    localparam logic [11:0] CSR_MSTATUS   = 12'h300;
    localparam logic [11:0] CSR_MISA      = 12'h301;
    localparam logic [11:0] CSR_MIE       = 12'h304;
    localparam logic [11:0] CSR_MTVEC     = 12'h305;
    localparam logic [11:0] CSR_MSCRATCH  = 12'h340;
    localparam logic [11:0] CSR_MEPC      = 12'h341;
    localparam logic [11:0] CSR_MCAUSE    = 12'h342;
    localparam logic [11:0] CSR_MTVAL     = 12'h343;
    localparam logic [11:0] CSR_MIP       = 12'h344;
    localparam logic [11:0] CSR_MVENDORID = 12'hf11;
    localparam logic [11:0] CSR_MARCHID   = 12'hf12;
    localparam logic [11:0] CSR_MIMPID    = 12'hf13;
    localparam logic [11:0] CSR_MHARTID   = 12'hf14;

    localparam logic [31:0] MISA_VALUE    = 32'h4000_0100;
    localparam logic [31:0] MSTATUS_MIE   = 32'h0000_0008;
    localparam logic [31:0] MSTATUS_MPIE  = 32'h0000_0080;
    localparam logic [31:0] MSTATUS_MPP_M = 32'h0000_1800;
    localparam logic [31:0] MSTATUS_MASK  = MSTATUS_MIE | MSTATUS_MPIE;
    localparam logic [31:0] MIE_MSIE      = 32'h0000_0008;
    localparam logic [31:0] MIE_MTIE      = 32'h0000_0080;
    localparam logic [31:0] MIE_MEIE      = 32'h0000_0800;
    localparam logic [31:0] MIE_MASK      = MIE_MSIE | MIE_MTIE | MIE_MEIE;
    localparam logic [31:0] MTVEC_MASK    = 32'hffff_fffc;
    localparam logic [31:0] MEPC_MASK     = 32'hffff_fffc;
    localparam logic [31:0] MIP_MSIP      = 32'h0000_0008;
    localparam logic [31:0] MIP_MTIP      = 32'h0000_0080;
    localparam logic [31:0] MIP_MEIP      = 32'h0000_0800;
endpackage : riscv_pkg

`endif
/* verilator lint_on DECLFILENAME */
