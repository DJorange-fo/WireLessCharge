# CLAUDE.md — Wireless Charge (WPT + FOD)

STM32F334C8T6 固件开发，LCC-LCC 谐振拓扑，100kHz，24V→48V 无线输电 + 金属异物检测。

## 行为准则（Karpathy Guidelines）

**Tradeoff:** 这些准则偏向谨慎而非速度。对简单任务（改错字、单行修复）可灵活处理。

### 1. Think Before Coding — 先想再写

**不要假设。不要隐藏困惑。呈现权衡。**

实现之前：
- 明确陈述假设。如果不确定，先问。
- 如果有多种解释，全部呈现 —— 不要悄悄选一个。
- 如果有更简单的方法，说出来。该反问时就反问。
- 如果有不清晰的，停下来。说出哪里困惑。问。

### 2. Simplicity First — 简单优先

**只写解决问题的最小代码。不要超前设计。**

- 不写超出需求的功能。
- 不为单次使用的代码做抽象。
- 不添加未被请求的"灵活性"或"可配置性"。
- 不为不可能的场景写错误处理。
- 如果写了 200 行可以缩减到 50 行，重写。

回头问自己："高级工程师会觉得这过度复杂吗？" 如果是，简化它。

### 3. Surgical Changes — 精准修改

**只改必须改的。只清理自己造成的混乱。**

编辑已有代码时：
- 不要"改进"相邻的代码、注释或格式。
- 不重构没有坏的东西。
- 匹配现有风格，即使你会用不同方式。
- 如果你注意到无关的死代码，提出来 —— 不要删除它。

当你的修改产生孤立代码时：
- 移除你的修改导致的未使用的 import/变量/函数。
- 不删除已有的死代码，除非用户要求。

测试标准：每个被修改的行都应该能追溯到用户的需求。

### 4. Goal-Driven Execution — 目标驱动执行

**定义成功标准。循环直到验证通过。**

将命令式任务转化为可验证的目标：
- "添加验证" → "为无效输入写测试，然后让测试通过"
- "修 bug" → "写一个能复现的测试，然后让它通过"
- "重构 X" → "确保测试在前后都通过"

对多步任务，给出简要计划：
```
1. [步骤] → 验证: [检查方法]
2. [步骤] → 验证: [检查方法]
3. [步骤] → 验证: [检查方法]
```

## 开发流程：Plan → Do → Verify

本项目采用分阶段开发。每实现一个模块，遵循以下流程：

### Phase 0: 文档发现（每次必须先做）
1. 阅读芯片参考手册相关章节（TIM/ADC/UART/I2C/DMA）
2. 阅读 STM32F3xx HAL 库驱动文档
3. 阅读现有代码中相关模式（如 tim.c 中的 PWM 配置）
4. 列出"允许使用的 API 清单"和要避免的反模式

### 每个实现阶段
1. **做什么** — 明确、具体、可验证的目标
2. **参考文档** — 引用具体文件/行号
3. **验证清单** — 如何证明这个阶段成功
4. **反模式警告** — 不要做什么（不要发明 API、不要加参数）

### 阶段完成后
1. 运行验证清单
2. 检查反模式（grep 已知的不良模式）
3. 确认编译通过（Keil MDK-ARM）

## 项目结构速览

```
code/wpt_v2/          — 固件（Keil MDK-ARM，HAL 库）
  Core/Inc/           — 头文件（main.h, fz_hbridge.h, tim.h, gpio.h...）
  Core/Src/           — 源文件（main.c, fz_hbridge.c, tim.c, gpio.c...）
  MDK-ARM/            — Keil 工程文件 + 编译输出
simulation/           — MATLAB/Simulink 仿真（待更新）
hardwork/             — 硬件原理图 PDF
build/                — CMake 构建（已废弃）
```

## 关键硬件参数

- MCU: STM32F334C8T6, 72MHz, 64KB Flash, 12KB SRAM
- PWM: TIM3(主) + TIM2(从), 100kHz, 相移控制, 50% 占空比
- 栅极驱动: EG2104S, EN 高有效, 内置 ~520ns 死区
- 调试: SWD (PA13/PA14), JTAG 已释放
- 通信: UART (PB6/PB7), I2C (PB8/PB9 → SSD1306 OLED)
- 采样: 6 路 ADC (VS1, VS2, VP, VIN, Idc, Iac)

## FreeRTOS 集成 Skill（坑与反模式）

> **适用范围**: STM32F334 + Keil MDK + HAL V1.11.6 + FreeRTOS V10.0.1 + ARMCC V5.06
> 每次在此项目上做 FreeRTOS 相关工作时，先读此节。

### CubeMX 兼容规则（铁律）

- **所有对 CubeMX 生成文件的修改必须放在 `USER CODE BEGIN/END` 块内**。其余代码会在 CubeMX 重新生成时被覆盖。
- `FreeRTOSConfig.h` 和新模块文件 (`syscfg.c`, `app_tasks.c`) 是手动创建的，CubeMX 不会碰。
- `wpt_v2.uvprojx` 的 include path 和源文件列表在 CubeMX 重新生成时会丢失，需要手动恢复。

### SVC_Handler / PendSV_Handler 冲突解决

FreeRTOS 移植层（`port.c`）的 `vPortSVCHandler`/`xPortPendSVHandler` 是 `__asm` 函数，必须直接作为 ISR 入口（不能用 C 包装函数调用，否则破坏异常栈帧）。但 CubeMX 生成的 `stm32f3xx_it.c` 也定义了 `SVC_Handler`/`PendSV_Handler` → 链接冲突。

**解决方案（双重宏重命名）**：

1. `FreeRTOSConfig.h` 中：
   ```c
   #define vPortSVCHandler      SVC_Handler
   #define xPortPendSVHandler   PendSV_Handler
   // SysTick 不映射 — 与 HAL 共用
   ```
2. `stm32f3xx_it.c` 的 `USER CODE BEGIN Includes` 中：
   ```c
   #ifdef USE_FREERTOS
   #define SVC_Handler      __unused_CubeMX_SVC_Handler   // 重命名为死函数
   #define PendSV_Handler   __unused_CubeMX_PendSV_Handler
   #endif
   ```

结果：CubeMX 的 `SVC_Handler` → 变成 `__unused_CubeMX_SVC_Handler`（不链接）；FreeRTOS 的 `vPortSVCHandler` → 变成 `SVC_Handler`（真正的 ISR）。

### SysTick 共享

```c
// stm32f3xx_it.c, USER CODE BEGIN SysTick_IRQn 1
#ifdef USE_FREERTOS
if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
    xPortSysTickHandler();
}
#endif
```

`HAL_IncTick()` 由 CubeMX 生成（在 USER CODE 块外），`xPortSysTickHandler()` 在 USER CODE 块内添加。

### configASSERT 死锁陷阱

FreeRTOS 默认 `configASSERT` 失败后调用 `taskDISABLE_INTERRUPTS(); for(;;);` → 静默死锁，SysTick 停止 → 所有 HAL_Delay 永久挂。
**在 bring-up 阶段必须禁用**：
```c
#define configASSERT( x )    ((void)(x))
```
确认调度器正常运行后再恢复。

### configMAX_SYSCALL_INTERRUPT_PRIORITY 汇编不兼容

ARM Compiler 5 的 `__asm` 函数中无法展开 C 宏表达式。必须硬编码：
```c
// 错误: #define configMAX_SYSCALL_INTERRUPT_PRIORITY  (5 << (8 - configPRIO_BITS))
// 正确:
#define configMAX_SYSCALL_INTERRUPT_PRIORITY       0x50  //   5 << 4
#define configKERNEL_INTERRUPT_PRIORITY            0xF0  // 0xf << 4
```

### STM32F3 HAL V1.11.6 限制

- `USE_RTOS` **必须设为 0**。此 HAL 版本不支持 `USE_RTOS=1`（编译报错 `#error "USE_RTOS should be 0"`）。
- HAL 锁使用忙等形式，不影响 FreeRTOS 任务安全（只要不同任务不并发操作同一 HAL 句柄）。

### 内存预算（12KB SRAM）

| 项目 | 大小 |
|------|------|
| FreeRTOS 内核 (tasks/queue/list) | ~3KB |
| 堆 (configTOTAL_HEAP_SIZE) | 4096B |
| Idle TCB + 栈 (128w) | ~584B |
| Controller TCB + 栈 (200w) | ~872B |
| Peripheral TCB + 栈 (200w) | ~872B |
| 应用静态数据 | ~2KB |
| **总计** | **~10.3KB / 12KB** |

configUSE_TIMERS=0 节省 ~600B。增加任务前先在 `heap_4.c` 中验证堆余量。

### 新增模块的模板

创建新模块文件时：
1. `Core/Inc/my_module.h` → 纯头文件
2. `Core/Src/my_module.c` → `#include "main.h"` + 模块实现
3. 在 `app_tasks.c` 中 `#include "my_module.h"` 并在对应 task 中调用
4. 在 `.mxproject` 和 `wpt_v2.uvprojx` 中注册新文件

### Keil 工程：FreeRTOS 源文件路径

```
FreeRTOS 内核来源:
  C:/Users/DxjCQ/STM32Cube/Repository/STM32Cube_FW_F3_V1.11.6/
    Middlewares/Third_Party/FreeRTOS/Source/

Keil Include Paths 需添加:
  - .../FreeRTOS/Source/include
  - .../FreeRTOS/Source/portable/RVDS/ARM_CM4F

Keil 源文件 (6个):
  - tasks.c, queue.c, list.c, timers.c
  - .../portable/MemMang/heap_4.c
  - .../portable/RVDS/ARM_CM4F/port.c

Preprocessor Define: USE_FREERTOS
```

## 当前状态与待实现模块

已完成:
- 100kHz 相移 PWM 生成 + H 桥控制 API
- GPIO 初始化和按键消抖（FreeRTOS task 化）
- JTAG 释放（PB3/PB4/PA15 复用为外设功能）
- **FreeRTOS 多任务架构**（vTaskController 1ms + vTaskPeripheral 50ms）
- **模块化代码结构**（syscfg.c, app_tasks.c, fz_hbridge.c）

待实现（推荐顺序）:
1. ADC 采样（6 通道）
2. UART 调试输出
3. 功率计算 + 保护逻辑（过流/过压）
4. PID 闭环控制
5. I2C OLED 显示
6. FOD 异物检测（Ring-Down Q 值法）
7. 系统状态机
