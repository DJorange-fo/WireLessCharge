# Wireless Charging Project — 工程快速参考手册

> **主控芯片**: STM32F334C8T6  
> **拓扑结构**: LCC-LCC 无线电能传输  
> **谐振频率**: 100 kHz (WPC Qi 标准)  
> **更新日期**: 2026-05-12

---

## 1. 系统总体架构

```
 24V DC 输入
    │
    ├─► [TPS43061 Boost] ──► 48V DC 母线 ──► [全桥逆变器] ──► [LCC谐振网络] ──► 发射线圈
    │                          (VIN)              (4× MOSFET)      (Lp + Cp + LCC补偿)
    │
    ├─► [TPS54360 Buck] ──► 12V ──► [TPS54202 Buck] ──► 5V ──► [AMS1117 LDO] ──► 3.3V
    │                          │                       │                │
    │                          │                       │                └─► STM32F334, 传感器
    │                          └─► 栅极驱动 EG2104S    └─► OLED, LED
    │
    └─► [采样电路] ◄── VS1, VS2 (电压), INA282 (电流), OPA365 (信号调理)
              │
              └─► STM32F334C8T6 ADC / 比较器 / 运放
```

---

## 2. 电源树 (Power Tree)

| 电源轨    | 来源芯片           | 电压   | 用途                         |
|-----------|--------------------|--------|------------------------------|
| VIN       | 外部输入            | 24V    | 系统总电源                   |
| VCC_24V   | TPS43061 内部 LDO   | 24V    | EG2104S 栅极驱动供电         |
| 12V       | TPS54360 Buck       | 12V    | EG2104S 逻辑供电, 中间电源   |
| +5V       | TPS54202 Buck       | 5V     | OLED, 比较器, LED, USB相关   |
| 3.3V      | AMS1117-3.3 LDO     | 3.3V   | STM32F334, INA282, OPA365    |
| V_REF     | 电阻分压            | —      | ADC 参考电压                 |

**关键电源芯片**:
- **U1 TPS43061RTER**: 同步升压控制器, 24V→48V, 集成 LDO (VCC=~7V)
- **U3 TPS54360BDDAR**: 60V/3.5A 降压转换器, VIN→12V
- **U4 TPS54202DDCR**: 28V/2A 同步降压, 12V→5V
- **U5 AMS1117-3.3**: LDO, 5V→3.3V

---

## 3. 功率级 (Power Stage)

### 3.1 升压级 (Boost Stage)
| 元件 | 型号/值        | 说明                    |
|------|----------------|------------------------|
| U1   | TPS43061RTER   | 同步升压控制器          |
| Q1   | BSC035N10NS5   | 下管 N-MOSFET, 100V     |
| Q2   | BSC035N10NS5   | 上管 N-MOSFET, 100V     |
| L1   | 6.8μH          | 升压电感                |
| R12  | 2.5mΩ          | 电流采样电阻 (ISNS±)    |
| D2   | US1M           | 防反/续流二极管         |
| C9-11| 10μF+10μF+47μF | 输出滤波电容            |

### 3.2 全桥逆变器 (Full-Bridge Inverter)
| 元件  | 型号             | 说明                    |
|-------|------------------|------------------------|
| Q11   | BSC035N10NS5     | 左上桥臂 MOSFET         |
| Q12   | BSC035N10NS5     | 左下桥臂 MOSFET         |
| Q13   | BSC035N10NS5     | 右上桥臂 MOSFET         |
| Q14   | BSC035N10NS5     | 右下桥臂 MOSFET         |
| U16   | EG2104S          | 半桥栅极驱动 (HO1/LO1)  |
| U17   | EG2104S          | 半桥栅极驱动 (HO2/LO2)  |
| D6/D7 | FR207W           | 快速恢复二极管          |
| D14-17| 1N4148W          | 栅极保护二极管          |

**EG2104S 控制接口**:
- IN: PWM输入 (来自 STM32F334)
- #SD: 关断控制 (EN1/EN2)
- HO/LO: 高/低边栅极驱动输出
- VS: 半桥中点 (接 MOSFET 源极)
- VB: 自举电容引脚

**PWM 映射**:
- PWM1 → U16 (EG2104S) → Q11/Q12 (左半桥)
- PWM2 → U17 (EG2104S) → Q13/Q14 (右半桥)
- EN1 → U16 #SD
- EN2 → U17 #SD

### 3.3 谐振网络 (Resonant Tank)

**LCC-LCC 拓扑说明**:
 + "`" + "
 初级侧:                         次级侧:
  V_inv -- L_f1 --+-- C_p1 --+-- L_p -- C_f1     C_f2 -- L_s --+-- C_p2 --+-- L_f2 -- R_load
                   |          |                                  |          |
                  GND        GND                                GND        GND
 + "`" + "
- **L_f1/L_f2**: 补偿电感 (串联谐振, f=100kHz)
- **C_p1/C_p2**: 并联谐振电容
- **C_f1/C_f2**: 串联补偿电容 (与线圈电感 L_p/L_s 谐振)
- 谐振条件: omega^2 * L_f * C_p = 1,  omega^2 * (L_p-L_f) * C_f = 1
- LCC-LCC 优势: 恒流输出特性, 对负载变化不敏感, 易于实现 ZVS

| 元件    | 值                 | 说明                    |
|---------|--------------------|-------------------------|
| L4      | 2.2uH (PCB上)      | 初级补偿电感 L_f1       |
| Lp      | 40uH (发射线圈)    | 初级线圈电感            |
| Cp      | ~63.3nF @100kHz    | 谐振电容 (C53-C62)     |
| C53-C62 | 470nF x 10         | 谐振电容组 (需确认串/并联) |
| C85     | 470uF              | 直流母线滤波            |
-------|--------------------|-------------------|
| L4    | 2.2μH (PCB上)      | 谐振补偿电感      |
| Lp    | 40μH (发射线圈)    | 初级线圈电感      |
| Cp    | ~63.3nF (计算值)   | 谐振电容 (C53-C62)|  
| C53-62| 470nF × 10         | 谐振电容组 (需确认串/并联) |
| C85   | 470μF              | 直流母线滤波      |

---

## 4. 主控芯片 STM32F334C8T6

### 4.1 芯片特性
| 特性              | 规格                           |
|-------------------|--------------------------------|
| 内核              | ARM Cortex-M4F @ 72MHz         |
| Flash             | 64KB                           |
| SRAM              | 12KB (含 4KB CCM)              |
| HRTIM             | 高分辨率定时器 (217ps 分辨率)  |
| ADC               | 2×12-bit, 5MSPS, 最多21通道    |
| DAC               | 3×12-bit                       |
| 比较器            | 3× 超快速模拟比较器            |
| 运放              | 1× 内置运放 (PGA)              |
| 高级定时器        | 1× 16-bit 电机控制定时器        |
| 通用定时器        | 5×                             |
| 通信接口          | CAN, USART, I2C, SPI           |

### 4.2 引脚分配
| 引脚 | 功能      | 连接目标           | 说明              |
|------|-----------|-------------------|-------------------|
| PA0  | —         | —                 | 通用 IO           |
| PA1  | —         | —                 | 通用 IO           |
| PA2  | —         | —                 | 通用 IO           |
| PA3  | —         | —                 | 通用 IO           |
| PA4  | —         | —                 | 通用 IO / DAC      |
| PA5  | —         | —                 | 通用 IO           |
| PA6  | —         | —                 | 通用 IO           |
| PA7  | —         | —                 | 通用 IO           |
| PA8  | PWM1      | U16 IN            | HRTIM CHA1        |
| PA9  | PWM2      | U17 IN            | HRTIM CHA2        |
| PA10 | —         | —                 | HRTIM CHB1 / TIM1 |
| PA11 | —         | —                 | HRTIM CHB2        |
| PA12 | —         | —                 | HRTIM CHC1        |
| PA13 | SWDIO     | SWD 接口          | 调试接口          |
| PA14 | SWCLK     | SWD 接口          | 调试接口          |
| PA15 | —         | —                 | 通用 IO           |
| PB0  | —         | —                 | 通用 IO           |
| PB1  | —         | —                 | 通用 IO           |
| PB2  | —         | —                 | 通用 IO           |
| PB3  | —         | —                 | 通用 IO           |
| PB4  | —         | —                 | 通用 IO           |
| PB5  | —         | —                 | 通用 IO           |
| PB6  | TX        | UART TX           | 串口通信          |
| PB7  | RX        | UART RX           | 串口通信          |
| PB8  | SCL       | OLED SCL          | I2C 时钟          |
| PB9  | SDA       | OLED SDA          | I2C 数据          |
| PB10 | —         | —                 | 通用 IO           |
| PB11 | —         | —                 | 通用 IO           |
| PB12 | BKIN      | 刹车/急停输入     | TIM1 BKIN         |
| PB13 | —         | —                 | 通用 IO           |
| PB14 | —         | —                 | 通用 IO           |
| PB15 | —         | —                 | 通用 IO           |
| PC13 | —         | —                 | RTC / Tamper      |
| PC14 | OSC32_IN  | 32.768kHz 晶振    | 可选 RTC 时钟     |
| PC15 | OSC32_OUT | 32.768kHz 晶振    | 可选 RTC 时钟     |
| PF0  | OSC_IN    | 外部晶振          | HSE 时钟输入      |
| PF1  | OSC_OUT   | 外部晶振          | HSE 时钟输出      |
| NRST | NRST      | 复位按键          | 系统复位          |
| BOOT0| BOOT0     | 启动选择          | 低电平 Flash 启动  |
| BOOT1| BOOT1     | 启动选择          | PB2 复用          |

**需从程序确认的 IO**: TX(PB6), RX(PB7), BKIN(PB12), SCL(PB8), SDA(PB9), PWM1(PA8), PWM2(PA9)

---

## 5. 采样与反馈电路

### 5.1 电流采样 (双路独立)

系统有**两条独立的电流采样路径**, 分工明确:

| 路径   | 芯片         | 采样电阻  | 增益   | 用途                       |
|--------|-------------|----------|--------|----------------------------|
| DC母线 | INA282AIDR  | R12=2.5mΩ | 50V/V  | 输入功率计算, 过流保护      |
| AC线圈 | OPA365AIDR  | R36=10mΩ  | 16.5×  | FOD异物检测, Q值计算, 谐振电流波形 |

**DC 母线电流 (INA282)**:
| 元件 | 型号          | 说明                          |
|------|---------------|-------------------------------|
| U10  | INA282AIDR    | 电流检测放大器, 增益 50V/V     |
| R12  | 2.5mΩ         | 采样电阻 (TPS43061 ISNS±)      |
| 输出 | V = I_in × 2.5mΩ × 50 | 接入 STM32 ADC 做功率/保护  |

**AC 线圈电流 (OPA365)**:
| 元件 | 型号/值       | 说明                               |
|------|---------------|------------------------------------|
| R36  | 10mΩ          | 采样电阻, 串联在 LCC 谐振主回路     |
| R36  | 2512封装 3W   | 实际 0.5W 功耗, 6× 余量            |
| L5   | YI201209U101  | 共模扼流圈, 抑制共模噪声            |
| U7   | OPA365AIDR    | 差分放大 16.5×, 详见 §5.3.1        |
| 输出 | 0~3.3V        | 1.65V 中心, ±1.65V 摆幅 → ADC      |

### 5.2 电压采样
| 信号  | 来源        | 说明                        |
|-------|-------------|-----------------------------|
| VS1   | 发射回路    | 谐振网络电压采样点 1        |
| VS2   | 发射回路    | 谐振网络电压采样点 2        |
| VP    | 发射回路    | 初级线圈电压采样            |
| VIN   | 48V 输入端  | 输入电压监测                |
| VCC_24V | TPS43061 LDO | 驱动电压监测              |

### 5.3 信号调理
| 元件 | 型号          | 说明                     |
|------|---------------|--------------------------|
| U7   | OPA365AIDR    | 轨到轨运放, 信号调理      |
| U11  | LM393DR       | 双路比较器 (过流/过压保护) |
| U18  | (未标注)      | 运放 / 比较器 (TBD)       |
| U19  | (未标注)      | 运放 / 比较器 (TBD)       |

﻿#### 5.3.1 OPA365 差分放大器 — Ring-Down FOD Q值检测前端

U7 (OPA365AIDR) 构成**单电源差分转单端放大器**，是金属异物检测 (FOD) 的专用前端，
采用工程最成熟的 **Ring-Down 自由衰减法** 通过捕捉谐振回路的能量损耗速度 (Q值) 判断异物。

`
                    +3.3V
                      |
                  ┌───┴───┐
                  │ R49   │  10kΩ
                  └───┬───┘
                      +---- V_REF = +1.65V
                  ┌───┴───┐
                  │ R50   │  10kΩ
                  └───┬───┘
                      |
                     GND

  采样电阻+端 ---- R47(1kΩ) ----+-- +IN                    R46(1kΩ)   C81(820pF)
                                 |    |\                    ____      ||
  采样电阻-端 ---- R45(1kΩ) ----+-- -| \___________________|____|-----||--- GND
                                 |      >  OPA365              OPA_VOUT → ADC
                     +1.65V ----+-- +| /
                            R48(16.5kΩ) |/
                                        |
        (R44 16.5kΩ 反馈: OPA_VOUT → -IN, 图中未单独标出, 与 R46/C81 并联路径)
`

**电路参数**:
| 参数       | 值                     | 说明                               |
|------------|------------------------|------------------------------------|
| 供电       | +3.3V 单电源           | V+ 接 3.3V, V- 接 GND              |
| 中点偏置   | +1.65V (V_REF)         | R49/R50 对 3.3V 分压               |
| 差模增益   | R44/R45 = **16.5×**    | 16.5kΩ / 1kΩ                       |
| 共模抑制比 | 100dB (OPA365)         | 抵消双线耦合的共模噪声              |
| 输入信号   | ±100mV (差分)          | 10A 峰值 × 10mΩ 采样电阻           |
| 输出范围   | 0 ~ 3.3V               | 1.65V ± 1.65V, 满量程利用 ADC      |
| 输出滤波   | R46(1kΩ) + C81(820pF)  | f_c ≈ 194kHz, 一阶 RC 低通          |

**信号处理流程**:

1. **Kelvin 四线采样** (10mΩ 采样电阻)
   - 采样电阻串联在 LCC 谐振主回路中, 10A 峰值电流 → ±100mV 差分电压
   - 每个焊盘独立引出信号线 (不走大电流), 消除 PCB 走线压降干扰
   - 2512 封装 3W 电阻, 实际功耗 0.5W, 6× 余量

2. **16.5× 差分放大**
   - 差模增益 G = R44/R45 = 16.5kΩ/1kΩ = **16.5**
   - ±100mV → **±1.65V**, 充分利用 STM32F334 ADC 的 0~3.3V 量程
   - OPA365 零交越失真特性保证 100kHz 正弦波过零时无失真

3. **1.65V 直流偏置**
   - V_REF = 3.3V × R50/(R49+R50) = 1.65V, 叠加至同相端
   - 输出中心电平 = 1.65V:
     - 无电流: ADC ≈ 2047
     - +10A 峰值: ADC ≈ 4095 (3.3V)
     - -10A 峰值: ADC ≈ 0 (0V)

4. **RC 低通滤波** (194kHz)
   - 滤除逆变桥 MOSFET 开关噪声 (几十 MHz) 和 EMI 干扰
   - 保证 100kHz 衰减波形干净无毛刺

**FOD 检测时序 (Ring-Down 法)**:
`
 正常运行           FOD检测窗口 (每~100ms)          正常运行
 ┌─────────┐       ┌──────────────────────┐       ┌─────────┐
 │ PWM驱动  │  →   │ 关PWM → 自由衰减振荡  │  →   │ PWM驱动  │
 │ 持续谐振  │       │ ADC高速采样 → 提取峰值 │       │ 持续谐振  │
 └─────────┘       └──────────────────────┘       └─────────┘

 Q值计算:  Q = π / ln(V_n / V_{n+1})
           V_n, V_{n+1} = 相邻两个衰减周期的峰值

 异物判定:  Q_实测 < Q_基准 × (1 - 阈值30%) → 存在金属异物 → 关PWM + 报警
`

**设计优势**:
- 检测在关 PWM 后自由衰减阶段进行, 不受接收端负载变化影响
- 16.5× 增益 + Kelvin 四线 + 差分放大 = 可识别硬币大小金属异物
- 仅 1 颗 OPA365 + 少量阻容, 成本低、可靠性高
### 5.4 异物检测 (FOD)
| 信号 | 说明                        |
|------|-----------------------------|
| FOD  | 异物检测信号, 接入 MCU 处理  |

---

## 6. 通信与人机接口

### 6.1 通信接口
| 接口   | 引脚         | 用途               |
|--------|-------------|--------------------|
| USART  | PB6(TX), PB7(RX) | 串口调试/上位机    |
| I2C    | PB8(SCL), PB9(SDA) | OLED 显示屏 (128×64) |
| SWD    | PA13(SWDIO), PA14(SWCLK) | 程序下载与调试 |

### 6.2 用户交互
| 元件 | 型号/值       | 说明            |
|------|---------------|-----------------|
| SW1  | TS-1088-AR02016 | 按键1         |
| SW2  | TS-1088-AR02016 | 按键2         |
| SW3  | TS-1088-AR02016 | 按键3         |
| LED1 | NCD0603Y2      | 指示灯1 (黄色)  |
| LED2 | NCD0603Y2      | 指示灯2 (黄色)  |
| LED4 | XL-1608SURC-06 | 指示灯4 (红色)  |
| LED5 | XL-1608SURC-06 | 指示灯5 (红色)  |
| H3   | 2.54-1×4P      | OLED 模块接口    |
| CN3  | PH1250-WT-04   | 扩展接口         |
| CN1  | XT60PW-M       | 电源输入接口     |

---

## 7. 控制策略分析

### 7.1 逆变器控制
```
控制目标: 产生 100kHz 方波/准方波驱动全桥逆变器
调制方式: 移相全桥 (Phase-Shifted Full-Bridge)

  Q11 ────┐         ┌────────
           │         │
  Q12 ────┘         └────────   ← 左半桥 (50% 占空比)

  Q13 ──────┐         ┌──────
             │         │
  Q14 ──────┘         └──────   ← 右半桥 (移相控制)

  移相角 φ ∈ [0, π] 控制输出功率
```

### 7.2 HRTIM 配置
- **通道**: HRTIM CHA1 (PA8) → PWM1, CHA2 (PA9) → PWM2
- **频率**: 100kHz (周期 = 11.76μs)
- **死区时间**: ~100-200ns (硬件死区插入)
- **移相分辨率**: 217ps (HRTIM 特性)

### 7.3 闭环控制
```
  ┌──────────┐    ┌───────────┐    ┌──────────┐    ┌──────────┐
  │ 设定功率 │───►│ PID 控制器 │───►│ 移相计算  │───►│ HRTIM PWM │
  │ P_ref    │    │           │    │ φ = f(P)  │    │          │
  └──────────┘    └───────────┘    └──────────┘    └──────────┘
                        ▲
                        │
                  ┌──────────┐
                  │ 功率估算  │ ◄── I_primary, V_primary
                  └──────────┘
```

### 7.4 保护功能
| 保护类型       | 实现方式                     |
|----------------|------------------------------|
| 过流保护       | LM393 比较器 + INA282 电流采样 |
| 过压保护       | ADC 电压监测                  |
| 过温保护       | MCU 内置温度传感器 / NTC      |
| 异物检测 (FOD) | 功率损耗分析                  |
| 紧急停止       | BKIN (PB12) 硬件刹车          |

---

## 8. 代码架构建议

### 8.1 工程目录结构
```
wirelessCharge/
├── code/                        ← MCU 固件代码
│   ├── Core/
│   │   ├── Inc/
│   │   │   ├── main.h
│   │   │   ├── stm32f3xx_hal_conf.h
│   │   │   ├── hrtim_config.h
│   │   │   ├── wireless_control.h
│   │   │   ├── power_sense.h
│   │   │   └── protection.h
│   │   └── Src/
│   │       ├── main.c
│   │       ├── stm32f3xx_it.c
│   │       ├── system_stm32f3xx.c
│   │       ├── hrtim_config.c
│   │       ├── wireless_control.c
│   │       ├── power_sense.c
│   │       └── protection.c
│   └── Drivers/
├── hardwork/                    ← 硬件设计文档
│   └── SCH_Schematic1_*.pdf
├── simulation/                  ← MATLAB/Simulink 仿真
│   ├── wireless_charge_params.m
│   ├── build_wireless_charge_model.m
│   └── wireless_charge_model.slx
└── PROJECT_REFERENCE.md         ← 本文件
```

### 8.2 核心模块设计

#### 8.2.1 `main.c` — 主函数框架
```c
int main(void)
{
    HAL_Init();
    SystemClock_Config();          // HSE → PLL → 72MHz

    // 初始化外设
    GPIO_Init();                    // LED, KEY, EN
    ADC_Init();                     // VS1, VS2, VP, VIN, I_sense, FOD
    UART_Init();                    // 调试串口
    I2C_Init();                     // OLED 显示

    // 核心初始化
    HRTIM_Init();                   // 100kHz PWM, 死区, 移相
    PowerSense_Init();              // 电压/电流采样校准
    Protection_Init();              // 过流/过压保护阈值
    WirelessControl_Init();         // PID 参数初始化

    // 启动流程
    PreCharge_Sequence();           // 预充电 (EN=0, 软启动)
    Protection_Enable();            // 使能保护
    OLED_ShowStatus();             // 显示状态

    while (1) {
        WirelessControl_Loop();     // 主控制循环 (1-10kHz)
        Protection_Check();         // 保护检查
        UI_Update();                // 按键/LED/OLED 更新
        HAL_Delay(1);
    }
}
```

#### 8.2.2 `hrtim_config.c` — HRTIM 配置
```c
/* 关键配置参数 */
#define PWM_FREQ_HZ         100000       // 100kHz 开关频率
#define HRTIM_TIMER_PERIOD  3600        // 144MHz / 2 / 100kHz = 847 (× 32x 倍频)
#define DEADTIME_NS         150         // 死区时间 150ns

/* 配置要点 */
// 1. HRTIM Master Timer 产生 100kHz 时基 (period ×32 模式)
// 2. Timer A = 左半桥 PWM (PA8), Timer B = 右半桥 PWM (PA9)
// 3. 死区由 HRTIM Deadtime 单元硬件生成
// 4. 移相通过调整 Timer B 的 Compare 值实现
// 5. Set-point 更新使用 HRTIM Burst DMA 模式
```

#### 8.2.3 `wireless_control.c` — 控制算法
```c
/* 控制参数 */
#define F_SW                100000.0f    // 开关频率
#define CTRL_LOOP_FREQ      5000.0f     // 控制环频率 5kHz

/* PID 参数 (需要调谐) */
typedef struct {
    float Kp, Ki, Kd;
    float integral;
    float prev_error;
    float integral_limit;
    float output_min, output_max;
} PID_Controller;

/* 状态机 */
typedef enum {
    STATE_IDLE,        // 空闲
    STATE_PRECHARGE,   // 预充电
    STATE_STARTUP,     // 软启动
    STATE_RUNNING,     // 正常运行
    STATE_PROTECTION,  // 保护状态
    STATE_FAULT        // 故障
} SystemState;
```

#### 8.2.4 `power_sense.c` — 功率与采样
```c
/* ADC 通道映射 (参考) */
// ADC1: VS1, VS2, VP, I_primary (INA282), VIN
// 采样率: 建议 ≥ 1MSPS per channel (用于峰值检测)

/* 关键计算 */
float GetPrimaryCurrent(void);       // I_primary = (ADC - offset) * scale
float GetPrimaryVoltage(void);       // V_primary = |VS1 - VS2|
float GetInputCurrent(void);         // I_in = V(ISNS) / 2.5mΩ
float CalculateActivePower(void);    // P = avg(V * I)
float CalculateApparentPower(void);  // S = Vrms * Irms
float GetFODLoss(void);              // FOD = P_in - P_calculated
```

#### 8.2.5 `protection.c` — 保护逻辑
```c
typedef struct {
    float i_primary_max;     // 初级电流上限
    float v_bus_max;         // 母线电压上限
    float v_bus_min;         // 母线电压下限
    float fod_threshold;     // FOD 功率损耗阈值
    float temp_max;          // 最高温度
    uint32_t fault_timeout;  // 故障恢复超时
} ProtectionConfig;

void Protection_Check(void)  // 每控制周期调用
{
    if (MeasuredCurrent() > config.i_primary_max)  Fault_OverCurrent();
    if (MeasuredVoltage() > config.v_bus_max)      Fault_OverVoltage();
    if (MeasuredVoltage() < config.v_bus_min)      Fault_UnderVoltage();
    if (CalculateFOD()    > config.fod_threshold)   Fault_FOD();
    if (MeasuredTemp()    > config.temp_max)        Fault_OverTemp();
}
```

---

## 9. 仿真参数 (MATLAB/Simulink)

| 参数        | 值          | 说明               |
|-------------|-------------|--------------------|
| f_sw        | 100 kHz      | 开关频率           |
| V_dc        | 100 V       | 直流母线电压 (仿真值, 实际硬件 48V) |
| L_p         | 40 μH       | 初级线圈电感       |
| L_s         | 40 μH       | 次级线圈电感       |
| k           | 0.3         | 耦合系数           |
| M           | 6.93 μH     | 互感               |
| C_p         | 63.3 nF     | 初级谐振电容       |
| C_s         | 63.3 nF     | 次级谐振电容       |
| R_p         | 0.1 Ω       | 初级线圈内阻       |
| R_s         | 0.1 Ω       | 次级线圈内阻       |
| R_load      | 10 Ω        | 负载电阻           |
| C_dc        | 100 μF      | 直流母线电容       |
| 拓扑        | LCC-LCC | LCC-LCC 谐振补偿      |

---

## 10. 快速调试清单

- [ ] **电源检查**: 48V → 12V → 5V → 3.3V 各级正常
- [ ] **晶振起振**: HSE 外部晶振正常, SYSCLK = 72MHz
- [ ] **SWD 连接**: ST-Link/J-Link 可正常连接下载
- [ ] **HRTIM 输出**: PA8/PA9 输出 100kHz 互补 PWM (示波器确认)
- [ ] **死区时间**: 上下管不直通 (测 HO/LO 波形)
- [ ] **ADC 采样**: 各通道读数正确
- [ ] **UART 输出**: 串口打印信息正常
- [ ] **I2C OLED**: 显示正常
- [ ] **预充电**: EN=0 时电感无电流
- [ ] **开环测试**: 固定移相角, 低功率验证谐振网络
- [ ] **闭环调参**: PID 参数整定
- [ ] **保护动作**: 模拟过流/过压确认保护生效

---

## 11. 关键注意事项

1. **BSC035N10NS5 MOSFET**: Vds_max=100V, 24V→48V 升压, MOSFET 100V 耐压有余量
2. **EG2104S**: 自举电容需在启动前预充电, #SD 低电平有效
3. **谐振电容组**: C53-C62 (470nF×10) 需确认串/并联接法 → 谐振频率匹配 100kHz
4. **HRTIM**: 是 STM32F334 的核心优势,务必使用 HRTIM 而非普通定时器
5. **死区时间**: 建议 150-200ns,确保上下管不直通
6. **FOD**: 对安全至关重要,需要精确功率计算
7. **电流采样**: INA282 增益 50×, 10mΩ 采样 → 0.5V/A 输出`n> **Demo**: Branch workflow test - 2026-05-12
