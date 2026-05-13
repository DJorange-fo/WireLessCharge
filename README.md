# Wireless Charging Project — 工程快速参考手册

> **主控芯片**: STM32F334C8T6  
> **拓扑结构**: LCC-LCC 无线电能传输  
> **谐振频率**: 100 kHz (WPC Qi 标准)  
> **更新日期**: 2026-05-13

---

## 1. 系统总体架构

```
 24V DC 输入
     │
     ├─► [TPS43061 Boost] ──► ~48V DC 母线 ──► [全桥逆变器] ──► [LCC谐振网络] ──► 发射线圈
     │    24V→48V               (V_BUS)           (4× MOSFET)      (Lp + Cp + LCC补偿)
     │
     ├─► [TPS54360 Buck] ──► 12V ──► [TPS54202 Buck] ──► 5V ──► [AMS1117 LDO] ──► 3.3V
     │                          │                       │                │
     │                          │                       │                └─► STM32F334, 传感器
     │                          └─► 栅极驱动 EG2104S    └─► OLED, LED
     │
     └─► [采样电路] ◄── VS1, VS2 (电压), INA282 (电流), OPA365 (信号调理)
               │
               └─► STM32F334C8T6 TIM2/TIM3 PWM 输出 (PB5/PB3), ADC / 比较器 / 运放
```

> **注**: 系统输入为 24V DC, 经 TPS43061 升压后得到 ~48V 直流母线, 再供给全桥逆变器。

---

## 2. 电源树 (Power Tree)

| 电源轨    | 来源芯片           | 电压   | 用途                         |
|-----------|--------------------|--------|------------------------------|
| VIN       | 外部输入            | 24V    | 系统总电源                   |
| V_BUS     | TPS43061 Boost 输出 | 48V    | 直流母线, 供给全桥逆变器     |
| 12V       | TPS54360 Buck       | 12V    | EG2104S 逻辑供电, 中间电源   |
| +5V       | TPS54202 Buck       | 5V     | OLED, 比较器, LED, USB相关   |
| 3.3V      | AMS1117-3.3 LDO     | 3.3V   | STM32F334, INA282, OPA365    |
| VCC_24V   | TPS43061 内部 LDO   | 24V    | EG2104S 栅极驱动供电         |

**关键电源芯片**:
- **U1 TPS43061RTER**: 同步升压控制器, 24V→48V, 集成 LDO (VCC=~7V)
- **U3 TPS54360BDDAR**: 60V/3.5A 降压转换器, VIN(24V)→12V
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
| **V_in** | **24V DC** | **系统输入电压 (24V→48V 升压, MOSFET 100V 余量充足)** |

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

**PWM 映射 (当前硬件实现)**:

| 信号 | STM32 引脚 | 定时器通道    | 目标               | 说明           |
|------|-----------|--------------|--------------------|----------------|
| PWM1 | **PB5**   | **TIM3_CH2** | U16 (EG2104S) IN  | 左半桥 Q11/Q12 |
| PWM2 | **PB3**   | **TIM2_CH2** | U17 (EG2104S) IN  | 右半桥 Q13/Q14 |
| EN1  | **PB4**   | GPIO Output  | U16 #SD           | 左半桥使能     |
| EN2  | **PA15**  | GPIO Output  | U17 #SD           | 右半桥使能     |

### 3.3 谐振网络 (Resonant Tank)

**LCC-LCC 拓扑说明**:
```
 初级侧:                         次级侧:
  V_inv -- L_f1 --+-- C_p1 --+-- L_p -- C_f1     C_f2 -- L_s --+-- C_p2 --+-- L_f2 -- R_load
                   |          |                                  |          |
                  GND        GND                                GND        GND
```
- **L_f1/L_f2**: 补偿电感 (串联谐振, f=100kHz)
- **C_p1/C_p2**: 并联谐振电容
- **C_f1/C_f2**: 串联补偿电容 (与线圈电感 L_p/L_s 谐振)
- 谐振条件: ω² · L_f · C_p = 1,  ω² · (L_p-L_f) · C_f = 1
- LCC-LCC 优势: 恒流输出特性, 对负载变化不敏感, 易于实现 ZVS

| 元件    | 值                 | 说明                    |
|---------|--------------------|-------------------------|
| L4      | 2.2μH (PCB上)      | 初级补偿电感 L_f1       |
| Lp      | 40μH (发射线圈)    | 初级线圈电感            |
| Cp      | ~63.3nF @100kHz    | 谐振电容 (C53-C62)     |
| C53-C62 | 470nF x 10         | 谐振电容组 (需确认串/并联) |
| C85     | 470μF              | 直流母线滤波            |

---

## 4. 主控芯片 STM32F334C8T6

### 4.1 芯片特性
| 特性              | 规格                           |
|-------------------|--------------------------------|
| 内核              | ARM Cortex-M4F @ 72MHz         |
| Flash             | 64KB                           |
| SRAM              | 12KB (含 4KB CCM)              |
| HRTIM             | 高分辨率定时器 (217ps 分辨率, **当前未使用**) |
| ADC               | 2×12-bit, 5MSPS, 最多21通道    |
| DAC               | 3×12-bit                       |
| 比较器            | 3× 超快速模拟比较器            |
| 运放              | 1× 内置运放 (PGA)              |
| 高级定时器        | 1× 16-bit 电机控制定时器       |
| 通用定时器        | 5× (TIM2/TIM3 用于 PWM)        |
| 通信接口          | CAN, USART, I2C, SPI           |

### 4.2 引脚分配
| 引脚 | 功能      | 连接目标           | 说明              |
|------|-----------|-------------------|-------------------|
| PB5  | PWM1      | U16 IN            | **TIM3_CH2**, 左半桥 |
| PB3  | PWM2      | U17 IN            | **TIM2_CH2**, 右半桥 |
| PB4  | EN1       | U16 #SD           | GPIO Output, 左桥使能 |
| PA15 | EN2       | U17 #SD           | GPIO Output, 右桥使能 |
| PB15 | KEY       | 按键 (SW1)        | Input Pull-Up, 开关控制 |
| PA13 | SWDIO     | SWD 接口          | 调试接口          |
| PA14 | SWCLK     | SWD 接口          | 调试接口          |
| PB6  | TX        | UART TX           | 串口通信          |
| PB7  | RX        | UART RX           | 串口通信          |
| PB8  | SCL       | OLED SCL          | I2C 时钟          |
| PB9  | SDA       | OLED SDA          | I2C 数据          |
| PB12 | BKIN      | 刹车/急停输入     | TIM1 BKIN         |
| PC13 | —         | —                 | RTC / Tamper      |
| PC14 | OSC32_IN  | 32.768kHz 晶振    | 可选 RTC 时钟     |
| PC15 | OSC32_OUT | 32.768kHz 晶振    | 可选 RTC 时钟     |
| PF0  | OSC_IN    | 外部晶振          | HSE 时钟输入      |
| PF1  | OSC_OUT   | 外部晶振          | HSE 时钟输出      |
| PA0-PA7, PA10-12, PB0-PB2, PB10-11, PB13-14 | — | — | 通用 IO (预留) |
| NRST | NRST      | 复位按键          | 系统复位          |
| BOOT0| BOOT0     | 启动选择          | 低电平 Flash 启动  |
| BOOT1| BOOT1     | 启动选择          | PB2 复用          |

**已确认的 IO**: PWM1(PB5), PWM2(PB3), EN1(PB4), EN2(PA15), KEY(PB15), TX(PB6), RX(PB7), SCL(PB8), SDA(PB9), BKIN(PB12)

### 4.3 引脚冲突与 JTAG 释放

PB3、PB4、PA15 默认为 JTAG 调试引脚 (JTDO, NJTRST, JTDI)。CubeMX 选择 "Serial Wire" 后理论上应释放, 但实际生成的 HAL_MspInit() 不会自动写 SYSCFG_CFGR1 寄存器, 导致这些引脚被硬件锁死在调试模式。

**解决方法** (已在 `main.c` SysInit 区实现):
```c
SYSCFG->CFGR1 = (SYSCFG->CFGR1 & ~(0x7UL << 26)) | (0x2UL << 26);
// bit[28:26]=010: 关闭 JTAG-DP, 保留 SW-DP (PA13/SWDIO, PA14/SWCLK)
```

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
| **灵敏度** | **0.125V/A** | 2.5mΩ × 50 = 0.125Ω 跨阻 |

**AC 线圈电流 (OPA365)**:
| 元件 | 型号/值       | 说明                               |
|------|---------------|------------------------------------|
| R36  | 10mΩ          | 采样电阻, 串联在 LCC 谐振主回路     |
| R36  | 2512封装 3W   | 实际 0.5W 功耗, 6× 余量            |
| L5   | YI201209U101  | 共模扼流圈, 抑制共模噪声            |
| U7   | OPA365AIDR    | 差分放大 16.5×, 详见 §5.3.1        |
| 输出 | 0~3.3V        | 1.65V 中心, ±1.65V 摆幅 → ADC      |
| **灵敏度** | **0.165V/A** | 10mΩ × 16.5 = 0.165Ω 跨阻 |

### 5.2 电压采样
| 信号  | 来源        | 说明                        |
|-------|-------------|-----------------------------|
| VS1   | 发射回路    | 谐振网络电压采样点 1        |
| VS2   | 发射回路    | 谐振网络电压采样点 2        |
| VP    | 发射回路    | 初级线圈电压采样            |
| VIN   | 24V 输入端  | 输入电压监测                |
| V_BUS | 48V 直流母线 | Boost 输出电压监测          |
| VCC_24V | TPS43061 LDO | 驱动电压监测              |

### 5.3 信号调理
| 元件 | 型号          | 说明                     |
|------|---------------|--------------------------|
| U7   | OPA365AIDR    | 轨到轨运放, 信号调理      |
| U11  | LM393DR       | 双路比较器 (过流/过压保护) |
| U18  | (未标注)      | 运放 / 比较器 (TBD)       |
| U19  | (未标注)      | 运放 / 比较器 (TBD)       |

#### 5.3.1 OPA365 差分放大器 — Ring-Down FOD Q值检测前端

U7 (OPA365AIDR) 构成**单电源差分转单端放大器**，是金属异物检测 (FOD) 的专用前端，
采用工程最成熟的 **Ring-Down 自由衰减法** 通过捕捉谐振回路的能量损耗速度 (Q值) 判断异物。

```
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
```

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
```
 正常运行           FOD检测窗口 (每~100ms)          正常运行
 ┌─────────┐       ┌──────────────────────┐       ┌─────────┐
 │ PWM驱动  │  →   │ 关PWM → 自由衰减振荡  │  →   │ PWM驱动  │
 │ 持续谐振  │       │ ADC高速采样 → 提取峰值 │       │ 持续谐振  │
 └─────────┘       └──────────────────────┘       └─────────┘

 Q值计算:  Q = π / ln(V_n / V_{n+1})
           V_n, V_{n+1} = 相邻两个衰减周期的峰值

 异物判定:  Q_实测 < Q_基准 × (1 - 阈值30%) → 存在金属异物 → 关PWM + 报警
```

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
| SW1  | TS-1088-AR02016 | 按键1 (PB15, 全桥开关控制) |
| SW2  | TS-1088-AR02016 | 按键2         |
| SW3  | TS-1088-AR02016 | 按键3         |
| LED1 | NCD0603Y2      | 指示灯1 (黄色)  |
| LED2 | NCD0603Y2      | 指示灯2 (黄色)  |
| LED4 | XL-1608SURC-06 | 指示灯4 (红色)  |
| LED5 | XL-1608SURC-06 | 指示灯5 (红色)  |
| H3   | 2.54-1×4P      | OLED 模块接口    |
| CN3  | PH1250-WT-04   | 扩展接口         |
| CN1  | XT60PW-M       | 电源输入接口 (24V) |

---

## 7. 控制策略分析

### 7.1 PWM 生成: TIM2/TIM3 主从同步移相方案

当前硬件使用 **TIM3 (Master) + TIM2 (Slave)** 实现移相全桥 PWM。该方案已在硬件上验证,
可稳定输出正确的全桥逆变波形。

```
时钟: 72MHz TIM Clock, Prescaler=0, Period=719 → f_pwm = 72MHz/720 = 100kHz

TIM3 Master (PB5, 左半桥):
  0 ────── CH1=phase ────── CH2=360 ────── 719 → 0
         ┌─── PWM1 (PB5, TIM3_CH2) 50% ───┐
         │ HIGH (0~359)   │ LOW (360~719) │
         
         CH1 匹配 → OC1REF → TRGO ────→ TIM2 Reset
                                        ↓
TIM2 Slave (PB3, 右半桥):               0 ──── 360 ──── 719 → ...
                                       ┌─── PWM2 (PB3, TIM2_CH2) 50% ───┐
                                       │ HIGH (0~359) │ LOW (360~719)  │
```

**移相原理**:
- 修改 TIM3 CH1 的 Compare 值 (0~360) 即可改变两路 PWM 之间的相位差
- phase=0: 两路同相
- phase=360: 两路反相 (180° 移相)
- 移相角 φ ∈ [0°, 180°] 对应 CH1 compare ∈ [0, 360]
- 移相分辨率: 72MHz / 100kHz = 720 counts → 每 count = 0.5° (13.9ns)

**定时器配置要点**:
| 定时器 | 角色   | 通道  | 引脚 | 模式          | 关键配置                     |
|--------|--------|-------|------|---------------|------------------------------|
| TIM3   | Master | CH1   | —    | Output Compare (无引脚) | TRGO=OC1REF, Master/Slave Enable |
| TIM3   | Master | CH2   | PB5  | PWM1 50%      | Pulse=360 (占空比 50%)       |
| TIM2   | Slave  | CH2   | PB3  | PWM1 50%      | Slave Mode=Reset, Trigger=ITR2 |

### 7.2 死区时间

EG2104S 栅极驱动芯片内置 **~520ns 死区时间**, 上下管不会直通, 软件无需额外处理。
这一硬件特性简化了 TIM2/TIM3 的配置——无需在定时器层面插入死区。

### 7.3 闭环控制

> ⚠️ **当前状态**: 闭环控制尚未实现, 以下为规划中的控制框图。

```
  ┌──────────┐    ┌───────────┐    ┌──────────┐    ┌──────────────-┐
  │ 设定功率 │───►│ PID 控制器 │───►│ 移相计算  │───►│ TIM2/TIM3 PWM │
  │ P_ref    │    │           │    │ φ = f(P)  │    │ fz_hbridge    │
  └──────────┘    └───────────┘    └──────────┘    └──────────────-┘
                         ▲
                         │
                  ┌──────────┐
                  │ 功率估算  │ ◄── I_primary (INA282), V_primary (VS1/VS2)
                  └──────────┘
```

### 7.4 保护功能 (规划中)
| 保护类型       | 实现方式                     |
|----------------|------------------------------|
| 过流保护       | LM393 比较器 + INA282 电流采样 |
| 过压保护       | ADC 电压监测                  |
| 过温保护       | MCU 内置温度传感器 / NTC      |
| 异物检测 (FOD) | Ring-Down Q值自由衰减法       |
| 紧急停止       | BKIN (PB12) 硬件刹车          |

---

## 8. 代码架构

### 8.1 当前工程目录结构 (code/wpt_v2/)

```
code/wpt_v2/
├── Core/
│   ├── Inc/
│   │   ├── main.h                   ← 引脚宏定义 (EN1_Pin, PWM1_Pin 等)
│   │   ├── stm32f3xx_hal_conf.h
│   │   ├── stm32f3xx_it.h
│   │   ├── tim.h                    ← TIM2/TIM3 句柄声明
│   │   ├── gpio.h
│   │   └── fz_hbridge.h             ← H 桥接口 (init/enable/disable/set_phase)
│   └── Src/
│       ├── main.c                   ← 主程序: JTAG释放 + 初始化 + 按键消抖控制
│       ├── tim.c                    ← TIM2(从)/TIM3(主) 100kHz 移相配置
│       ├── gpio.c                   ← EN1/EN2/KEY GPIO 初始化
│       ├── fz_hbridge.c             ← H 桥实现
│       ├── stm32f3xx_it.c
│       ├── stm32f3xx_hal_msp.c
│       └── system_stm32f3xx.c
├── MDK-ARM/                         ← Keil MDK 工程
├── wpt_v2.ioc                       ← CubeMX 配置
└── README.md                        ← v2 固件说明
```

### 8.2 已实现的核心模块

#### 8.2.1 `main.c` — 主函数
```c
int main(void)
{
    HAL_Init();
    SystemClock_Config();            // HSE 8MHz → PLL×9 → SYSCLK=72MHz

    // SysInit: JTAG 释放 + 上电安全
    SYSCFG->CFGR1 = ...;             // 关闭 JTAG-DP, 保留 SW-DP
    HAL_GPIO_WritePin(EN1, SET);     // 保持 EN=HIGH (关断态)
    HAL_GPIO_WritePin(EN2, SET);
    HAL_Delay(50);                   // ST-Link 连接窗口

    MX_GPIO_Init();
    MX_TIM2_Init();                  // 从定时器, PB3 右半桥 PWM
    MX_TIM3_Init();                  // 主定时器, PB5 左半桥 PWM, TRGO=OC1REF
    fz_hbridge_init();
    fz_hbridge_set_phase(180);       // 初始反相 (180° 移相)

    while (1) {
        // 按键消抖 + 开关控制
        if (debounced_key_press)
            fz_hbridge_toggle();     // PB15 按键切换 PWM 输出
    }
}
```

#### 8.2.2 `fz_hbridge.c` — H 桥控制模块
```c
#define HBRIDGE_PERIOD      720U
#define HBRIDGE_PHASE_MAX   (HBRIDGE_PERIOD / 2U)  // 360 = 180° 移相

void fz_hbridge_init(void);           // 初始化, EN=0 关断, 设 PWM 50% 占空比
void fz_hbridge_enable(void);         // 使能 EN1/EN2 + 启动 TIM2/TIM3 PWM
void fz_hbridge_disable(void);        // 停止 PWM + 关 EN1/EN2
void fz_hbridge_toggle(void);         // 翻转使能状态
int  fz_hbridge_is_enabled(void);     // 查询状态
void fz_hbridge_set_phase(uint16_t);  // 动态设置移相角 [0, 360]
```

#### 8.2.3 `tim.c` — TIM2/TIM3 初始化
```c
/* TIM2 (Slave) 关键配置 */
htim2.Init.Period = 719;
sSlaveConfig.SlaveMode = TIM_SLAVEMODE_RESET;   // TRGO 触发时复位计数器
sSlaveConfig.InputTrigger = TIM_TS_ITR2;        // 触发源 = TIM3 TRGO
// PWM CH2 (PB3): 50% 占空比

/* TIM3 (Master) 关键配置 */
htim3.Init.Period = 719;
sMasterConfig.MasterOutputTrigger = TIM_TRGO_OC1REF;  // CH1 匹配时发 TRGO
sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_ENABLE;
// CH1: Output Compare (内部, 无引脚, 用作移相参考)
// CH2: PWM 50% (PB5, 左半桥)
```

### 8.3 待实现的模块

| 模块                  | 文件 (规划)        | 说明                                |
|-----------------------|-------------------|-------------------------------------|
| ADC 采样              | (main.c 扩展)     | VS1/VS2/VP/VIN/I_primary/I_ac 六路  |
| UART 调试             | (main.c 扩展)     | PB6/PB7, 115200bps                  |
| I2C OLED 驱动         | (新增)            | PB8/PB9, SSD1306 128×64             |
| PID 控制器            | wireless_control.c| 功率闭环, 移相角自动调节            |
| 功率采样与计算        | power_sense.c     | 有效值/有功功率/视在功率            |
| 保护逻辑              | protection.c      | 过流/过压/过温/FOD/急停             |
| FOD 异物检测          | fod.c             | Ring-Down Q值法, OPA365 前端        |
| 系统状态机            | (集成 main.c)     | IDLE→PRECHARGE→STARTUP→RUNNING→FAULT|

---

## 9. 仿真参数 (MATLAB/Simulink)

> ⚠️ **注意**: 当前 `simulation/` 目录下的 MATLAB 模型使用 **85kHz Series-Series** 参数,
> 与硬件实际 (100kHz LCC-LCC) 不一致。以下为硬件设计目标参数, 仿真模型待更新。

| 参数        | 值          | 说明               |
|-------------|-------------|--------------------|
| f_sw        | 100 kHz      | 开关频率           |
| V_dc        | 48 V        | 直流母线电压 (24V Boost → 48V) |
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
| 拓扑        | LCC-LCC     | LCC-LCC 谐振补偿    |

---

## 10. 快速调试清单

- [ ] **电源检查**: 24V → 48V(Boost) → 12V → 5V → 3.3V 各级正常
- [ ] **晶振起振**: HSE 外部晶振正常, SYSCLK = 72MHz
- [x] **SWD 连接**: ST-Link/J-Link 可正常连接下载
- [x] **PWM 输出**: PB5(TIM3_CH2) / PB3(TIM2_CH2) 输出 100kHz 移相 PWM (示波器确认)
- [x] **死区时间**: EG2104S 内置 ~520ns 死区, 上下管不直通
- [ ] **ADC 采样**: 各通道读数正确
- [ ] **UART 输出**: 串口打印信息正常
- [ ] **I2C OLED**: 显示正常
- [x] **预充电/安全启动**: EN=HIGH 关断态, 上电安全
- [ ] **开环测试**: 固定移相角, 低功率验证谐振网络
- [ ] **闭环调参**: PID 参数整定
- [ ] **保护动作**: 模拟过流/过压确认保护生效

---

## 11. 关键注意事项

1. **BSC035N10NS5 MOSFET**: Vds_max=100V, 24V→48V 升压余量充足
2. **EG2104S**: 自举电容需在启动前预充电, #SD 低电平有效 (EN=LOW=关断, EN=HIGH=使能)
3. **谐振电容组**: C53-C62 (470nF×10) 需确认串/并联接法 → 谐振频率匹配 100kHz
4. **PWM 引脚**: 当前使用 PB5/PB3 (TIM3_CH2/TIM2_CH2), 配合 PB4/PA15 为 EN, 注意 JTAG 引脚冲突
5. **死区时间**: EG2104S 内置 ~520ns, 无需软件死区
6. **FOD**: 对安全至关重要, 需要精确功率计算
7. **电流采样**: INA282 增益 50×, 2.5mΩ 采样 → 0.125V/A 输出; OPA365 16.5× + 10mΩ → 0.165V/A 输出

---

## 附录: 开发中遇到的问题与解决

| 问题 | 原因 | 解决方法 |
|------|------|----------|
| "Could not stop Cortex-M device!" | PB3 配置为 TIM2_CH2 覆盖 SWO, SWD 无法连接 | SysInit 中加 HAL_Delay(50) 留出连接窗口 |
| 上电后 PB4/PA15 一直 2.8V, 代码不执行 | 27.12MHz 晶振配 PREDIV=/1 PLL×9=244MHz 超频 | 换 8MHz 晶振 |
| PWM1 (PB5) 有信号, PWM2 (PB3) 无信号 | JTAG 未禁用, PB3 硬件锁死在 JTDO 模式 | SYSCFG->CFGR1 写 010 释放 JTAG |
| TIM3 TRGO=RESET 导致 TIM2 不触发 | TRGO 只在 Reset 事件发脉冲 | CubeMX 中改 TRGO 为 OC1REF |
| 上电过流 | SysInit 中误将 EN 拉 LOW 打开了栅极驱动 | 改 EN=LOW→HIGH, 保持关断安全态 |
