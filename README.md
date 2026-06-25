# Supervisor — 环境监控主机

基于 STM32F103RCT6 的嵌入式环境监控系统，支持温湿度采集、LCD 曲线显示、SPI Flash 数据存储、串口控制台及越限报警。

## 硬件平台

| 项目 | 规格 |
|------|------|
| MCU | STM32F103RCT6 (Cortex-M3, 72MHz, 256KB Flash / 48KB SRAM) |
| 传感器 | DHT11 数字温湿度传感器 |
| 显示屏 | ILI9341 TFT LCD (SPI) |
| 存储 | W25Q64 SPI Flash (8MB) |
| 调试串口 | USART1 (PA9/PA10, 115200-8-N-1) |

## 引脚分配

| 外设 | 引脚 | 说明 |
|------|------|------|
| K1 (暂停/恢复) | PA0 | 按下为低电平，上拉输入 |
| K2 (清除数据) | PC13 | 按下为低电平，上拉输入 |
| LED1 (运行指示) | PC2 | TIM2 控制，500ms 闪烁 |
| LED2 (报警指示) | PC3 | 低电平点亮 |
| BEEP (蜂鸣器) | PC1 | 高电平鸣响 |
| DHT11 | PC0 | 单总线数字传感器 |
| 风机 PWM | PA2 (TIM5_CH3), PA3 (TIM5_CH4) | 1kHz PWM 调速 |
| USART1 TX/RX | PA9 / PA10 | 串口控制台 |

## 功能特性

- **温湿度采集**：DHT11 每秒采样一次，LCD 实时曲线显示
- **数据记录**：每 30 秒自动存储一条记录至 W25Q64，可存储约 24,576 条（约 8.5 天，循环写入）
- **串口控制台**：115200 波特率，支持交互式命令
- **越限报警**：温湿度超出阈值时蜂鸣器鸣响 + LED2 亮 + PWM 风机启动
- **按键控制**：K1 暂停/恢复采集，K2 清除全部存储数据

### 串口命令

| 命令 | 格式 | 说明 |
|------|------|------|
| `HELP` | `HELP` | 查看可用命令列表 |
| `TEMP` | `TEMP <min> <max>` | 设置温度报警阈值 (整数 °C)，例: `TEMP 10 35` |
| `HUMI` | `HUMI <min> <max>` | 设置湿度报警阈值 (整数 %)，例: `HUMI 30 70` |
| `THRESH` | `THRESH` | 查询当前温湿度报警阈值 |
| `FAN` | `FAN <duty>` | 手动控制风机 (0=关, 1~999=占空比)，例: `FAN 500` |
| `DUMP` | `DUMP [start] [n]` | 导出已存储的温湿度记录，例: `DUMP 0 100` |
| `PAUSE` | `PAUSE` | 暂停/恢复数据采集 (同 K1) |
| `CLR` | `CLR` | 清除全部存储数据 (需先暂停，同 K2) |

## 项目结构

```
Supervisor/
├── USER/                   # 用户应用层
│   ├── main.c              # 主程序
│   ├── stm32f10x_conf.h    # 标准外设库配置
│   ├── stm32f10x_it.c/h    # 中断服务函数
│   ├── system_stm32f10x.c/h # 系统时钟初始化
│   └── font/ lcd/          # 字体 & LCD 驱动
├── HARDWARE/               # 硬件驱动层
│   ├── DHT11.c/h           # DHT11 温湿度传感器
│   ├── thermohygrometer.c/h # 温湿度计抽象层
│   ├── data_logger.c/h     # SPI Flash 数据记录器
│   ├── serial_cmd.c/h      # 串口命令解析
│   ├── alarm.c/h           # 声光报警 + PWM 风机
│   ├── BEEP.c/h            # 蜂鸣器驱动
│   ├── LED.c/h             # LED 驱动
│   ├── GPIO.c/h            # GPIO 快速初始化
│   ├── bsp_spi_flash.c/h   # W25Q64 SPI Flash 驱动
│   └── usart_console.c/h   # 串口控制台
├── SYSTEM/
│   ├── delay/              # 延时函数
│   └── sys/                # 系统基础函数
├── CORE/                   # CMSIS Core + 启动文件
├── STM32F103x_FWLIB/       # STM32 标准外设库
└── STM32F103RCTX_FLASH.ld  # 链接脚本
```

## 构建工具链

- **编译器**：`arm-none-eabi-gcc` 16.1.0
- **构建系统**：[EIDE](https://github.com/github0null/eide) (Embedded IDE, VS Code 插件)
- **调试器**：CMSIS-DAP

### VS Code 任务

| 任务 | 说明 |
|------|------|
| `build` | 编译项目 |
| `rebuild` | 重新编译 |
| `flash` | 烧录到设备 |
| `build and flash` | 编译并烧录 |
| `clean` | 清理构建产物 |

## License

本项目基于 MIT License 开源，详见 [LICENSE](LICENSE)。
