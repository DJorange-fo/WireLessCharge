# STM32F334 全桥移相逆变 (Phase-Shift Full-Bridge Inverter)

基于 STM32F334C8T6 + EG2104S 栅极驱动的无线充电全桥逆变器固件。

## 硬件引脚

| 引脚 | 方向 | 功能 | 特殊说明 |
|------|------|------|----------|
| PB5 | AF2 (TIM3_CH2) | 左半桥 PWM | 无冲突 |
| PB3 | AF1 (TIM2_CH2) | 右半桥 PWM | **与 JTDO/SWO 冲突** |
| PB4 | Output PP | EN1 栅极使能 | **与 NJTRST 冲突** |
| PA15 | Output PP | EN2 栅极使能 | **与 JTDI 冲突** |
| PB15 | Input Pull-Up | 按键（toggle 开关） | 无冲突 |

### 引脚冲突详细说明

PB3、PB4、PA15 默认为 JTAG 调试引脚。CubeMX 选择 \"Serial Wire\" 后理论上应释放这些引脚，但实际生成的 HAL_MspInit() **不会写 SYSCFG_CFGR1 寄存器来关闭 JTAG**，导致这些引脚被硬件锁死在调试模式，GPIO 配置无效。

**解决方法：** 在 main.c 的 USER CODE SysInit 区手动写入：
`c
SYSCFG->CFGR1 = (SYSCFG->CFGR1 & ~(0x7UL << 26)) | (0x2UL << 26);
`
bit[28:26]=010: 关闭 JTAG-DP，保留 SW-DP（PA13/SWDIO, PA14/SWCLK 不受影响）。

## CubeMX 配置

### 时钟
- HSE: 8MHz Crystal, PREDIV=/1, PLL×9 → SYSCLK=72MHz
- APB1=/2 (36MHz, Timer Clock=72MHz), APB2=/1 (72MHz)

### TIM3 (Master)
| 参数 | 值 |
|------|-----|
| Prescaler | 0 |
| Counter Period | 719 |
| CH1 | Output Compare, No Pin (内部触发源) |
| CH2 | PWM Generation CH2 (PB5) |
| TRGO | **OC1REF** |
| Master/Slave | Enable |

### TIM2 (Slave)
| 参数 | 值 |
|------|-----|
| Prescaler | 0 |
| Counter Period | 719 |
| CH2 | PWM Generation CH2 (PB3) |
| Slave Mode | **Reset Mode** |
| Trigger Source | **ITR2 (TIM3)** |

### GPIO
| 引脚 | 模式 | 备注 |
|------|------|------|
| PB4 | Output PP, Pull-down, Initial Low | EN1 |
| PA15 | Output PP, Pull-down, Initial Low | EN2 |
| PB15 | Input, Pull-up | 按键 |

### SYS
- Debug: **Serial Wire**

## 安全机制

### 上电安全

上电到 MX_GPIO_Init() 执行前，PB4/PA15 处于 JTAG 模式（内部上拉 ≈ HIGH），此时 EG2104S 的 SD 引脚为高电平 → **栅极驱动关断，安全**。

SysInit 区执行顺序：
1. SYSCFG->CFGR1 关闭 JTAG → PB4/PA15 释放为 GPIO
2. 手动开启 GPIOA/GPIOB 时钟
3. **立即写 EN1/EN2 = HIGH（保持关断态）**
4. HAL_Delay(50) 给 ST-Link 连接 SWD 的时间窗口
5. MX_GPIO_Init() 正式初始化

### SWD 烧录保护

PB3 用作 PWM 时，CubeMX 必须在 SYS 中选择 Serial Wire。配合 SysInit 中的 JTAG 禁用代码，SWD（PA13/PA14）始终可用，烧录调试不受影响。

### 死区

EG2104S 芯片内置 ≈520ns 死区，软件无需额外处理。

## 文件结构

`
Core/
├── Inc/
│   └── fz_hbridge.h      # H 桥接口头文件
└── Src/
    ├── fz_hbridge.c       # H 桥实现 (init/enable/disable/toggle/set_phase)
    ├── main.c             # 主程序 (按键消抖 + 开关控制)
    ├── tim.c              # TIM2/TIM3 初始化 (CubeMX 生成)
    └── gpio.c             # GPIO 初始化 (CubeMX 生成)
`

## 移相工作原理

`
TIM3:  0 ──── CH1=phase ──── CH2=360 ──── 719 → 0
       ┌─── PWM1 (PB5, 左半桥) 50% ───┐
       │ HIGH (0~359)  │ LOW (360~719) │

       CH1 匹配 → OC1REF → TRGO ────→ TIM2 Reset
                                       ↓
TIM2:                                 0 ──── 360 ──── 719 → ...
                                      ┌─── PWM2 (PB3, 右半桥) 50% ───┐
                                      │ HIGH (0~359) │ LOW (360~719) │
`

- phase=0: 两路同相
- phase=360: 两路反相 (180° 移相)
- phase 可动态调整，控制输出功率

## 开发中遇到的问题

| 问题 | 原因 | 解决方法 |
|------|------|----------|
| \"Could not stop Cortex-M device!\" | PB3 配置为 TIM2_CH2 覆盖 SWO，SWD 无法连接 | SysInit 中加 HAL_Delay(50) 留出连接窗口 |
| 上电后 PB4/PA15 一直 2.8V，代码不执行 | 27.12MHz 晶振配 PREDIV=/1 PLL×9=244MHz 超频，进 Error_Handler | 换 8MHz 晶振 |
| PWM1 (PB5) 有信号，PWM2 (PB3) 无信号 | JTAG 未禁用，PB3 硬件锁死在 JTDO 模式 | SYSCFG->CFGR1 写 010 释放 JTAG |
| TIM3 TRGO=RESET 导致 TIM2 不触发 | TRGO 只在 Reset 事件发脉冲，TIM3 自由运行不 Reset | CubeMX 中改 TRGO 为 OC1REF |
| 上电过流 | SysInit 中误将 EN 拉 LOW 打开了栅极驱动 | 改 EN=LOW→HIGH，保持关断安全态 |
