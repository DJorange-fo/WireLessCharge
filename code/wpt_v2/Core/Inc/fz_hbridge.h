#ifndef FZ_HBRIDGE_H
#define FZ_HBRIDGE_H
#include "main.h"

#define HBRIDGE_PERIOD      720U
#define HBRIDGE_PHASE_MAX   (HBRIDGE_PERIOD / 2U)

void fz_hbridge_init(void);
void fz_hbridge_enable(void);
void fz_hbridge_disable(void);
void fz_hbridge_toggle(void);
int  fz_hbridge_is_enabled(void);
void fz_hbridge_set_phase(uint16_t phase);
uint16_t fz_hbridge_get_phase(void);

#endif
