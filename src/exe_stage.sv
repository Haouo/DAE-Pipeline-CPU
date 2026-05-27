`include "uarch.svh"

module dae_exe_stage (
    input  uarch_pkg::inst_token_t exe_i,
    input  logic                 mem_stall_i,
    input  logic                 trap_pending_i,
    input  logic [31:0]          csr_mepc_i,
    input  logic [31:0]          csr_old_i,
    input  logic [31:0]          csr_new_i,
    input  logic                 csr_illegal_i,

    output uarch_pkg::inst_token_t exe_o,
    output logic                 redirect_valid_o,
    output logic [31:0]          redirect_pc_o,
    output logic                 csr_valid_o,
    output logic                 csr_commit_o,
    output logic                 mret_commit_o,
    output logic [31:0]          csr_source_o,
    output logic                 rd_ready_o
);
    import riscv_pkg::*;
    import uarch_pkg::*;

    logic [31:0] alu_lhs, alu_rhs, alu_result;
    logic branch_taken, branch_mispredict;
    logic [31:0] branch_target, link_val, actual_redirect_pc;
    logic token_alive;

    assign token_alive = exe_i.valid && !exe_i.killed;

    always_comb begin
        unique case (exe_i.ir.alu_op1_sel)
            ALU_OP1_PC:   alu_lhs = exe_i.ir.pc;
            ALU_OP1_ZERO: alu_lhs = 32'd0;
            default:      alu_lhs = exe_i.rs1_val;
        endcase
        alu_rhs = (exe_i.ir.alu_op2_sel == ALU_OP2_IMM) ? exe_i.ir.imm : exe_i.rs2_val;
        csr_source_o = exe_i.ir.csr_source_is_imm ? exe_i.ir.csr_source_imm : exe_i.rs1_val;
    end

    alu u_alu (
        .op_i(exe_i.ir.alu_op),
        .lhs_i(alu_lhs),
        .rhs_i(alu_rhs),
        .result_o(alu_result)
    );

    branch_unit u_branch (
        .op(exe_i.ir.branch_op),
        .pc(exe_i.ir.pc),
        .rs1_val(exe_i.rs1_val),
        .rs2_val(exe_i.rs2_val),
        .imm(exe_i.ir.imm),
        .taken(branch_taken),
        .target_pc(branch_target),
        .link_val(link_val),
        .mispredict(branch_mispredict)
    );

    assign actual_redirect_pc = (exe_i.ir.raw_inst[6:0] == OPCODE_JALR) ?
                                ((exe_i.rs1_val + exe_i.ir.imm) & ~32'd1) :
                                branch_target;

    assign csr_valid_o = token_alive && exe_i.ir.fu_type == FU_CSR;
    assign csr_commit_o = csr_valid_o && !mem_stall_i && !trap_pending_i;
    assign mret_commit_o = token_alive && exe_i.ir.sys_kind == SYS_MRET &&
                           !mem_stall_i && !trap_pending_i;
    assign rd_ready_o = token_alive && exe_i.ir.rd_to_write && !is_load(exe_i.ir);

    assign redirect_valid_o = token_alive && !mem_stall_i && !trap_pending_i &&
                              ((exe_i.ir.fu_type == FU_BRANCH && branch_mispredict) ||
                               (exe_i.ir.sys_kind == SYS_MRET));
    assign redirect_pc_o = (exe_i.ir.sys_kind == SYS_MRET) ? csr_mepc_i : actual_redirect_pc;

    always_comb begin
        exe_o = exe_i;
        exe_o.alu_result = alu_result;
        exe_o.mem_addr = alu_result;
        exe_o.store_data = exe_i.rs2_val;
        exe_o.csr_old = csr_old_i;
        exe_o.csr_new = csr_new_i;
        exe_o.csr_illegal = csr_illegal_i;

        if (exe_i.ir.fu_type == FU_BRANCH) begin
            exe_o.rd_value = link_val;
            if (exe_i.ir.raw_inst[6:0] == OPCODE_JALR) begin
                exe_o.mem_addr = (exe_i.rs1_val + exe_i.ir.imm) & ~32'd1;
            end
        end else if (exe_i.ir.fu_type == FU_CSR) begin
            exe_o.rd_value = csr_old_i;
            if (csr_illegal_i) exe_o.ir.illegal = 1'b1;
        end else begin
            exe_o.rd_value = alu_result;
        end

        if (token_alive && exe_i.ir.fu_type == FU_BRANCH && branch_taken) begin
            if (actual_redirect_pc[1:0] != 2'b00) begin
                exe_o.halt_observed = 1'b1;
                exe_o.halt_kind = HALT_MISALIGN_PC;
            end
        end
    end
endmodule
