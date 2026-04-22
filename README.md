<div align="center">

# iFly_Ctrl

<p><strong>基于 STM32F405 的飞控控制台、参数中心与底层通信框架</strong></p>

<p>
  以 STM32CubeMX 生成的 HAL 工程为底座，叠加 C++17 应用层、USB CDC / UART DMA / CAN 抽象、
  交互式 Shell、工程级参数中心、PID 控制器、时间与调度基础设施，以及 SBUS / CRSF 协议处理能力。
</p>

<p>
  <img src="https://img.shields.io/badge/MCU-STM32F405-0F766E?style=for-the-badge" alt="MCU">
  <img src="https://img.shields.io/badge/Language-C11%20%2B%20C%2B%2B17-1D4ED8?style=for-the-badge" alt="Language">
  <img src="https://img.shields.io/badge/Build-CMake%20%2B%20Ninja-CA8A04?style=for-the-badge" alt="Build">
  <img src="https://img.shields.io/badge/Interfaces-USB%20CDC%20%7C%20UART%20DMA%20%7C%20CAN-7C3AED?style=for-the-badge" alt="Interfaces">
</p>

</div>

## 项目概览

`iFly_Ctrl` 目前更接近一个“飞控工程底座”，而不是已经完成姿态解算、电机混控、遥控接收闭环的成品飞控。当前仓库重点完成的是这些基础设施：

- STM32F405 的 CubeMX / HAL 初始化底座
- `Core/` C 工程与 `iFly/` C++ 业务层分层
- USB CDC、UART DMA、CAN 的统一设备抽象
- 可登录、可切换链路、可调参、可执行函数调用的 Shell / CLI
- 基于 `ProjectParameters` / `ProjectParameterManager` 的工程级参数中心
- 支持在线验证与热调参的 PID 控制器
- `ms/us/ns` 时间接口、DWT 高精度计时、软定时器与任务封装
- SBUS / CRSF 协议编解码
- 观察者通道、无锁队列、双缓冲等基础工具

> [!IMPORTANT]
> 当前 `app_main()` 真正接入主循环的业务链路，是 `USB/UART CLI + Shell + ProjectParameterManager + PID + tick`。
> `SoftTimer`、`Task`、`ObserverChannel`、`CAN`、`SBUS`、`CRSF` 等模块已经有独立实现，但还没有在主业务中接成完整飞控闭环。

## 当前工程状态

| 模块 | 当前状态 | 说明 |
| --- | --- | --- |
| CubeMX / HAL 底座 | 已接入 | `Core/`、`Drivers/`、`.ioc` 负责硬件初始化 |
| CMake 构建链 | 已接入 | `Debug` / `Release` 预设可用 |
| Shell / FlightCtrlCli | 已接入 | 默认通过 USB CDC 提供 CLI |
| USB CDC | 已接入 | 默认 CLI 传输通道 |
| UART5 CLI | 已接入 | 备用 CLI 传输通道，可运行时切换 |
| ProjectParameterManager | 已接入 | 工程级参数树、按名读写、变更回调 |
| PID | 已接入 | 支持参数热更新和单次采样验证 |
| Tick / SysTick / DWT | 已接入 | 提供 `ms/us/ns` 时间接口 |
| SoftTimerService | 已实现 | 当前未直接接入 `app_main()` 主循环 |
| Task 模块 | 已实现 | 基于 SoftTimer 的函数式任务接口 |
| ObserverChannel | 已实现 | 单发布者、多消费者无锁快照通道 |
| UART DMA Service | 已接入 | 多串口统一 DMA 收发服务 |
| USB CDC ACM | 已接入 | 自实现轻量协议层 |
| CAN Service | 已实现 | 当前更偏向底层能力和 loopback 验证 |
| SBUS / CRSF | 已实现 | 提供协议编解码和统计信息 |
| PWM / LED | 已实现 | 已有底层封装，尚未进入主业务链路 |

## 仓库结构

```text
iFly_Ctrl/
├─ Core/                           # STM32CubeMX 生成的 C 层启动与外设初始化
│  ├─ Inc/
│  └─ Src/
├─ Drivers/                        # HAL / CMSIS 驱动
├─ cmake/                          # 工具链与 CubeMX-CMake 相关脚本
├─ iFly/
│  ├─ app_main.cpp                 # C++ 应用层主入口
│  ├─ app/
│  │  ├─ cli/                      # FlightCtrlCli 与终端交互逻辑
│  │  ├─ ctrl/                     # PID 控制器
│  │  ├─ observer/                 # 观察者通道与消息中心
│  │  ├─ paramete/                 # 工程参数中心（目录名按仓库现状保留）
│  │  ├─ proto/
│  │  │  ├─ crsf/                  # CRSF 协议编解码
│  │  │  └─ sbus/                  # SBUS 协议编解码
│  │  ├─ task_mage/                # 基于 SoftTimer 的任务接口
│  │  └─ tick/                     # 时间接口薄封装
│  ├─ dev/
│  │  ├─ can_dev/                  # CAN 设备封装
│  │  ├─ uart_dev/                 # UART 设备封装
│  │  ├─ usb_dev/                  # USB CDC 设备封装
│  │  └─ serial_io_base.hpp        # 统一字节流设备抽象
│  └─ lib/
│     ├─ can/                      # CAN 运行时服务
│     ├─ double_buffer/            # 双缓冲工具
│     ├─ freelock_queue/           # 无锁队列
│     ├─ led/                      # LED 封装
│     ├─ platform/                 # HAL 句柄转换工具
│     ├─ pwm/                      # PWM 封装
│     ├─ shell/                    # 通用 Shell
│     ├─ soft_timer/               # 软定时器服务
│     ├─ systick_timer/            # DWT + SysTick 时间基准
│     ├─ uart/                     # UART DMA 服务
│     ├─ usb_cdc/                  # USB CDC ACM 协议层
│     └─ usermath.hpp              # 通用数学工具
├─ CMakeLists.txt
├─ CMakePresets.json
├─ CONTRIBUTING.md
├─ iFly_Ctrl.ioc
├─ STM32F405XX_FLASH.ld
└─ startup_stm32f405xx.s
```

## 运行时架构

```mermaid
flowchart TD
    A[上电复位] --> B[Core/Src/main.c]
    B --> C[HAL_Init + 时钟配置 + 外设初始化]
    C --> D[app_main()]

    D --> E[FlightCtrlCli]
    E --> F[Shell]
    F --> G[ProjectParameterManager]
    G --> H[Pid]

    E --> I[UsbUart]
    E --> J[HardwareUart UART5]
    I --> K[UsbCdcAcm]
    J --> L[UartDmaService]

    D --> M[tick]
    M --> N[systick_time]
    N --> O[SoftTimerService]
    O -. 可选上层封装 .-> P[TaskCreate / TaskDispatch]

    D -. 预留接入 .-> Q[HardwareCan / CanService]
    D -. 预留接入 .-> R[SbusProtocol / CrsfProtocol]
    D -. 预留接入 .-> S[ObserverChannel / ObserverHub]
```

当前 `app_main()` 的主循环很简单：

1. 初始化 `UsbUart` 和 `HardwareUart(UART5)`
2. 初始化 `FlightCtrlCli`
3. 注册 `usb` 与 `uart5` 传输通道，并默认选择 `usb`
4. 使用 `tick::NonBlockingDelayMs` 每 `50 ms` 轮询一次 CLI

这说明当前主业务定位非常明确：先把“稳定可交互的控制台 + 参数中心 + 基础控制组件”跑通。

## 关键模块

### 1. ProjectParameterManager

参数系统已经从旧的 Shell 风格参数表，演进成工程级参数中心：

- 根参数树定义在 `iFly/app/paramete/project_parameters.*`
- 运行时管理器在 `iFly/app/paramete/project_parameter_manager.*`
- 支持按结构体直接访问，也支持按字符串名读写
- 支持参数变更回调
- `FlightCtrlCli` 通过它驱动 PID 参数、密码等运行时配置

示例：

```cpp
auto &pm = iFly::ProjectParameterManager::Instance();
bool arm_locked = pm.Data().system.arm_locked;
(void)pm.Write("system.arm_locked", false);
```

更详细的参数系统文档见：

- [iFly/app/paramete/README.md](iFly/app/paramete/README.md)

### 2. Shell 与 FlightCtrlCli

`iFly/lib/shell` 提供通用终端框架，负责：

- 登录状态机
- 密码验证
- 输入行编辑
- 命令 / 函数 / 参数注册
- 文本输出与提示符控制

`iFly/app/cli/flight_ctrl_cli.*` 则是飞控场景封装，负责：

- 注册可管理参数
- 注册业务函数
- 绑定 USB / UART 传输通道
- 同步 Shell 横幅、开机动画、状态输出

当前 CLI 已注册的受管参数包括：

- `pid.kp / pid.ki / pid.kd / pid.kff`
- `pid.i_min / pid.i_max`
- `pid.out_min / pid.out_max`
- `pid.d_cutoff_hz`
- `sys.loop_hz`
- `sys.arm_locked`
- `sys.transport`
- `sys.uptime_ms`

已注册函数包括：

- `status`
- `sys.reboot`
- `pid.reset`
- `pid.sample <sp> <meas> <dt_ms>`
- `transport.list`
- `transport.use <name>`

### 3. Tick / SoftTimer / Task

时间与调度相关模块现在分成三层：

- `iFly/lib/systick_timer/`
  - 基于 `HAL_GetTick()` + `DWT->CYCCNT`
  - 提供 `ms/us/ns` 时间戳和阻塞延时
- `iFly/app/tick/`
  - 对上层暴露统一的时间接口与非阻塞延时器
- `iFly/lib/soft_timer/`
  - 非抢占式软定时器服务
  - 支持优先级、周期任务、单次任务、运行中删除、当前任务延时

`iFly/app/task_mage/` 现在已经改成函数式接口，不再对外暴露类，主要接口有：

- `TaskCreate`
- `TaskCreatePeriodic`
- `TaskCreateOneShot`
- `TaskDelete`
- `TaskDelay`
- `TaskSuspend`
- `TaskResume`
- `TaskDispatch`
- `TaskNow`

这个模块本质上是对 `SoftTimerService` 的上层任务语义封装。

### 4. ObserverChannel

`iFly/app/observer/observer_channel.hpp` 已经提供：

- `ObserverChannel<T>`
- `CallbackConsumer`
- `ObserverHub`

它适合做“单发布者、多消费者”的无锁快照分发，目前实现已完成，但尚未进入主业务链路。

### 5. 统一设备抽象：SerialIoBase

`iFly/dev/serial_io_base.hpp` 是上层通信抽象的核心，它统一了：

- `Init()`
- `Write()`
- `Read()`
- `TxFree() / TxUsed()`
- `RxDropped()`
- `IsConnected()`

这样上层业务无需知道底层是 USB、UART 还是 CAN，只需要绑定某个 `SerialIoBase` 派生对象。

### 6. UART DMA / USB CDC / CAN

#### UART DMA

`iFly/lib/uart/uart_dma.*` 提供单例服务管理多路逻辑串口，每一路端口都有自己独立的运行时槽位：

- DMA RX 环形缓冲区
- 上层 RX 队列
- TX 无锁队列
- TX 双缓冲

上层通过 `HardwareUart` 使用它，当前主循环接入的是 `UART5`。

#### USB CDC ACM

`iFly/lib/usb_cdc/usb_cdc.*` 是自实现的轻量级 `USB CDC ACM` 协议层，负责：

- 描述符
- EP0 控制请求处理
- 数据端点收发
- 上层 RX 队列挂接
- TX 队列与双缓冲驱动

当前 CLI 默认走 USB CDC。

#### CAN

`iFly/lib/can/` 和 `iFly/dev/can_dev/` 已经提供完整底层服务与设备封装。

当前特点：

- CubeMX 当前配置的是 `CAN1`
- 现阶段更偏向底层能力和 loopback 验证
- 尚未在 `app_main()` 主流程中接入

### 7. 控制与协议模块

#### PID

PID 控制器当前支持：

- `P / I / D / FeedForward`
- 积分限幅
- 输出限幅
- 导数低通滤波
- 抗积分饱和
- 非法输入保护

CLI 的 `pid.sample` 命令可以直接验证单次 PID 输出。

#### SBUS / CRSF

这两个协议模块都已经提供独立的编解码器和统计信息：

- `iFly/app/proto/sbus/`
- `iFly/app/proto/crsf/`

它们适合作为后续遥控接收链路接入的基础。

## app 模块使用示例

下面的示例统一使用工程内当前约定的头文件引用方式：只 `#include` 对应 `.hpp` 文件名，不再写前缀路径。

### 1. cli

`FlightCtrlCli` 的典型用法就是在启动阶段绑定传输通道，在主循环中周期性调用 `Poll()`。当前 `app_main()` 也是按这个方式接入的。

```cpp
#include "flight_ctrl_cli.hpp"
#include "hardware_uart.hpp"
#include "tick.hpp"
#include "usb_uart.hpp"

namespace {

constexpr uint32_t kCliRxQueueSize = 1024U;

iFly::HardwareUart g_uart5(iFly::UartPortId::kUart5, kCliRxQueueSize);
iFly::UsbUart g_usb_cli(kCliRxQueueSize);
iFly::FlightCtrlCli g_cli;

} // namespace

void InitCliRuntime()
{
  g_uart5.Init();
  g_usb_cli.Init();

  g_cli.Init();
  (void)g_cli.RegisterTransport("uart5", &g_uart5);
  (void)g_cli.RegisterTransport("usb", &g_usb_cli);
  (void)g_cli.UseTransport("usb");
}

void PollCliLoop()
{
  iFly::tick::NonBlockingDelayMs poll_delay {};
  poll_delay.Start(50U);

  while (1) {
    if (poll_delay.ConsumeIfExpired()) {
      g_cli.Poll();
      poll_delay.Start(50U);
    }

    iFly::tick::DelayMs(1U);
  }
}
```

### 2. ctrl

`ctrl` 目录当前对外核心接口是 `Pid`。典型流程是先配置参数，再在控制周期内反复调用 `Update()`。

```cpp
#include "pid.hpp"

void RunPidSample()
{
  iFly::Pid pid;

  iFly::Pid::Config config {};
  config.kp = 0.80f;
  config.ki = 0.15f;
  config.kd = 0.02f;
  config.output_min = -500.0f;
  config.output_max = 500.0f;
  config.derivative_cutoff_hz = 80.0f;
  pid.Configure(config);

  iFly::Pid::UpdateInput input {};
  input.setpoint = 100.0f;
  input.measurement = 92.0f;
  input.dt_s = 0.001f;

  const iFly::Pid::UpdateResult result = pid.Update(input);
  const float output = result.output;
  (void)output;
}
```

### 3. observer

`ObserverChannel` 适合单发布者、多消费者场景。发布端只负责 `Publish()`，消费端通过订阅句柄读取最新值或按序读取。

```cpp
#include "observer_channel.hpp"

struct AttitudeSample final {
  float roll = 0.0f;
  float pitch = 0.0f;
};

void UseObserverChannel()
{
  iFly::ObserverChannel<AttitudeSample, 8U, 2U> channel;
  auto consumer = channel.SubscribeFromNext();

  channel.Publish({1.5f, -0.3f});

  AttitudeSample latest {};
  const iFly::ConsumeStatus status = consumer.TryReadLatest(latest);
  if (status != iFly::ConsumeStatus::kNoData) {
    const float roll = latest.roll;
    (void)roll;
  }
}
```

### 4. paramete

`paramete` 模块当前通过 `ProjectParameterManager` 统一管理整棵工程参数树。建议读取用 `Data()` 或 `Read()`，写入优先走 `Write()` / `WriteRaw()`，这样才能统一触发变更回调。

```cpp
#include "project_parameter_manager.hpp"

static void OnCliPasswordChanged(const char *name, void *context)
{
  (void)name;
  (void)context;
}

void UseProjectParameters()
{
  auto &pm = iFly::ProjectParameterManager::Instance();

  const bool arm_locked = pm.Data().system.arm_locked;

  uint32_t loop_hz = 0U;
  (void)pm.Read("system.control_loop_hz", &loop_hz);
  (void)pm.Write("system.arm_locked", false);

  char password[8] = "admin";
  (void)pm.WriteRaw("cli.password", password, sizeof(password));
  (void)pm.SetChangeHandler("cli.password", &OnCliPasswordChanged, nullptr);

  (void)arm_locked;
  (void)loop_hz;
}
```

### 5. proto/crsf

`CrsfProtocol` 既可以做字节流解析，也可以做结构化帧编解码。下面示例演示一帧 RC 通道数据的编码和解码。

```cpp
#include "crsf_protocol.hpp"

void UseCrsfProtocol()
{
  iFly::CrsfRcChannelsPacked tx_channels {};
  tx_channels.channels[0] = 992U;
  tx_channels.channels[1] = 992U;
  tx_channels.channels[2] = 172U;
  tx_channels.channels[3] = 1811U;

  uint8_t raw_frame[iFly::CrsfProtocol::kMaxPacketSize] {};
  uint32_t written_length = 0U;

  (void)iFly::CrsfProtocol::EncodeRcChannelsPacked(
      0xC8U, tx_channels, raw_frame, sizeof(raw_frame), &written_length);

  iFly::CrsfFrame frame {};
  if (iFly::CrsfProtocol::TryDecodeFrame(raw_frame, written_length, &frame)) {
    iFly::CrsfRcChannelsPacked rx_channels {};
    if (iFly::CrsfProtocol::DecodeRcChannelsPacked(frame, &rx_channels)) {
      const uint16_t throttle = rx_channels.channels[2];
      (void)throttle;
    }
  }
}
```

如果你的输入来自 UART 连续字节流，则直接调用 `Parse()`，把本次收到的字节块解出为一个或多个 `CrsfFrame`。

### 6. proto/sbus

`SbusProtocol` 用法和 `CrsfProtocol` 类似，既支持单帧编解码，也支持连续字节流解析。

```cpp
#include "sbus_protocol.hpp"

void UseSbusProtocol()
{
  iFly::SbusProtocol protocol;

  iFly::SbusFrame tx_frame {};
  tx_frame.channels[0] = 1024U;
  tx_frame.channels[1] = 1024U;
  tx_frame.channels[2] = 172U;
  tx_frame.channel17 = true;

  uint8_t raw_frame[iFly::SbusProtocol::kFrameSize] {};
  (void)protocol.Encode(tx_frame, raw_frame);

  iFly::SbusFrame rx_frame {};
  if (iFly::SbusProtocol::TryDecodeFrame(raw_frame, sizeof(raw_frame),
                                         &rx_frame)) {
    const uint16_t throttle = rx_frame.channels[2];
    (void)throttle;
  }
}
```

如果你的 UART 接收是分段到达的，推荐长期持有一个 `SbusProtocol` 实例，并对每次收到的数据块调用 `Parse()`。

### 7. task_mage

`task_mage` 现在是函数式接口，不需要再实例化任务管理类。创建任务后，需要在主循环里持续调用 `TaskDispatch()`。

```cpp
#include "task.hpp"
#include "tick.hpp"

namespace {

void HeartbeatTask(void *context)
{
  uint32_t *run_count = static_cast<uint32_t *>(context);
  ++(*run_count);
}

} // namespace

void UseTaskModule()
{
  uint32_t run_count = 0U;

  const iFly::TaskHandle heartbeat =
      iFly::TaskCreatePeriodic(&HeartbeatTask, &run_count, 10U);

  while (iFly::TaskIsAlive(heartbeat)) {
    (void)iFly::TaskDispatch();
    iFly::tick::DelayMs(1U);

    if (run_count >= 100U) {
      (void)iFly::TaskDelete(heartbeat);
    }
  }
}
```

### 8. tick

`tick` 模块提供统一的 `ms/us/ns` 时间接口，以及阻塞延时和非阻塞延时器。

```cpp
#include "tick.hpp"

void UseTickModule()
{
  const uint32_t start_ms = iFly::tick::NowMs();
  iFly::tick::DelayMs(10U);

  if (iFly::tick::ElapsedMs(start_ms) >= 10U) {
    iFly::tick::DelayUs(20U);
  }

  iFly::tick::NonBlockingDelayMs period {};
  period.Start(50U);

  while (!period.ConsumeIfExpired()) {
    // 在这里执行其他轮询任务。
  }
}
```

## CLI 快速上手

### 1. 登录

当前默认流程：

1. 打开 USB CDC 串口
2. 等待提示
3. 按空格进入终端
4. 输入默认密码 `ifly`

### 2. 常用命令

```text
help
params
get pid.kp
set pid.kp 1.20
get sys.loop_hz
set sys.arm_locked false
call status
call pid.sample 100 92 1
call transport.list
call transport.use uart5
call sys.reboot
```

### 3. 当前默认链路

- 默认 CLI 通道：`usb`
- 备用 CLI 通道：`uart5`

切到 `uart5` 后，需要在对应串口重新连接。

## 构建方式

### 依赖

建议准备以下环境：

- `CMake 3.22+`
- `Ninja`
- `GNU Arm Embedded Toolchain`
- `arm-none-eabi-gcc / g++ / objcopy / size` 已加入 `PATH`

### 预设

仓库通过 `CMakePresets.json` 提供：

- `Debug`
- `Release`

### 终端构建

```bash
cmake --preset Debug
cmake --build --preset Debug
```

构建输出目录：

```text
build/Debug/
```

主要产物：

```text
iFly_Ctrl.elf
```

### VS Code 说明

如果你使用 STM32 / CMake 相关 VS Code 扩展，IDE 里可能会调用 `cube-cmake` 之类的包装命令；本质上仍然是基于当前仓库的 `CMakePresets.json` 和 `cmake/` 工具链脚本构建。

### CubeMX 说明

如果要修改时钟、GPIO、DMA、UART、CAN、USB 等硬件配置，建议继续以 `iFly_Ctrl.ioc` 为入口，用 STM32CubeMX 维护底层初始化代码；业务逻辑尽量继续放在 `iFly/` 目录，避免被生成代码覆盖。

## 开发建议

- 底层初始化继续交给 CubeMX / HAL，业务逻辑放在 `iFly/`
- 参数更新如果需要通知联动，优先走 `ProjectParameterManager::Write()`，不要直接依赖 `MutableData()`
- `task_mage` 现在是函数式模块，不再需要实例化任务管理类
- 当前工程内部头文件引入已经统一成不带目录前缀的方式，例如 `#include "tick.hpp"`

## 文档入口

- 参数系统详细说明：
  [iFly/app/paramete/README.md](iFly/app/paramete/README.md)
- 文档贡献说明：
  [CONTRIBUTING.md](CONTRIBUTING.md)

## 后续扩展建议

如果要把这个仓库继续推进成完整飞控，推荐优先做这些事：

1. 把 `SoftTimerService` / `Task` 真正接入主循环调度
2. 把 `SBUS` 或 `CRSF` 接入实际 UART 数据源
3. 增加 IMU、姿态状态、遥控状态、电机输出等运行时对象
4. 把 `PWM` / `LED` 接入状态指示或输出控制
5. 从 CLI 扩展到任务状态、遥控输入、姿态状态调试
6. 把 `CAN` 从当前验证状态切到真实总线业务

## 总结

`iFly_Ctrl` 当前最重要的价值，不是“功能已经全部做完”，而是它已经把飞控工程最容易失控的基础部分先搭好了：

- 工程分层
- 通信抽象
- 可交互 CLI
- 工程级参数中心
- 控制器基础件
- 时间与调度底座
- 协议处理基础模块

如果你想从零演进一个可维护、可继续扩展的 STM32 飞控工程，这个仓库已经是一个比较扎实的起点。
