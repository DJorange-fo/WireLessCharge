#include "fz_hbridge.h"
#include "tim.h"

#define HBRIDGE_DUTY_50  (HBRIDGE_PERIOD / 2U)

static uint16_t g_phase;
static int      g_enabled;

void fz_hbridge_init(void)
{
    g_phase   = 0;
    g_enabled = 0;

    HAL_GPIO_WritePin(EN1_GPIO_Port, EN1_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(EN2_GPIO_Port, EN2_Pin, GPIO_PIN_RESET);

    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, HBRIDGE_DUTY_50);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, HBRIDGE_DUTY_50);
}

void fz_hbridge_enable(void)
{
    if (g_enabled) return;

    HAL_GPIO_WritePin(EN1_GPIO_Port, EN1_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(EN2_GPIO_Port, EN2_Pin, GPIO_PIN_SET);

    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);

    g_enabled = 1;
}

void fz_hbridge_disable(void)
{
    if (!g_enabled) return;

    HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_2);
    HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_2);

    HAL_GPIO_WritePin(EN2_GPIO_Port, EN2_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(EN1_GPIO_Port, EN1_Pin, GPIO_PIN_RESET);

    g_enabled = 0;
}

void fz_hbridge_toggle(void)
{
    if (g_enabled)
        fz_hbridge_disable();
    else
        fz_hbridge_enable();
}

int fz_hbridge_is_enabled(void)
{
    return g_enabled;
}

uint16_t fz_hbridge_get_phase(void)
{
    return g_phase;
}

void fz_hbridge_set_phase(uint16_t phase)
{
    if (phase > HBRIDGE_PHASE_MAX)
        phase = HBRIDGE_PHASE_MAX;

    g_phase = phase;

    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, HBRIDGE_DUTY_50);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, phase);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, HBRIDGE_DUTY_50);
}
