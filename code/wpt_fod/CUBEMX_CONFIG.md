# STM32F334C8T6 CubeMX 配置参考 — WPT_FOD 无线充电异物检测

> **工程名**: `wpt_fod`  
> **MCU**: STM32F334C8T6 (LQFP48)  
> **HAL 库**: STM32Cube FW_F3 V1.12+  
> **IDE**: STM32CubeIDE / Keil MDK  
> **更新日期**: 2026-05-12

---

## 1. 工程初始化

### 1.1 CubeMX 新建工程
```
MCU 选型: STM32F334C8Tx (LQFP48)
工程名:   wpt_fod
工具链:   MDK-ARM V5 / STM32CubeIDE
堆栈:     Stack=0x400, Heap=0x200
```

### 1.2 Pinout 总览 (按功能分组)

```
                    STM32F334C8T6 LQFP48
              ┌─────────────────────────────┐
              │                             │
  NRST  ── 7 │                             │ 42 BOOT0
  OSC_IN── 5 │                             │ 40 PB7  ── UART_RX
  OSC_OUT─ 6 │                             │ 41 PB6  ── UART_TX
              │                             │
  PA8 ─── 31 │                             │ 29 PB8  ── I2C_SCL (OLED)
  PA9 ─── 32 │                             │ 30 PB9  ── I2C_SDA (OLED)
  PA10 ── 33 │                             │ 35 PB12 ── BKIN (刹车)
  PA11 ── 34 │                             │
              │                             │ 10 PA0  ── ADC_IN1  (VS1)
  PA13 ── 37 │                             │ 11 PA1  ── ADC_IN2  (VS2)
  PA14 ── 38 │                             │ 12 PA2  ── ADC_IN3  (I_primary)
              │                             │ 13 PA3  ── ADC_IN4  (VIN)
  VDD ── 24,36,48                        │ 14 PA4  ── ADC_IN5  (I_bus/INA282)
  VSS ── 23,35,47                        │ 15 PA5  ── ADC_IN6  (V_REF)
              │                             │ 16 PA6  ── ADC_IN7  (FOD/VOUT)
              │                             │ 17 PA7  ── GPIO_OUT (EN1)
              │                             │ 18 PB0  ── GPIO_OUT (EN2)
              │                             │ 19 PB1  ── GPIO_OUT (LED1)
              │                             │ 20 PB2  ── GPIO_IN  (KEY1)
              │                             │ 26 PB13 ── GPIO_OUT (LED2)
              │                             │ 27 PB14 ── GPIO_IN  (KEY2)
              │                             │ 28 PB15 ── GPIO_IN  (KEY3)
              │                             │ 21 PB10 ── GPIO_OUT (Buzzer/报警)
              │                             │ 22 PB11 ── GPIO_OUT (Relay/使能)
              └─────────────────────────────┘
```

---

## 2. 系统时钟 (Clock Configuration)

### 2.1 HSE + PLL → 72MHz
```
HSE:         8 MHz (外部晶振)
PLL Source:  HSE
PLL MUL:     ×9  (= 72MHz 之前 PLL 分频由 PREDIV 处理)
PREDIV:      /1
SYSCLK:      72 MHz
HCLK:        72 MHz
APB1:        36 MHz (max)
APB2:        72 MHz (max)

HRTIM Clock: 144 MHz (SYSCLK × 2, 通过 PLL ×18 或 TIM2 倍频)
             实际 CubeMX 中 HRTIM 时钟 = 144 MHz (从 SYSCLK 2×)
```

CubeMX 操作: `Clock Configuration` 标签页 → HSE=8MHz → PLL=×9 → HCLK=72

---

## 3. HRTIM 高分辨率定时器 (核心)

### 3.1 概述
HRTIM 是 STM32F334 的核心外设，用于产生 100kHz 高精度互补 PWM 驱动全桥逆变器。

### 3.2 CubeMX 配置

**Pinout 视图**:
- `PA8` → HRTIM_CHA1
- `PA9` → HRTIM_CHA2
- `PA10` → HRTIM_CHB1
- `PA11` → HRTIM_CHB2

**HRTIM 参数配置** (在 `HRTIM` 标签页):

```yaml
Master Timer:
  Master Period:      1439        # 144MHz / 100kHz = 1440; Period = 1440-1
  Master Prescaler:   1           # HRTIM clock = 144MHz (fHRTIM = fCPU × 2)
  Period x32 Mode:    Disable     # 标准模式即可, 100kHz 周期 1440 足够
  Push-Pull:          Enable      # 推挽输出

Timer A (左半桥):
  TA Reset Update:    Master Period
  Compare Unit 1:     720         # 50% 占空比
  Compare Unit 2:     720
  Output1 (PA8):      Active High
  Output2:            Active Low  # 互补输出
  Deadtime Rising:    22          # 150ns @ 144MHz (22 × 6.94ns ≈ 150ns)
  Deadtime Falling:   22

Timer B (右半桥):
  TB Reset Update:    Master Period
  Compare Unit 1:     720         # 初始 50%, 运行时调整移相
  Compare Unit 2:     720
  Output1 (PA10):     Active High
  Output2 (PA11):     Active Low
  Deadtime Rising:    22          # 150ns
  Deadtime Falling:   22
```

**关键计算**:
```
fHRTIM    = 144 MHz
T_HRTIM   = 1/144MHz ≈ 6.94 ns
f_PWM     = 100 kHz
Period    = fHRTIM / f_PWM = 1440
PERxR     = Period - 1 = 1439
死区时间   = Deadtime × 6.94ns
          = 22 × 6.94ns ≈ 152ns
```

**移相控制原理**:
```
Timer A CMP1 = 0     → PA8 上升沿
Timer A CMP1 = 720   → PA8 下降沿 (50%)
Timer B CMP1 = φ     → PA10 上升沿 (移相角 φ)
Timer B CMP1 = 720+φ → PA10 下降沿

移相角 φ ∈ [0, 719] 对应 0°~180°
输出功率 ∝ (1 - φ/720) (φ越小功率越大)
```

---

## 4. ADC 模数转换

### 4.1 CubeMX 配置

**ADC1** (独立模式, 12-bit):

| 通道   | 引脚 | 信号         | 采样时间  | 说明              |
|--------|------|-------------|----------|-------------------|
| IN1    | PA0  | VS1         | 7.5 cycles | 谐振电压采样点1   |
| IN2    | PA1  | VS2         | 7.5 cycles | 谐振电压采样点2   |
| IN3    | PA2  | I_primary   | 7.5 cycles | OPA365 线圈电流    |
| IN4    | PA3  | VIN         | 7.5 cycles | 24V 输入电压       |
| IN5    | PA4  | I_bus       | 7.5 cycles | INA282 母线电流    |
| IN6    | PA5  | V_REF       | 7.5 cycles | 1.65V 参考         |
| IN7    | PA6  | FOD_VOUT    | 7.5 cycles | OPA365 输出(冗余)  |

**ADC 参数**:
```yaml
Mode:                 Independent
Resolution:           12-bit (数据对齐: Right)
Scan Mode:            Enable (扫描所有通道)
Continuous Mode:      Enable 或 Disable (取决于触发方式)
DMA Request:          Enable (DMA1 Channel1, Circular, Half-Word)
External Trigger:     HRTIM_ADCTRIG1 (与 PWM 同步触发)
Sampling Time:        7.5 cycles (所有通道一致, 约 104ns @ 72MHz ADC clock)
ADC Clock:            PCLK2/2 = 72MHz/2 = 36MHz (max for F334)
```

**DMA 配置**:
```yaml
DMA:              DMA1 Channel1
Direction:        Peripheral to Memory
Mode:             Circular
Data Width:       Half Word (16-bit)
Increment:        Memory=Yes, Peripheral=No
Priority:         High
```

### 4.2 采样时序
```
单通道采样时间: 12.5 cycles @ 36MHz = 347ns
7通道总时间:    7 × 347ns ≈ 2.43μs
100kHz 周期:    10μs
每周期可采:     10μs / 2.43μs ≈ 4 个完整扫描 (足够)
```

---

## 5. 内置比较器 (COMP)

### 5.1 CubeMX 配置

**COMP2** (过流保护):
```yaml
Input +:          PA1 (VS2 分压/比较阈值)
Input -:          Internal VREFINT (≈1.2V) 或 DAC 输出
Output:           GPIO (PB12 BKIN) 或直接连 HRTIM Fault
Output Polarity:  None (不反相)
Blanking Window:  ~50ns
```

**COMP4** (母线过压):
```yaml
Input +:          PA4 (Vin 分压)
Input -:          DAC1_OUT 或 VREFINT
Output:           HRTIM EEV (Emergency Event)
```

### 5.2 HRTIM 故障联动
```
HRTIM Fault Input 1 → COMP2_OUT (过流)
HRTIM Fault Input 2 → COMP4_OUT (过压)
Fault Action:      所有输出强制 Idle State (高阻/低电平)
Fault 恢复:        软件清除
```

---

## 6. 通信外设

### 6.1 USART1 (调试/上位机)

| 引脚 | 功能 |
|------|------|
| PB6  | TX   |
| PB7  | RX   |

```yaml
Mode:              Asynchronous
Baud Rate:         115200
Word Length:       8 bits
Parity:            None
Stop Bits:         1
Data Direction:    RX & TX
Over Sampling:     16 samples
```

### 6.2 I2C1 (OLED)

| 引脚 | 功能   |
|------|--------|
| PB8  | SCL    |
| PB9  | SDA    |

```yaml
Mode:              I2C
Speed:             Fast Mode (400 kHz)
Clock No Stretch:  Disable
Addressing:        7-bit
Duty Cycle:        Tlow/Thigh = 2
Analog Filter:     Enable
Digital Filter:    0
```

---

## 7. GPIO 配置

| 引脚 | 模式         | 上拉/下拉 | 标签     | 功能           |
|------|-------------|----------|----------|----------------|
| PA7  | Output PP   | No Pull  | EN1      | U16 栅极驱动使能 |
| PB0  | Output PP   | No Pull  | EN2      | U17 栅极驱动使能 |
| PB1  | Output PP   | No Pull  | LED1     | 状态指示灯1     |
| PB13 | Output PP   | No Pull  | LED2     | 状态指示灯2     |
| PB2  | Input       | Pull-Up  | KEY1     | 功能按键1       |
| PB14 | Input       | Pull-Up  | KEY2     | 功能按键2       |
| PB15 | Input       | Pull-Up  | KEY3     | 功能按键3       |
| PB10 | Output PP   | No Pull  | Buzzer   | 蜂鸣器/报警     |
| PB11 | Output PP   | No Pull  | Relay    | 电源继电器      |
| PB12 | Alternate   | No Pull  | BKIN     | TIM1 刹车输入   |

**PA7/PB0 初始状态**: LOW (EN=0, 栅极驱动关闭 → 安全启动)

---

## 8. 高级定时器 TIM1

TIM1 用于刹车 (BKIN) 和辅助功能:

```yaml
TIM1:
  BKIN (PB12):     External Signal, Active High (或 Low, 取决于 COMP 输出极性)
  Break Polarity:  High
  Auto-Output:     Enable (刹车时自动关闭 PWM 输出)
```

---

## 9. 中断优先级

| 中断               | 优先级 | 说明                 |
|--------------------|--------|----------------------|
| HRTIM Master       | 0 (最高) | 移相角更新, ADC触发  |
| ADC+DMA            | 1      | ADC 转换完成          |
| TIM1 Break         | 2      | 过流/过压 紧急刹车    |
| SysTick            | 3      | HAL 时基, 1ms         |
| USART1             | 4      | 串口收发              |
| I2C1               | 5      | OLED 通信             |
| EXTI (KEY)         | 6      | 按键检测              |

---

## 10. NVIC 设置

```yaml
Preemption Priority Group:  4 bits (16 levels)
HRTIM Master IRQ:           Enable, Preempt=0, Sub=0
ADC1+DMA:                   Enable, Preempt=1, Sub=0
TIM1 BRK:                   Enable, Preempt=2, Sub=0
SysTick:                    Enable, Preempt=3, Sub=0
USART1:                     Enable, Preempt=4, Sub=0
```

---

## 11. 生成代码后的检查清单

### 11.1 `stm32f3xx_hal_conf.h`
```c
#define HAL_HRTIM_MODULE_ENABLED   // ← 必须手动使能 HRTIM
#define HAL_COMP_MODULE_ENABLED
#define HAL_DAC_MODULE_ENABLED
```

### 11.2 `main.c` 初始化顺序
```c
int main(void)
{
    HAL_Init();
    SystemClock_Config();    // 72MHz

    // 外设初始化 (CubeMX 自动生成)
    MX_GPIO_Init();          // 先 GPIO, 保证 EN=0 (安全)
    MX_DMA_Init();
    MX_ADC1_Init();
    MX_HRTIM_Init();         // PWM 初始化 (此时 EN=0, 不输出)
    MX_COMP2_Init();
    MX_USART1_UART_Init();
    MX_I2C1_Init();
    MX_TIM1_Init();

    // 启动 HRTIM 输出
    HAL_HRTIM_WaveformOutputStart(&hhrtim1, HRTIM_OUTPUT_TA1 | HRTIM_OUTPUT_TB1);
    HAL_HRTIM_WaveformCounterStart(&hhrtim1, HRTIM_TIMERID_MASTER |
                                             HRTIM_TIMERID_TIMER_A |
                                             HRTIM_TIMERID_TIMER_B);

    // 启动 ADC + DMA (连续采集)
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_buffer, ADC_CHANNEL_COUNT);

    while (1) {
        WPT_FOD_MainLoop();
    }
}
```

---

## 12. 关键外设地址 (调试用)

| 外设    | 基地址     |
|---------|-----------|
| HRTIM1  | 0x40017400 |
| ADC1    | 0x50000000 |
| COMP    | 0x40010020 |
| TIM1    | 0x40012C00 |
| USART1  | 0x40013800 |
| I2C1    | 0x40005400 |
| DMA1    | 0x40020000 |

---

## 13. 代码模块对应关系

| CubeMX 外设     | 代码模块            | 功能               |
|-----------------|---------------------|--------------------|
| HRTIM Timer A/B | `hrtim_pwm.c`       | 100kHz PWM + 移相  |
| ADC1 + DMA      | `adc_sense.c`       | 电压/电流采样       |
| COMP2/4         | `protection.c`      | 硬件过流/过压保护   |
| TIM1 BKIN       | `protection.c`      | 刹车联动           |
| USART1          | `uart_debug.c`      | 调试输出            |
| I2C1            | `oled_display.c`    | OLED 状态显示       |
| GPIO PA7/PB0    | `gate_driver.c`     | 栅极驱动使能        |
| GPIO PB2/PB14/15| `key_input.c`       | 按键扫描            |
| SysTick         | `main.c`            | 系统时基            |

---

## 14. Ring-Down FOD 检测流程与时序

```c
// FOD 检测伪代码
void WPT_FOD_RingDownDetect(void)
{
    static uint32_t last_fod_tick = 0;

    // 每 100ms 执行一次 FOD 检测
    if (HAL_GetTick() - last_fod_tick < 100) return;
    last_fod_tick = HAL_GetTick();

    // Step 1: 关闭 PWM (所有 Timer 输出强制 Idle)
    HAL_HRTIM_WaveformOutputStop(&hhrtim1, HRTIM_OUTPUT_TA1 | HRTIM_OUTPUT_TB1);

    // Step 2: 触发高速 ADC 采样 (burst DMA, 采集多个周期)
    //         至少采集 10 个衰减周期 (100μs)

    // Step 3: 提取每个周期的峰值 V_peak[0..N]
    float V_peak[10];
    ExtractDecayPeaks(adc_fod_buffer, V_peak, 10);

    // Step 4: 计算 Q 值
    float Q = 0;
    for (int i = 1; i < 10; i++) {
        if (V_peak[i] > 0 && V_peak[i-1] > V_peak[i]) {
            Q += M_PI / logf(V_peak[i-1] / V_peak[i]);
        }
    }
    Q /= 9.0f;  // 平均值

    // Step 5: 判断异物
    if (Q < Q_BASELINE * 0.7f) {  // 下降 >30%
        FOD_Alarm();
    }

    // Step 6: 恢复 PWM
    HAL_HRTIM_WaveformOutputStart(&hhrtim1, HRTIM_OUTPUT_TA1 | HRTIM_OUTPUT_TB1);
}
```

---

## 15. 附录: CubeMX 工程检查清单

- [ ] MCU = STM32F334C8Tx
- [ ] HSE = 8MHz, PLL ×9, HCLK = 72MHz
- [ ] HRTIM Master Period = 1439 (100kHz)
- [ ] HRTIM Deadtime = 22 (~150ns)
- [ ] PA8=HRTIM_CHA1, PA9=HRTIM_CHA2 (PWM 输出)
- [ ] PA10=HRTIM_CHB1, PA11=HRTIM_CHB2 (互补或第二路)
- [ ] ADC1: 7通道, Scan+DMA+Circular, 12-bit
- [ ] ADC 触发源 = HRTIM_ADCTRIG1
- [ ] USART1: PB6(TX), PB7(RX), 115200-8-N-1
- [ ] I2C1: PB8(SCL), PB9(SDA), 400kHz
- [ ] PB12 = TIM1_BKIN (硬件刹车)
- [ ] COMP2/COMP4 使能 + 连到 HRTIM Fault
- [ ] 所有 GPIO 启动状态安全 (EN=0)
- [ ] HAL_HRTIM_MODULE_ENABLED 已定义
- [ ] NVIC 中断优先级合理
- [ ] 堆栈 Stack=0x400, Heap=0x200