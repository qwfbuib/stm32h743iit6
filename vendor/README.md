
# 第三方源码

这些文件来自原 IoT Shark V2 示例包的 `iot-shark-v2-rtt` 目录。只带入首版构建所需的 RT-Thread 内核和 FinSH、Cortex-M7 移植、CMSIS 启动与头文件，以及 STM32 HAL 驱动。

| 目录 | 原工程来源 | 许可 |
| --- | --- | --- |
| `rt-thread/` | `rt-thread/include`、`src`、`libcpu/arm/cortex-m7` 和 `components/finsh`。内核版本为 4.1.0。 | [Apache-2.0](rt-thread/LICENSE) |
| `cmsis/Core/` | `libraries/CMSIS/Include`。`core_cm7.h` 记录版本 5.0.8。 | [Apache-2.0](cmsis/LICENSE-APACHE-2.0) |
| `cmsis/Device/` | `libraries/CMSIS/Device/ST/STM32H7xx`。其中 GNU 启动文件调用 RT-Thread 的 `entry`。 | [BSD-3-Clause](hal/LICENSE-BSD-3-Clause) |
| `hal/` | `libraries/STM32H7xx_HAL_Driver`。HAL 源码记录版本 1.7.0。 | [BSD-3-Clause](hal/LICENSE-BSD-3-Clause) |

依赖文件中的版权和许可声明随源码保留。新写的固件、诊断和测试代码采用项目根目录的 [Apache-2.0](../LICENSE)。
