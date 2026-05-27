/* verilator lint_off DECLFILENAME */
`ifndef DAE_PIPELINE_CPU_UARCH_SVH
`define DAE_PIPELINE_CPU_UARCH_SVH

`include "riscv.svh"

package uarch_pkg;
    import riscv_pkg::*;

    parameter int EPOCH_W = 2;
    parameter int SEQ_ID_W = 8;

    typedef logic [EPOCH_W-1:0] epoch_t;
    typedef logic [SEQ_ID_W-1:0] seq_id_t;

    typedef enum logic [2:0] {
        FU_INT    = 3'd0,
        FU_BRANCH = 3'd1,
        FU_CSR    = 3'd2,
        FU_LOAD   = 3'd3,
        FU_STORE  = 3'd4,
        FU_SYSTEM = 3'd5
    } fu_type_e;

    typedef enum logic [3:0] {
        ALU_ADD  = 4'd0,
        ALU_SLL  = 4'd1,
        ALU_SLT  = 4'd2,
        ALU_SLTU = 4'd3,
        ALU_XOR  = 4'd4,
        ALU_SRL  = 4'd5,
        ALU_OR   = 4'd6,
        ALU_AND  = 4'd7,
        ALU_SUB  = 4'd8,
        ALU_SRA  = 4'd9
    } alu_op_e;

    typedef enum logic [1:0] {
        ALU_OP1_RS1  = 2'd0,
        ALU_OP1_PC   = 2'd1,
        ALU_OP1_ZERO = 2'd2
    } alu_op1_sel_e;

    typedef enum logic [0:0] {
        ALU_OP2_RS2 = 1'd0,
        ALU_OP2_IMM = 1'd1
    } alu_op2_sel_e;

    typedef enum logic [2:0] {
        BR_JUMP = 3'd0,
        BR_EQ   = 3'd1,
        BR_NE   = 3'd2,
        BR_LT   = 3'd3,
        BR_LTU  = 3'd4,
        BR_GE   = 3'd5,
        BR_GEU  = 3'd6
    } branch_op_e;

    typedef enum logic [2:0] {
        MEM_NONE = 3'd0,
        MEM_LB   = 3'd1,
        MEM_LH   = 3'd2,
        MEM_LW   = 3'd3,
        MEM_LBU  = 3'd4,
        MEM_LHU  = 3'd5,
        MEM_SB   = 3'd6,
        MEM_SH   = 3'd7
    } mem_op_e;

    typedef enum logic [1:0] {
        MEM_LEN_1 = 2'd0,
        MEM_LEN_2 = 2'd1,
        MEM_LEN_4 = 2'd2
    } mem_len_e;

    typedef enum logic [2:0] {
        SYS_NOT    = 3'd0,
        SYS_ECALL  = 3'd1,
        SYS_EBREAK = 3'd2,
        SYS_MRET   = 3'd3,
        SYS_WFI    = 3'd4,
        SYS_CSR_RW = 3'd5,
        SYS_CSR_RS = 3'd6,
        SYS_CSR_RC = 3'd7
    } system_kind_e;

    typedef enum logic [3:0] {
        HALT_NONE         = 4'd0,
        HALT_VOLUNTARY    = 4'd1,
        HALT_ILLEGAL      = 4'd2,
        HALT_BUS_ERROR_IF = 4'd3,
        HALT_BUS_ERROR_LD = 4'd4,
        HALT_BUS_ERROR_ST = 4'd5,
        HALT_MISALIGN_PC  = 4'd6,
        HALT_MISALIGN_LD  = 4'd7,
        HALT_MISALIGN_ST  = 4'd8,
        HALT_DOUBLE_TRAP  = 4'd9
    } halt_kind_e;

    typedef struct packed {
        logic        req_valid;
        logic [31:0] addr;
        logic        we;
        logic [3:0]  be;
        logic [31:0] wdata;
    } mem_req_t;

    typedef struct packed {
        logic        resp_valid;
        logic [31:0] rdata;
        logic        error;
    } mem_resp_t;

    typedef struct packed {
        logic        valid;
        epoch_t      epoch;
        seq_id_t     seq_id;
        logic [31:0] pc;
        logic [31:0] inst;
        logic        error;
    } fetch_pkt_t;

    typedef struct packed {
        logic [31:0] pc;
        logic [31:0] rd_value;
        logic [4:0]  rd;
        logic        rd_to_write;
        logic        store_valid;
        logic [31:0] store_addr;
        logic [31:0] store_data;
        logic [3:0]  store_strb;
        logic        halt_observed;
        halt_kind_e  halt_kind;
        logic [7:0]  halt_exit_code;
        logic        trap_taken;
        logic [31:0] trap_cause;
        logic [31:0] trap_mepc;
        logic [31:0] trap_mtval;
        logic [31:0] csr_mstatus;
        logic [31:0] csr_mtvec;
        logic [31:0] csr_mepc;
        logic [31:0] csr_mcause;
        logic [31:0] csr_mtval;
        logic [31:0] csr_mscratch;
        logic [31:0] csr_mie;
    } commit_packet_t;

    typedef struct packed {
        epoch_t      epoch;
        seq_id_t     seq_id;
        logic [31:0] pc;
        logic [31:0] raw_inst;
        logic        if_bus_error;
        fu_type_e    fu_type;
        alu_op_e     alu_op;
        alu_op1_sel_e alu_op1_sel;
        alu_op2_sel_e alu_op2_sel;
        branch_op_e  branch_op;
        system_kind_e sys_kind;
        mem_len_e    mem_len;
        logic        load_signext;
        logic        rs1_use;
        logic [4:0]  rs1_idx;
        logic        rs2_use;
        logic [4:0]  rs2_idx;
        logic        rd_to_write;
        logic [4:0]  rd_idx;
        logic [31:0] imm;
        logic [11:0] csr_addr;
        logic [31:0] csr_source_imm;
        logic        csr_source_is_imm;
        logic        csr_writes;
        logic        illegal;
    } decoded_ir_t;

    typedef struct packed {
        logic        valid;
        epoch_t      epoch;
        seq_id_t     seq_id;
        decoded_ir_t ir;
        logic [31:0] rs1_val;
        logic [31:0] rs2_val;
        logic [31:0] alu_result;
        logic [31:0] rd_value;
        logic [31:0] mem_addr;
        logic [31:0] store_addr;
        logic [31:0] store_data;
        logic [3:0]  store_strb;
        logic        csr_illegal;
        logic [31:0] csr_old;
        logic [31:0] csr_new;
        logic        killed;
        logic        trap_pending;
        logic [31:0] trap_cause;
        logic [31:0] trap_tval;
        logic        halt_observed;
        halt_kind_e  halt_kind;
    } inst_token_t;

    typedef struct packed {
        logic        valid;
        logic        rs1_use;
        logic [4:0]  rs1_idx;
        logic        rs2_use;
        logic [4:0]  rs2_idx;
        logic        rd_to_write;
        logic [4:0]  rd_idx;
        logic        rd_ready;
        logic        rd_written_back;
    } scoreboard_row_t;

    function automatic logic is_load(input decoded_ir_t ir);
        return ir.fu_type == FU_LOAD;
    endfunction

    function automatic logic is_store(input decoded_ir_t ir);
        return ir.fu_type == FU_STORE;
    endfunction

    function automatic logic is_mem(input decoded_ir_t ir);
        return is_load(ir) || is_store(ir);
    endfunction

    function automatic logic [7:0] halt_exit_code(
        input halt_kind_e  kind,
        input logic [31:0] a0
    );
        unique case (kind)
            HALT_VOLUNTARY:    return a0[7:0];
            HALT_ILLEGAL:      return 8'd130;
            HALT_BUS_ERROR_IF: return 8'd131;
            HALT_BUS_ERROR_LD: return 8'd132;
            HALT_BUS_ERROR_ST: return 8'd133;
            HALT_MISALIGN_PC,
            HALT_MISALIGN_LD,
            HALT_MISALIGN_ST:  return 8'd134;
            HALT_DOUBLE_TRAP:  return 8'd135;
            default:           return 8'd1;
        endcase
    endfunction
endpackage : uarch_pkg

`endif
/* verilator lint_on DECLFILENAME */
