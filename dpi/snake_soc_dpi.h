#ifndef PIPELINE_CPU_SNAKE_SOC_DPI_H
#define PIPELINE_CPU_SNAKE_SOC_DPI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int  dpi_snake_soc_init(const char *elf_path);
void dpi_snake_soc_fini(void);
void dpi_snake_soc_tick(void);

int dpi_snake_soc_read(uint32_t addr, int width, uint32_t *value_out);
int dpi_snake_soc_write(uint32_t addr, int width, uint32_t value, uint8_t strb);

int dpi_snake_soc_msip(void);
int dpi_snake_soc_mtip(void);
int dpi_snake_soc_meip(void);

#ifdef __cplusplus
}
#endif

#endif /* PIPELINE_CPU_SNAKE_SOC_DPI_H */
