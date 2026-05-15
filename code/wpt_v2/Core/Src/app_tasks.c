/**
  * @file    app_tasks.c
  * @brief   FreeRTOS task functions. Glue layer between RTOS and modules.
  *
  * Task layout:
  *   vTaskController  (prio 3, 1ms) — ADC sampling, protection, PID, H-bridge
  *   vTaskPeripheral  (prio 1, 50ms) — button, UART debug, OLED (future)
  */

#include "main.h"
#include "fz_hbridge.h"
#include "adc_sensor.h"
#include "FreeRTOS.h"
#include "task.h"

/* ---------------------------------------------------------------- */
/* Peripheral task — button scanning, state toggling, debug output  */
/* ---------------------------------------------------------------- */
void vTaskPeripheral(void *pvParameters)
{
    (void)pvParameters;

    #define TASK_PERI_PERIOD_MS   50U
    #define BUTTON_PIN            GPIO_PIN_15
    #define BUTTON_PORT           GPIOB
    #define DEBOUNCE_MS           50U

    uint32_t lastTick = 0;
    uint8_t  stable   = 1;
    uint8_t  prev     = 1;

    TickType_t xLastWake = xTaskGetTickCount();

    for (;;) {
        /* Button debounce — same logic as original main.c superloop */
        uint8_t curr = HAL_GPIO_ReadPin(BUTTON_PORT, BUTTON_PIN);
        if (curr != prev) {
            lastTick = xTaskGetTickCount();
            prev = curr;
        }
        if ((xTaskGetTickCount() - lastTick) > pdMS_TO_TICKS(DEBOUNCE_MS)) {
            if (curr != stable) {
                stable = curr;
                if (stable == 0) {
                    fz_hbridge_toggle();
                }
            }
        }

        vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(TASK_PERI_PERIOD_MS));
    }

    #undef TASK_PERI_PERIOD_MS
    #undef BUTTON_PIN
    #undef BUTTON_PORT
    #undef DEBOUNCE_MS
}

/* ---------------------------------------------------------------- */
/* Controller task — ADC, protection, PID, H-bridge phase control   */
/* ---------------------------------------------------------------- */
void vTaskController(void *pvParameters)
{
    (void)pvParameters;

    TickType_t xLastWake = xTaskGetTickCount();

    for (;;) {
        /* 1. ADC read (fresh from DMA circular buffer, updated at ~150kHz) */
        float vin  = adc_read_vin_v();
        float idc  = adc_read_idc_a();
        float iac  = adc_read_iac_a();
        (void)vin; (void)idc; (void)iac;

        /* TODO: 2. Protection checks (OCP, OVP, OTP) */
        /* TODO: 3. PID control loop → fz_hbridge_set_phase() */

        vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(1));
    }
}
