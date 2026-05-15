/**
 * @file    adc_sensor.c
 * @brief   ADC sensor module — DMA-driven 3-channel scan with physical conversion.
 *
 * Hardware:
 *   PA0 → ADC1_IN1  — VIN  (24V divider: 20K/1K, ratio 1/21)
 *   PA1 → ADC1_IN2  — Idc  (INA282, 0.125V/A, REF=GND)
 *   PB0 → ADC1_IN11 — Iac  (OPA365, 0.165V/A, REF=1.65V)
 *
 * DMA:  DMA1_Channel1, Circular mode → adc_raw[3] continuously updated.
 *       vTaskController reads adc_raw[] at 1kHz — always fresh.
 */

#include "adc_sensor.h"
#include "adc.h"

/* ---------------------------------------------------------------- */
/* Calibration constants (from hardware schematic)                  */
/* ---------------------------------------------------------------- */

/* ---- VIN (PA0) ---- */
#define VIN_DIVIDER_RATIO    21.0f    /* (20K + 1K) / 1K */
#define ADC_VREF             3.30f    /* ADC reference voltage */
#define ADC_MAX_CODE         4095.0f  /* 12-bit */

/* ---- Idc (PA1) — INA282 (REF=GND, unidirectional) ---- */
#define IDC_SENSITIVITY      0.125f   /* V per ampere (50V/V × 2.5mΩ) */

/* ---- Iac (PB0) — OPA365 (REF=1.65V, bidirectional) ---- */
#define IAC_SENSITIVITY      0.165f   /* V per ampere (16.5× × 10mΩ) */
#define IAC_MID_VOLTAGE      1.65f    /* zero-current output */

/* ---------------------------------------------------------------- */
/* DMA buffer                                                     */
/* ---------------------------------------------------------------- */
volatile uint16_t adc_raw[ADC_CH_COUNT];

/* ---------------------------------------------------------------- */
/* Conversion helpers                                              */
/* ---------------------------------------------------------------- */

static inline float adc_to_voltage(uint16_t raw)
{
    return (float)raw * ADC_VREF / ADC_MAX_CODE;
}

/* ---------------------------------------------------------------- */
/* Public API — physical conversion                               */
/* ---------------------------------------------------------------- */

float adc_read_vin_v(void)
{
    float v_adc = adc_to_voltage(adc_raw[ADC_IDX_VIN]);
    return v_adc * VIN_DIVIDER_RATIO;
}

float adc_read_idc_a(void)
{
    float v_adc = adc_to_voltage(adc_raw[ADC_IDX_IDC]);
    return v_adc / IDC_SENSITIVITY;
}

float adc_read_iac_a(void)
{
    float v_adc = adc_to_voltage(adc_raw[ADC_IDX_IAC]);
    return (v_adc - IAC_MID_VOLTAGE) / IAC_SENSITIVITY;
}

float adc_read_iac_raw_v(void)
{
    return adc_to_voltage(adc_raw[ADC_IDX_IAC]);
}

/* ---------------------------------------------------------------- */
/* Lifecycle                                                       */
/* ---------------------------------------------------------------- */

void adc_sensor_start(void)
{
    HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);

    if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_raw, ADC_CH_COUNT) != HAL_OK) {
        Error_Handler();
    }
}
