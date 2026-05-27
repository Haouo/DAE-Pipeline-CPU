`include "uarch.svh"

module tb_top #(
    parameter bit MEM_FORWARDING = 1'b1,
    parameter bit WB_FORWARDING  = 1'b1
);
    import uarch_pkg::*;

    import "DPI-C" function int dpi_snake_soc_init(input string elf_path);
    import "DPI-C" function void dpi_snake_soc_fini();
    import "DPI-C" function void dpi_snake_soc_tick();
    import "DPI-C" function int dpi_snake_soc_read(input int unsigned addr, input int width, output int unsigned value);
    import "DPI-C" function int dpi_snake_soc_write(input int unsigned addr, input int width, input int unsigned value, input byte unsigned strb);
    import "DPI-C" function int dpi_snake_soc_msip();
    import "DPI-C" function int dpi_snake_soc_mtip();
    import "DPI-C" function int dpi_snake_soc_meip();

    localparam int MEM_WORDS = 1 << 16;
    localparam int INDEX_W = 16;

    logic clk;
    logic rst;
    /* verilator lint_off UNUSEDSIGNAL */
    mem_req_t imem_req;
    /* verilator lint_on UNUSEDSIGNAL */
    mem_resp_t imem_resp;
    mem_req_t dmem_req;
    mem_resp_t dmem_resp;
    logic commit_valid;
    /* verilator lint_off UNUSEDSIGNAL */
    commit_packet_t commit;
    /* verilator lint_on UNUSEDSIGNAL */
    logic [31:0] dram [MEM_WORDS];
    string hex_path;
    string elf_path;
    int max_cycles;
    bit done;
    bit use_dpi;
    bit quiet;
    bit allow_timeout;
    logic msip;
    logic mtip;
    logic meip;

    Top #(
        .MEM_FORWARDING(MEM_FORWARDING),
        .WB_FORWARDING(WB_FORWARDING)
    ) dut (
        .clk_i(clk),
        .rst_i(rst),
        .imem_req_o(imem_req),
        .imem_req_ready_i(1'b1),
        .imem_resp_i(imem_resp),
        .dmem_req_o(dmem_req),
        .dmem_req_ready_i(1'b1),
        .dmem_resp_i(dmem_resp),
        .msip_i(msip),
        .mtip_i(mtip),
        .meip_i(meip),
        .commit_valid_o(commit_valid),
        .commit_o(commit)
    );

    initial begin
        clk = 1'b0;
        forever #5 clk = ~clk;
    end

    initial begin
        for (int i = 0; i < MEM_WORDS; i++) begin
            dram[i] = 32'd0;
        end
        if ($value$plusargs("HEX=%s", hex_path)) begin
            $readmemh(hex_path, dram);
        end
        use_dpi = $value$plusargs("ELF=%s", elf_path);
        if (use_dpi && dpi_snake_soc_init(elf_path) != 0) begin
            $fatal(1, "dpi_snake_soc_init failed");
        end
        if (!$value$plusargs("MAX_CYCLES=%d", max_cycles)) begin
            max_cycles = 1000;
        end
        quiet = $test$plusargs("QUIET");
        allow_timeout = $test$plusargs("ALLOW_TIMEOUT");

        rst = 1'b1;
        repeat (3) @(posedge clk);
        rst = 1'b0;

        done = 1'b0;
        for (int cyc = 0; cyc < max_cycles && !done; cyc++) begin
            @(posedge clk);
            if (commit_valid && !quiet) begin
                $display("commit pc=%08x rd=%0d rd_value=%08x store=%0d store_addr=%08x store_data=%08x store_strb=%x trap=%0d cause=%08x halt=%0d halt_kind=%0d exit=%0d",
                         commit.pc,
                         commit.rd,
                         commit.rd_value,
                         commit.store_valid,
                         commit.store_addr,
                         commit.store_data,
                         commit.store_strb,
                         commit.trap_taken,
                         commit.trap_cause,
                         commit.halt_observed,
                         commit.halt_kind,
                         commit.halt_exit_code);
            end
            if (commit_valid && commit.halt_observed) begin
                done = 1'b1;
            end
        end

        if (!done && allow_timeout) begin
            $display("timeout after %0d cycles (allowed)", max_cycles);
        end else if (!done) begin
            $fatal(1, "timeout after %0d cycles", max_cycles);
        end
        if (use_dpi) begin
            dpi_snake_soc_fini();
        end
        $finish;
    end

    always_ff @(posedge clk) begin
        int unsigned dpi_rdata;
        int dpi_resp;
        int dpi_width;
        int unsigned dpi_addr;
        int unsigned dpi_wdata;
        byte unsigned dpi_strb;

        if (rst) begin
            imem_resp <= '0;
            dmem_resp <= '0;
            msip <= 1'b0;
            mtip <= 1'b0;
            meip <= 1'b0;
        end else begin
            if (use_dpi) begin
                if (commit_valid) begin
                    dpi_snake_soc_tick();
                end
                msip <= dpi_snake_soc_msip() != 0;
                mtip <= dpi_snake_soc_mtip() != 0;
                meip <= dpi_snake_soc_meip() != 0;

                dpi_rdata = 32'd0;
                dpi_resp = imem_req.req_valid ?
                           dpi_snake_soc_read(imem_req.addr, 4, dpi_rdata) : 0;
                imem_resp.resp_valid <= imem_req.req_valid;
                imem_resp.rdata <= dpi_rdata;
                imem_resp.error <= dpi_resp != 0;

                dpi_rdata = 32'd0;
                if (dmem_req.req_valid && dmem_req.we) begin
                    dpi_addr = dmem_req.addr;
                    dpi_wdata = dmem_req.wdata;
                    dpi_strb = {4'd0, dmem_req.be};
                    unique case (dmem_req.be)
                        4'b0001: begin dpi_width = 1; dpi_strb = 8'h1; end
                        4'b0010: begin dpi_width = 1; dpi_wdata = dmem_req.wdata >> 8; dpi_strb = 8'h1; end
                        4'b0100: begin dpi_width = 1; dpi_wdata = dmem_req.wdata >> 16; dpi_strb = 8'h1; end
                        4'b1000: begin dpi_width = 1; dpi_wdata = dmem_req.wdata >> 24; dpi_strb = 8'h1; end
                        4'b0011: begin dpi_width = 2; dpi_strb = 8'h3; end
                        4'b1100: begin dpi_width = 2; dpi_wdata = dmem_req.wdata >> 16; dpi_strb = 8'h3; end
                        default: begin dpi_width = 4; dpi_strb = 8'hf; end
                    endcase
                    dpi_resp = dpi_snake_soc_write(dpi_addr, dpi_width, dpi_wdata, dpi_strb);
                end else if (dmem_req.req_valid) begin
                    dpi_resp = dpi_snake_soc_read(dmem_req.addr & 32'hffff_fffc, 4, dpi_rdata);
                end else begin
                    dpi_resp = 0;
                end
                dmem_resp.resp_valid <= dmem_req.req_valid;
                dmem_resp.rdata <= dpi_rdata;
                dmem_resp.error <= dpi_resp != 0;
            end else begin
                msip <= 1'b0;
                mtip <= 1'b0;
                meip <= 1'b0;

                imem_resp.resp_valid <= imem_req.req_valid;
                unique case (imem_req.addr)
                    32'h0000_0000: imem_resp.rdata <= 32'h8000_00b7;
                    32'h0000_0004: imem_resp.rdata <= 32'h0000_8067;
                    default:       imem_resp.rdata <= dram[imem_req.addr[INDEX_W+1:2]];
                endcase
                imem_resp.error <= 1'b0;

                dmem_resp.resp_valid <= dmem_req.req_valid;
                dmem_resp.rdata <= dram[dmem_req.addr[INDEX_W+1:2]];
                dmem_resp.error <= 1'b0;
            end
        end

        if (!rst && !use_dpi && dmem_req.req_valid && dmem_req.we) begin
            if (dmem_req.be[0]) dram[dmem_req.addr[INDEX_W+1:2]][7:0] <= dmem_req.wdata[7:0];
            if (dmem_req.be[1]) dram[dmem_req.addr[INDEX_W+1:2]][15:8] <= dmem_req.wdata[15:8];
            if (dmem_req.be[2]) dram[dmem_req.addr[INDEX_W+1:2]][23:16] <= dmem_req.wdata[23:16];
            if (dmem_req.be[3]) dram[dmem_req.addr[INDEX_W+1:2]][31:24] <= dmem_req.wdata[31:24];
        end
    end
endmodule
