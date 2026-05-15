/**
  * @file    syscfg.c
  * @brief   System configuration: JTAG release, safety init.
  *
  * NOTE: SystemClock_Config() is kept in main.c (CubeMX-generated).
  *        JTAG release resides here to keep main.c minimal.
  */

#include "main.h"

void syscfg_jtag_release(void)
{
    /* Release JTAG-DP, keep SW-DP (PA13/SWDIO, PA14/SWCLK).
     * bit[28:26] = 010: disable JTAG, enable SW.
     * MUST happen BEFORE MX_GPIO_Init() — otherwise PB3/PB4/PA15
     * stay locked in JTAG mode and cannot be used as PWM2/EN1/EN2. */
    SYSCFG->CFGR1 = (SYSCFG->CFGR1 & ~(0x7UL << 26)) | (0x2UL << 26);
}

void syscfg_powerup_safety(void)
{
    /* Enable GPIO clocks early so we can set EN pins HIGH.
     * EN1(PB4), EN2(PA15) = HIGH → EG2104S #SD active-low = SHUTDOWN.
     * This keeps all MOSFETs OFF during power-up until firmware
     * explicitly enables the H-bridge. */
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    HAL_GPIO_WritePin(EN1_GPIO_Port, EN1_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(EN2_GPIO_Port, EN2_Pin, GPIO_PIN_SET);

    /* Give the ST-Link/SWD a 50ms window to attach before GPIO reconfig. */
    HAL_Delay(50);
}
