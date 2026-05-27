`include "uarch.svh"

module dae_id_stage (
    input  logic                  flush_i,
    input  logic                  in_valid_i,
    output logic                  in_ready_o,
    input  uarch_pkg::fetch_pkt_t in_i,
    output logic                  out_valid_o,
    input  logic                  out_ready_i,
    output uarch_pkg::decoded_ir_t out_o
);
    uarch_pkg::decoded_ir_t decoded_raw;

    decoder u_decoder (
        .pc_i(in_i.pc),
        .inst_i(in_i.inst),
        .if_bus_error_i(in_i.error),
        .decoded_o(decoded_raw)
    );

    always_comb begin
        out_o = decoded_raw;
        out_o.epoch = in_i.epoch;
        out_o.seq_id = in_i.seq_id;
    end

    assign out_valid_o = in_valid_i && !flush_i;
    assign in_ready_o = out_ready_i || flush_i;
endmodule
