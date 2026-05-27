set(DAE_PIPELINE_CPU_SV
    ${CMAKE_CURRENT_LIST_DIR}/include/riscv.svh
    ${CMAKE_CURRENT_LIST_DIR}/include/uarch.svh
    ${CMAKE_CURRENT_LIST_DIR}/src/fifo.sv
    ${CMAKE_CURRENT_LIST_DIR}/src/regfile.sv
    ${CMAKE_CURRENT_LIST_DIR}/src/decoder.sv
    ${CMAKE_CURRENT_LIST_DIR}/src/scoreboard.sv
    ${CMAKE_CURRENT_LIST_DIR}/src/alu.sv
    ${CMAKE_CURRENT_LIST_DIR}/src/branch_unit.sv
    ${CMAKE_CURRENT_LIST_DIR}/src/CSRFile.sv
    ${CMAKE_CURRENT_LIST_DIR}/src/lsu.sv
    ${CMAKE_CURRENT_LIST_DIR}/src/load_data_filter.sv
    ${CMAKE_CURRENT_LIST_DIR}/src/if_stage.sv
    ${CMAKE_CURRENT_LIST_DIR}/src/id_stage.sv
    ${CMAKE_CURRENT_LIST_DIR}/src/frontend.sv
    ${CMAKE_CURRENT_LIST_DIR}/src/exe_stage.sv
    ${CMAKE_CURRENT_LIST_DIR}/src/mem_stage.sv
    ${CMAKE_CURRENT_LIST_DIR}/src/wb_stage.sv
    ${CMAKE_CURRENT_LIST_DIR}/src/backend.sv
    ${CMAKE_CURRENT_LIST_DIR}/src/Top.sv
)

set(DAE_PIPELINE_CPU_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/include)

set(DAE_PIPELINE_CPU_TB
    ${CMAKE_CURRENT_LIST_DIR}/tb/tb_top.sv
)

set(DAE_PIPELINE_CPU_DPI
    ${CMAKE_CURRENT_LIST_DIR}/dpi/snake_soc_dpi.c
)
