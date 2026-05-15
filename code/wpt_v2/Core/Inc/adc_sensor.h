#ifndef __ADC_SENSOR_H__
#define __ADC_SENSOR_H__

#include "main.h"

/* Number of ADC channels in scan sequence */
#define ADC_CH_COUNT    3U

/* Index into adc_raw[] */
typedef enum {
    ADC_IDX_VIN = 0,    /* PA0, ADC1_IN1  — 24V input, 20K:1K divider */
    ADC_IDX_IDC = 1,    /* PA1, ADC1_IN2  — INA282 DC bus current */
    ADC_IDX_IAC = 2,    /* PB0, ADC1_IN11 — OPA365 AC coil current */
} adc_channel_idx_t;

/* DMA target buffer — written by hardware, read by vTaskController */
extern volatile uint16_t adc_raw[ADC_CH_COUNT];

/* Lifecycle — call once after MX_ADC1_Init() */
void adc_sensor_start(void);

/* Physical conversion (based on latest DMA buffer, ISR-safe to read adc_raw) */
float adc_read_vin_v(void);     /* 24V input voltage (at connector, before divider) */
float adc_read_idc_a(void);     /* DC bus current in amperes (INA282, 0.125V/A) */
float adc_read_iac_a(void);     /* AC instantaneous coil current in amperes (OPA365, 0.165V/A) */
float adc_read_iac_raw_v(void); /* Iac raw voltage at PB0 pin (0~3.3V) */

#endif /* __ADC_SENSOR_H__ */
