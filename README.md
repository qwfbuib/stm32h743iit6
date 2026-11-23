
# IoT Shark V2 固件 · 第一阶段

面向 IoT Shark V2.0.0 核心板和底板（STM32H743IIT6、25 MHz HSE）。固件使用 RT-Thread 4.1.0，提供 USART2 控制台、LED 心跳、SDRAM 校验和经典 CAN 诊断。板级初始化和应用使用 C17，CMake 与 Ninja 可在命令行独立构建。

## Windows 构建

安装 [CMake 3.28 或更新版本](https://cmake.org/download/)、[Ninja 1.11 或更新版本](https://github.com/ninja-build/ninja/releases) 和 [Arm GNU Toolchain 14.3.Rel1](https://developer.arm.com/tools-and-software/gnu-toolchain#Downloads)。Arm 工具链选 Windows x86-64、裸机目标 `arm-none-eabi`。将 `cmake` 和 `ninja` 加入 PATH。主机测试另需本机 GCC 或 Clang，例如 MinGW-w64 GCC。

在本目录打开 PowerShell。把工具链路径替换为自己的解压目录。

```powershell
$env:ARM_GNU_TOOLCHAIN_ROOT = 'C:\Tools\arm-gnu-toolchain-14.3.rel1-mingw-w64-x86_64-arm-none-eabi'
cmake --preset board-debug
cmake --build --preset board-debug
cmake --preset board-release
cmake --build --preset board-release
```

也可以将指定版本的 `arm-none-eabi-gcc` 及配套工具加入 PATH。已经配置的构建目录更换编译器时，用 `cmake --fresh --preset board-debug` 或 `board-release` 重新配置。依赖源码随工程保存，配置和构建可以离线执行。

如果 Windows 上的 CMake 在中文路径下退出，可在项目目录运行 `subst Z: (Get-Location).Path`，切换到 `Z:\` 后执行构建和测试。用空闲盘符并在后续构建中使用同一盘符。完成后退出该盘符，再执行 `subst Z: /D` 解除映射。

各固件构建目录位于 `build/board-debug` 和 `build/board-release`，内含 `iot-shark.elf`、`iot-shark.hex`、`iot-shark.bin`、`iot-shark.map` 和 `compile_commands.json`。构建时自动检查中断向量、复位入口、RT-Thread 初始化表和 MSH 命令符号。

## 主机测试

```powershell
cmake --preset host-test
cmake --build --preset host-test
ctest --preset host-test
```

主机测试覆盖 SDRAM 参数边界、越界防护和数据损坏，以及 CAN 发送超时、回环、时钟计数回绕与模式恢复。主机测试使用模拟后端，板上验证见下文。

## SWD 烧录

1. 组装核心板和底板并给板供电。将 ST-Link 或 J-Link 接到核心板 SWD 接口 U8，共地并接好 SWDIO、SWCLK 和目标电压参考。
2. 在 [STM32CubeProgrammer](https://www.st.com/content/st_com/en/stm32cubeprogrammer.html) 中连接调试器，选中本次构建的 `iot-shark.hex`，启用写入校验。烧录后复位。使用 BIN 时，下载起始地址为 `0x08000000`。
3. 使用 ST-Link 时，也可运行以下命令。CLI 路径和选项参见 [STM32CubeProgrammer 用户手册](https://www.st.com/resource/en/user_manual/dm00403500-stm32cubeprogrammer-software-description-stmicroelectronics.pdf)。

```powershell
STM32_Programmer_CLI -c port=SWD -d build/board-debug/iot-shark.hex -v -rst
```

## 板上验收

将底板 **USB2 Type-C** 接到电脑。打开 CH340K 对应串口，设为 **115200 baud、8N1、无流控**。复位后应看到 RT-Thread 启动信息、`IoT Shark V2 diagnostics`、`SDRAM init: ok` 和 `FDCAN1 init: ok`。按回车进入 MSH，核心板 PI1 上的 ULED2 约每秒闪一次。SDRAM 或 FDCAN 初始化失败时，控制台显示对应错误。

| MSH 命令 | 预期结果 |
| --- | --- |
| `help` | 列出命令，包含 `sdram`、`cantx`、`can_rx` 和 `can_selftest`。 |
| `sdram 65536` | 多次运行均打印 `SDRAM PASS 65536 bytes, 2 patterns`。`sdram 3` 和 `sdram 65540` 应返回用法提示。 |
| `can_selftest` | 无需连接外部适配器，打印 `can_selftest: ok`。即使回环失败，固件也会尝试恢复正常模式。 |
| `cantx` | 在 CAN 总线上发送标准帧 `0x078`，负载为 `00 11 22 33 44 55 66 77`。需要正常模式的接收节点应答，成功后打印 `cantx: ok`。 |
| `can_rx` | 在约 1 秒内等待并打印一帧。没有收到帧时打印 `can_rx: timeout`。 |

用设为 **125 kbit/s 经典 CAN** 的适配器连接底板 CN6，1 脚为 CANH，2 脚为 CANL。核对原理图中的 R61 终端电阻和总线另一端终端配置。运行 `cantx` 后在适配器端检查帧，再运行 `can_rx` 并从适配器发送测试帧。内部回环覆盖控制器及诊断路径，CN6 双向试验用于确认板上收发器和总线接线。

## 板级配置与后续迁移

| 功能 | 配置 |
| --- | --- |
| 时钟 | 25 MHz HSE，系统 480 MHz，HCLK 240 MHz，APB1 120 MHz。 |
| 串口和 LED | USART2 PD5 TX、PD6 RX。PI1 低电平点亮。 |
| SDRAM | FMC Bank 1、32 MiB、16 位。`0xC0000000` 起 2 MiB 保留给显示。命令只写测试区 `0xC0200000–0xC020FFFF`，该区由 MPU 配为非缓存。 |
| CAN | FDCAN1 PB8 RX、PB9 TX。正常模式下由 `cantx` 发起主动数据帧。 |

引脚和供电以原工程 `sch/iot-shark-core-v2.0.0.pdf` 和 `sch/iot-shark-db-v2.0.0.pdf` 为准。应用入口在 [app/main.c](app/main.c)，板级实现和链接脚本在 [board/](board/)，可在主机测试的诊断逻辑在 [diagnostics/](diagnostics/)。

首版板上验收后，按 QSPI 与 TF 卡、LCD 与触摸、RW007 Wi-Fi、ES8388 音频的顺序迁移。各阶段沿用同一构建入口和实板检查流程。第三方源码来源和许可见 [vendor/README.md](vendor/README.md)。
