`include "uarch.svh"

module CSRFile (
    input  logic                    clk_i,
    input  logic                    rst_i,
    input  logic                    msip_i,
    input  logic                    mtip_i,
    input  logic                    meip_i,
    input  logic                    csr_valid_i,
    input  uarch_pkg::system_kind_e csr_op_i,
    input  logic [11:0]             csr_addr_i,
    input  logic [31:0]             csr_source_i,
    input  logic                    csr_writes_i,
    output logic [31:0]             csr_old_o,
    output logic [31:0]             csr_new_o,
    output logic                    csr_illegal_o,
    input  logic                    trap_entry_i,
    input  logic [31:0]             trap_cause_i,
    input  logic [31:0]             trap_epc_i,
    input  logic [31:0]             trap_tval_i,
    input  logic                    mret_i,
    output logic [31:0]             mstatus_o,
    output logic [31:0]             mtvec_o,
    output logic [31:0]             mepc_o,
    output logic [31:0]             mcause_o,
    output logic [31:0]             mtval_o,
    output logic [31:0]             mscratch_o,
    output logic [31:0]             mie_o,
    output logic [31:0]             mip_o,
    output logic [31:0]             mtvec_base_o
);
    import riscv_pkg::*;
    import uarch_pkg::*;

    logic [31:0] mstatus_q, mie_q, mtvec_q, mscratch_q, mepc_q, mcause_q, mtval_q;

    assign mip_o = (msip_i ? MIP_MSIP : 32'd0) |
                   (mtip_i ? MIP_MTIP : 32'd0) |
                   (meip_i ? MIP_MEIP : 32'd0);
    assign mstatus_o = (mstatus_q & MSTATUS_MASK) | MSTATUS_MPP_M;
    assign mie_o = mie_q & MIE_MASK;
    assign mtvec_o = mtvec_q & MTVEC_MASK;
    assign mscratch_o = mscratch_q;
    assign mepc_o = mepc_q & MEPC_MASK;
    assign mcause_o = mcause_q;
    assign mtval_o = mtval_q;
    assign mtvec_base_o = mtvec_q & MTVEC_MASK;

    function automatic logic recognised(input logic [11:0] addr);
        unique case (addr)
            CSR_MSTATUS, CSR_MISA, CSR_MIE, CSR_MTVEC, CSR_MSCRATCH,
            CSR_MEPC, CSR_MCAUSE, CSR_MTVAL, CSR_MIP,
            CSR_MVENDORID, CSR_MARCHID, CSR_MIMPID, CSR_MHARTID: return 1'b1;
            default: return 1'b0;
        endcase
    endfunction

    function automatic logic writable(input logic [11:0] addr);
        return recognised(addr) && addr[11:10] != 2'b11;
    endfunction

    function automatic logic [31:0] read_csr(input logic [11:0] addr);
        unique case (addr)
            CSR_MSTATUS:  return mstatus_o;
            CSR_MISA:     return MISA_VALUE;
            CSR_MIE:      return mie_o;
            CSR_MTVEC:    return mtvec_o;
            CSR_MSCRATCH: return mscratch_q;
            CSR_MEPC:     return mepc_o;
            CSR_MCAUSE:   return mcause_q;
            CSR_MTVAL:    return mtval_q;
            CSR_MIP:      return mip_o;
            CSR_MVENDORID,
            CSR_MARCHID,
            CSR_MIMPID,
            CSR_MHARTID:  return 32'd0;
            default:      return 32'd0;
        endcase
    endfunction

    function automatic logic [31:0] mask_write(input logic [11:0] addr, input logic [31:0] value);
        unique case (addr)
            CSR_MSTATUS: return value & MSTATUS_MASK;
            CSR_MIE:     return value & MIE_MASK;
            CSR_MTVEC:   return value & MTVEC_MASK;
            CSR_MEPC:    return value & MEPC_MASK;
            default:     return value;
        endcase
    endfunction

    always_comb begin
        csr_old_o = read_csr(csr_addr_i);
        csr_new_o = csr_old_o;
        csr_illegal_o = csr_valid_i && (!recognised(csr_addr_i) || (csr_writes_i && !writable(csr_addr_i)));

        unique case (csr_op_i)
            SYS_CSR_RW: csr_new_o = csr_source_i;
            SYS_CSR_RS: csr_new_o = csr_old_o | csr_source_i;
            SYS_CSR_RC: csr_new_o = csr_old_o & ~csr_source_i;
            default:    csr_new_o = csr_old_o;
        endcase
    end

    always_ff @(posedge clk_i) begin
        if (rst_i) begin
            mstatus_q <= 32'd0;
            mie_q <= 32'd0;
            mtvec_q <= 32'd0;
            mscratch_q <= 32'd0;
            mepc_q <= 32'd0;
            mcause_q <= 32'd0;
            mtval_q <= 32'd0;
        end else if (trap_entry_i) begin
            mepc_q <= trap_epc_i & MEPC_MASK;
            mcause_q <= trap_cause_i;
            mtval_q <= trap_tval_i;
            mstatus_q <= ((mstatus_q & MSTATUS_MIE) != 32'd0) ? MSTATUS_MPIE : 32'd0;
        end else begin
            if (csr_valid_i && csr_writes_i && !csr_illegal_o) begin
                unique case (csr_addr_i)
                    CSR_MSTATUS:  mstatus_q <= mask_write(csr_addr_i, csr_new_o);
                    CSR_MIE:      mie_q <= mask_write(csr_addr_i, csr_new_o);
                    CSR_MTVEC:    mtvec_q <= mask_write(csr_addr_i, csr_new_o);
                    CSR_MSCRATCH: mscratch_q <= csr_new_o;
                    CSR_MEPC:     mepc_q <= mask_write(csr_addr_i, csr_new_o);
                    CSR_MCAUSE:   mcause_q <= csr_new_o;
                    CSR_MTVAL:    mtval_q <= csr_new_o;
                    default:      mstatus_q <= mstatus_q;
                endcase
            end

            if (mret_i) begin
                mstatus_q <= MSTATUS_MPIE |
                             (((mstatus_q & MSTATUS_MPIE) != 32'd0) ? MSTATUS_MIE : 32'd0);
            end
        end
    end
endmodule
