module regfile (
    input  logic        clk_i,
    input  logic        rst_i,
    input  logic [4:0]  rs1_i,
    input  logic [4:0]  rs2_i,
    output logic [31:0] rs1_o,
    output logic [31:0] rs2_o,
    input  logic        we_i,
    input  logic [4:0]  rd_i,
    input  logic [31:0] wdata_i,
    output logic [31:0] x10_o
);
    logic [31:0] regs_q [32];

    always_ff @(posedge clk_i) begin
        if (rst_i) begin
            for (int i = 0; i < 32; i++) begin
                regs_q[i] <= 32'd0;
            end
        end else if (we_i && rd_i != 5'd0) begin
            regs_q[rd_i] <= wdata_i;
        end
    end

    always_comb begin
        rs1_o = (rs1_i == 5'd0) ? 32'd0 : regs_q[rs1_i];
        rs2_o = (rs2_i == 5'd0) ? 32'd0 : regs_q[rs2_i];
    end

    assign x10_o = regs_q[10];
endmodule
