# iFly 架构改进建议

本文档基于当前 `D:\personal\iFly_Ctrl_mavlink_qgc\iFly` 目录结构整理，目标是帮助你学习代码架构，而不是要求一次性重构。

建议阅读方式：

1. 先看“不需要改的部分”，明确哪些设计已经是对的。
2. 再看“建议改的部分”，按优先级逐步做。
3. 每次只改一个小目标，改完用“验证方式”确认结果。

## 当前整体判断

当前项目已经有比较清楚的分层雏形：

```text
iFly/
  app/          飞控业务逻辑、协议、参数、CLI、控制算法
  dev/          设备适配层，例如 USB/UART/CAN 对上统一成接口
  lib/          可复用底层库，例如 soft_timer、shell、queue、driver service
  task/         任务创建、模块初始化、运行调度
  middlewares/  MAVLink、DroneCAN 等第三方或生成代码
```

这个方向是对的。后续主要要改的是“边界清晰度”，不是推倒重来。

## 不需要改的部分

### 1. 顶层分层方向不需要改

`app/dev/lib/task/middlewares` 的分层是合理的。

建议保留这个方向：

```text
task -> app -> dev -> lib -> HAL / middlewares
```

含义：

- `task` 负责把对象创建出来、连起来、周期调用。
- `app` 负责飞控业务逻辑。
- `dev` 负责把具体硬件包装成上层可用接口。
- `lib` 负责通用能力，不知道飞控业务。
- `middlewares` 放第三方协议和生成代码，不混入业务逻辑。

不要让依赖方向反过来。例如 `lib` 不应该 include `project_parameter_manager.hpp`，`dev` 不应该知道 `MavlinkReceiver`。

### 2. `SerialIoBase` 这个抽象不需要改

`iFly/dev/serial_io_base.hpp` 把 USB、UART 等链路抽象成统一字节流，这是很好的设计。

当前 `MavlinkLink` 依赖 `SerialIoBase`，而不是直接依赖 `UsbUart`，这是对的。

后面如果增加 UART MAVLink、蓝牙串口、仿真串口，只需要新增一个继承 `SerialIoBase` 的设备适配类，MAVLink 上层逻辑可以不动。

### 3. `MavlinkLink / MavlinkSend / MavlinkReceiver / MavlinkConsole` 的拆分方向不需要改

当前拆分思路是好的：

- `MavlinkLink`：负责 MAVLink 字节流收发。
- `MavlinkSend`：负责把状态结构打包成 MAVLink 消息发送。
- `MavlinkReceiver`：负责接收消息并分发。
- `MavlinkConsole`：负责 MAVLink SERIAL_CONTROL 和 CLI 之间的转接。

这个方向不要推倒。需要改的是“把它们接完整”和“后续避免单个文件过大”。

### 4. `SoftTimerService` 和 `Task` 的基本思路不需要改

现在 `lib/soft_timer` 提供底层软定时器，`app/task_mage/task.*` 再包装成上层任务接口。这个分层可以保留。

对学习架构来说，这里已经体现了一个很重要的思想：

```text
底层服务提供能力，上层模块提供更贴近业务的接口。
```

### 5. `ObserverChannel` 不需要删除

`iFly/app/observer/observer_channel.hpp` 是一个值得保留的模块。它适合后续做模块间状态传递，例如：

- RC 输入发布给控制任务。
- 姿态解算发布给控制任务和 MAVLink。
- 电池状态发布给 MAVLink 和 CLI。
- 控制输出发布给 PWM 输出和日志模块。

它现在可能还没成为主干通信方式，但不需要删。后续可以逐步引入。

### 6. `middlewares` 不需要纳入业务架构重构

`iFly/middlewares` 里很多文件是第三方或生成代码，例如 MAVLink、DroneCAN。它们不应该和你自己写的业务代码一起改。

建议规则：

- 不在 `middlewares` 里写业务逻辑。
- 不手改生成文件。
- 用你自己的 wrapper 隔离它们，例如 `MavlinkLink`、`MavlinkSend`。

## 需要改的部分

下面按优先级排列。优先级越靠前，越适合现在做。

## P0：先把已有 MAVLink 架构接完整

### 当前问题

`iFly/task/task_mavlink.cpp` 里创建了 MAVLink 周期任务，但 `MavlinkTask()` 当前只判断了 `link == nullptr`，没有真正处理消息。

也就是说，项目已经有 `MavlinkReceiver`、`MavlinkConsole`、`MavlinkSend`，但 task 主循环没有真正驱动这些模块。

### 为什么要改

这是当前最影响可验证性的点。

如果 MAVLink task 不真正 poll，那么：

- QGC 请求参数时，参数协议不会稳定工作。
- SERIAL_CONTROL 到 CLI 的链路不会完整。
- 后续 heartbeat、status、RC override、manual control 都没有统一入口。

### 建议怎么改

目标：`task_mavlink.cpp` 只负责周期调用，不写具体业务。

建议做法：

1. 在装配层创建这些对象：

```cpp
iFly::FlightCtrlCli Mavlink_CLI;
iFly::UsbUart Usb_Cdc(kUsbRxQueueSize);
iFly::MavlinkLink Mavlink(&Usb_Cdc);
iFly::MavlinkReceiver Mavlink_Receiver(&Mavlink);
iFly::MavlinkConsole Mavlink_Console(&Mavlink, &Mavlink_CLI);
```

2. 给 MAVLink task 传入一个上下文结构，而不是只传 `MavlinkLink *`：

```cpp
struct MavlinkTaskContext final {
  iFly::MavlinkReceiver *receiver = nullptr;
};
```

3. `MavlinkTask()` 里只做一件事：

```cpp
while (context->receiver->Poll()) {
}
```

4. 让 `MavlinkReceiver` 能处理 console。

当前 `MavlinkReceiver::HandleConsoleMessage()` 是空的。建议给 `MavlinkReceiver` 增加一个 `MavlinkConsole *console_`，并提供 `BindConsole()`。

然后：

```cpp
void HandleConsoleMessage(const mavlink_message_t &msg)
{
  if (console_ != nullptr) {
    (void)console_->ProcessConsoleMessage(msg);
  }
}
```

这样 SERIAL_CONTROL 消息仍然由 receiver 分发，console 逻辑仍然在 `MavlinkConsole` 内，不会串层。

### 验证方式

1. 编译通过：

```powershell
cmake --build build\Debug
```

2. 连接 QGC 后，请求参数列表，能收到 `PARAM_VALUE`。

3. 使用 MAVLink Console 或 SERIAL_CONTROL，CLI 能收到输入并返回输出。

4. `task_mavlink.cpp` 里不出现参数读写、CLI 命令解析、具体 MAVLink command 业务逻辑。

## P0：修正目录命名和参数模块归属

### 当前问题

当前有两个目录名不太清楚：

```text
iFly/app/paramete
iFly/app/task_mage
```

它们看起来像拼写问题：

- `paramete` 可能想表达 `parameter`。
- `task_mage` 可能想表达 `task_manage` 或 `task_manager`。

另外，参数定义现在在：

```text
iFly/app/fly_sys/project_parameters.*
```

参数管理器在：

```text
iFly/app/paramete/project_parameter_manager.*
```

这会让读代码的人分不清“参数模块到底属于哪里”。

### 为什么要改

目录名是架构的一部分。目录名不清楚时，后续新文件容易被放错位置。

参数是全局基础模块，后面会被 MAVLink、CLI、控制器、存储模块使用，所以它的归属需要清楚。

### 建议怎么改

推荐方案一：参数属于飞控系统层。

```text
iFly/app/fly_sys/parameters/project_parameters.hpp
iFly/app/fly_sys/parameters/project_parameters.cpp
iFly/app/fly_sys/parameters/project_parameter_manager.hpp
iFly/app/fly_sys/parameters/project_parameter_manager.cpp
```

推荐方案二：参数作为独立 app 基础模块。

```text
iFly/app/parameter/project_parameters.hpp
iFly/app/parameter/project_parameters.cpp
iFly/app/parameter/project_parameter_manager.hpp
iFly/app/parameter/project_parameter_manager.cpp
```

我更推荐方案一。因为你现在的参数基本是飞控系统参数，和 MAVLink/QGC 关系很近，放在 `fly_sys/parameters` 比较自然。

### 具体改法

1. 新建目录：

```text
iFly/app/fly_sys/parameters
```

2. 移动文件：

```text
iFly/app/fly_sys/project_parameters.* -> iFly/app/fly_sys/parameters/
iFly/app/paramete/project_parameter_manager.* -> iFly/app/fly_sys/parameters/
```

3. 修改 `CMakeLists.txt` 里的源文件路径。

4. 修改 include path 或 include 引用。

5. 如果没有其他文件使用 `app/paramete`，删除空目录。

### 验证方式

```powershell
rg -n "paramete|task_mage" iFly CMakeLists.txt
cmake --build build\Debug
```

理想结果：

- `paramete` 不再出现。
- 如果你也改 `task_mage`，`task_mage` 不再出现。
- 编译通过。

## P1：控制 `MavlinkReceiver` 文件继续变大

### 当前问题

`iFly/app/fly_sys/ground_station/mavlink_receiver.hpp` 现在已经包含很多消息分发：

- heartbeat
- ping
- command long
- manual control
- parameter
- mission
- log
- serial control
- HIL

现在还能接受，但继续加功能后会快速变成大文件。

### 为什么要改

一个文件同时处理所有 MAVLink 消息，后续会出现几个问题：

- 参数协议改动会影响 command 代码。
- mission 代码变长后影响 receiver 可读性。
- 文件 include 变多，编译依赖变重。
- 很难判断某个消息属于哪个业务模块。

### 建议怎么改

先不要一次性拆全部。建议按功能成熟度拆。

第一步只拆参数：

```text
iFly/app/fly_sys/ground_station/mavlink_parameter_handler.hpp
iFly/app/fly_sys/ground_station/mavlink_parameter_handler.cpp
```

职责：

- 处理 `PARAM_REQUEST_READ`
- 处理 `PARAM_REQUEST_LIST`
- 处理 `PARAM_SET`
- 调用 `ProjectParameterManager`
- 调用 `MavlinkSend::SendParameterValue`

`MavlinkReceiver` 只保留：

```cpp
case MAVLINK_MSG_ID_PARAM_REQUEST_READ:
case MAVLINK_MSG_ID_PARAM_REQUEST_LIST:
case MAVLINK_MSG_ID_PARAM_SET:
  parameter_handler_.HandleMessage(msg);
  break;
```

第二步再拆 command：

```text
mavlink_command_handler.hpp/.cpp
```

第三步再拆 mission/log 等更复杂的模块。

### 验证方式

1. 拆分前后 QGC 参数列表行为一致。
2. `MavlinkReceiver` 文件长度明显下降。
3. `MavlinkReceiver` 不直接 include `project_parameter_manager.hpp`，而是 handler include。
4. 编译通过。

## P1：把系统状态从“大结构”逐步拆成领域状态

### 当前问题

`iFly/app/fly_sys/sys_state_type.hpp` 里包含很多状态：

- heartbeat
- system status
- battery
- gps
- global position
- local position
- attitude
- rc channels
- output status
- command
- manual control
- version

早期放一起方便，但后续会变成“所有模块都 include 一个大状态头文件”。

### 为什么要改

如果控制模块只需要 `Attitude`，它不应该被迫看到 GPS、电池、MAVLink command。

架构上，这叫降低 include 范围和认知范围。

### 建议怎么改

不要马上拆。建议等状态类型继续增加时再拆。

未来可以拆成：

```text
iFly/app/fly_sys/state/vehicle_state.hpp
iFly/app/fly_sys/state/sensor_state.hpp
iFly/app/fly_sys/state/control_state.hpp
iFly/app/fly_sys/state/communication_state.hpp
```

建议分法：

- `vehicle_state.hpp`：Heartbeat、VehicleType、VehicleState、ModeFlag
- `sensor_state.hpp`：BatteryStatus、GpsStatus、Attitude、GlobalPosition、LocalPosition
- `control_state.hpp`：ManualControl、RcChannels、OutputStatus
- `communication_state.hpp`：CommandRequest、CommandResponse、StatusText、VersionInfo

### 验证方式

1. 每个 `.hpp` 只包含对应领域的类型。
2. `mavlink_send.hpp` 可以 include 多个状态头，但 PID、RC、mixer 这类模块只 include 自己需要的状态头。
3. 编译通过。

## P1：CLI 需要拆职责

### 当前问题

`iFly/app/cli/flight_ctrl_cli.*` 现在同时负责：

- CLI 初始化
- 参数注册
- 参数 get/set
- status/reboot 功能
- banner
- 启动动画
- 输出通道绑定

这个文件后面很容易继续长大。

### 为什么要改

CLI 是典型的“容易吸收杂活”的模块。所有调试功能都容易往里面塞。

如果不控制，后面它会变成一个包含所有系统知识的中心文件。

### 建议怎么改

保留 `FlightCtrlCli` 作为总入口，把细节拆出去。

建议拆成：

```text
iFly/app/cli/flight_ctrl_cli.hpp/.cpp
iFly/app/cli/flight_ctrl_cli_parameters.hpp/.cpp
iFly/app/cli/flight_ctrl_cli_commands.hpp/.cpp
iFly/app/cli/flight_ctrl_cli_intro.hpp/.cpp
```

职责：

- `flight_ctrl_cli`：持有 `Shell`，负责 Init、SetOutput、ProcessInput。
- `flight_ctrl_cli_parameters`：注册参数，处理参数 get/set。
- `flight_ctrl_cli_commands`：注册 status、reboot 等命令。
- `flight_ctrl_cli_intro`：启动动画和 banner。

### 具体改法

建议分三次做，不要一次性全拆。

第一次：只拆参数注册。

1. 新增 `flight_ctrl_cli_parameters.hpp/.cpp`。
2. 把 `ManagedParameterContext` 和 `RegisterParameters()` 相关代码移过去。
3. `FlightCtrlCli::Init()` 里调用参数模块注册函数。

第二次：拆 commands。

1. 新增 `flight_ctrl_cli_commands.hpp/.cpp`。
2. 移动 `StatusFunction()`、`RebootFunction()`。

第三次：拆 intro。

1. 新增 `flight_ctrl_cli_intro.hpp/.cpp`。
2. 移动 `IntroAnimationState` 和动画步骤函数。

### 验证方式

1. CLI 参数列表和拆分前一致。
2. `status`、`reboot` 命令行为一致。
3. MAVLink console 输出行为一致。
4. `FlightCtrlCli` 主类变薄，只负责组织。

## P1：模块间通信尽量从“直接拿对象”变成“发布订阅”

### 当前问题

项目里已经有 `ObserverChannel`，但主干业务还没有明显使用它。

后续如果每个任务都直接拿别的模块指针，依赖会越来越乱。

### 建议怎么改

先选一个低风险数据流练手，例如 RC 输入。

建议新增：

```text
iFly/app/fly_sys/system_bus.hpp
iFly/app/fly_sys/system_bus.cpp
```

里面定义全局消息通道，例如：

```cpp
using RcChannel = ObserverChannel<RcChannels, 4U, 4U>;
using AttitudeChannel = ObserverChannel<Attitude, 4U, 4U>;
```

然后：

- RC 任务发布 `RcChannels`。
- 控制任务订阅 `RcChannels`。
- MAVLink 发送任务也可以订阅 `RcChannels`，定期发送给 QGC。

### 验证方式

1. 发布者不知道消费者是谁。
2. 消费者不知道数据来自哪个具体任务。
3. 新增 MAVLink telemetry 消费 RC 数据时，不需要改 RC 任务。

## P2：减少业务层对单例的直接依赖

### 当前问题

项目里单例比较多，例如：

- `ProjectParameterManager::Instance()`
- `SoftTimerService::Instance()`
- `UsbCdcAcm::Instance()`
- `UartDmaService::Instance()`

嵌入式里这很常见，不是错误。

真正需要注意的是：不要让业务逻辑到处直接拿全局单例。

### 建议原则

底层驱动服务可以用单例，因为 HAL 回调需要找到对象。

业务模块尽量通过构造函数或 `BindXxx()` 接收依赖。

例如：

```cpp
class SomeModule {
public:
  explicit SomeModule(ProjectParameterManager *parameters);
};
```

比在模块内部反复写：

```cpp
ProjectParameterManager::Instance()
```

更容易测试，也更容易看出依赖关系。

### 不建议现在大改

现在不建议全项目去掉单例。成本大，收益短期不明显。

建议从新模块开始遵守：

- 新业务类不要主动拿全局单例。
- 在 `task_all_init.cpp` 或初始化层把依赖传进去。

## P2：区分“协议参数名”和“内部配置结构”

### 当前问题

现在 QGC/MAVLink 参数名直接绑定到内部 `ProjectParameters` 偏移。

这个实现简洁、有效，但要注意一个风险：以后如果内部结构变化，可能影响外部参数名兼容。

### 建议怎么改

短期不需要改。

长期可以把参数分成两层：

```text
内部配置结构：ControlParameters、BatteryParameters、MotorParameters
外部参数表：MAVLink/QGC 可见的参数名、类型、范围、读写权限
```

也就是保持：

```text
外部名字稳定，内部结构可调整。
```

例如 QGC 看到的名字仍然是 `ANG_PID_P`，但内部可以从 `control.angle_pid.kp` 改成其他存储方式。

### 验证方式

1. QGC 参数名保持稳定。
2. 内部结构调整后，外部 `PARAM_REQUEST_LIST` 结果不变。
3. PID 任务读到的配置不变。

## P2：逐步补“模块 README”

### 当前问题

很多目录的职责需要靠读代码推断。

对学习架构来说，每个关键目录有一个小 README 会非常有帮助。

### 建议新增

```text
iFly/app/fly_sys/README.md
iFly/app/cli/README.md
iFly/app/fly_sys/ground_station/README.md
iFly/dev/README.md
iFly/task/README.md
```

每个 README 只写三件事：

1. 这个目录负责什么。
2. 这个目录可以依赖谁。
3. 这个目录不应该做什么。

示例：

```markdown
# ground_station

负责地面站协议适配，目前主要是 MAVLink。

可以依赖：
- fly_sys state types
- parameter manager
- SerialIoBase through MavlinkLink

不应该做：
- 不直接控制电机
- 不直接读写 HAL
- 不实现 PID 控制算法
```

### 验证方式

新同学或未来的自己看 README 后，能判断新文件应该放在哪个目录。

## 暂时不建议做的事情

### 1. 不建议现在引入复杂框架

当前项目是嵌入式飞控，不建议为了架构好看引入复杂依赖注入框架、动态分配框架或大型事件系统。

优先使用简单 C++ 类、固定数组、显式初始化、显式 `BindXxx()`。

### 2. 不建议一次性大重构

现在最好的节奏是：

```text
接通一个模块 -> 验证 -> 再拆一点 -> 再验证
```

不要一次性移动十几个文件、重命名多个目录、重写任务系统。这样很容易把能工作的部分弄坏。

### 3. 不建议业务代码直接写进 `middlewares`

MAVLink 和 DroneCAN 生成文件不要直接改。

需要扩展时，在 `iFly/app/fly_sys/ground_station` 或你自己的 wrapper 里做。

### 4. 不建议让 task 文件承担业务逻辑

`task_xxx.cpp` 应该像启动脚手架：

- 创建任务
- 拿到上下文
- 调用模块 `Poll()` 或 `Update()`

不建议在 task 文件里写：

- MAVLink 参数协议细节
- PID 算法细节
- CLI 参数解析
- 传感器融合逻辑

## 推荐改造顺序

建议按这个顺序做，每一步都能独立验证。

### 第 1 步：接通 MAVLink task

目标：

- `task_mavlink.cpp` 真正调用 `MavlinkReceiver::Poll()`。
- SERIAL_CONTROL 能进入 `MavlinkConsole`。
- QGC 参数请求能被任务周期处理。

验证：

```powershell
cmake --build build\Debug
```

然后用 QGC 请求参数列表。

### 第 2 步：整理参数目录

目标：

- 参数定义和参数管理器放在同一个清晰目录。
- 不再使用 `paramete` 这种不清楚目录名。

验证：

```powershell
rg -n "paramete" iFly CMakeLists.txt
cmake --build build\Debug
```

### 第 3 步：拆 MAVLink 参数处理器

目标：

- `MavlinkReceiver` 不直接处理参数协议细节。
- 参数协议集中在 `mavlink_parameter_handler`。

验证：

- QGC 参数列表和设置参数功能不变。
- `MavlinkReceiver` 文件变薄。

### 第 4 步：拆 CLI 参数注册

目标：

- `FlightCtrlCli` 不再直接包含大量参数注册细节。
- CLI 参数逻辑集中在独立文件。

验证：

- CLI 参数 get/set 行为不变。
- `status`、`reboot` 行为不变。

### 第 5 步：选一个数据流接入 ObserverChannel

目标：

- 先用 RC 输入或电池状态练习发布订阅。
- 发布者和消费者不互相直接依赖。

验证：

- 新增一个消费者，不需要修改发布者。

## 每次改架构时的检查清单

每次改完一个小步骤，建议检查：

```text
1. 编译是否通过。
2. 原有功能是否还能用。
3. 是否只改了目标模块。
4. 是否减少了某个文件的职责。
5. 是否让依赖方向更清楚。
6. 是否没有把业务逻辑写到 lib/dev/middlewares 里。
7. 是否没有让 task 文件变胖。
```

## 判断一个文件职责是否过重的方法

如果一个文件里同时出现下面三类以上内容，就要考虑拆：

```text
协议解析
参数读写
硬件 IO
业务状态机
算法计算
CLI 命令
任务调度
数据存储
```

例如 `FlightCtrlCli` 同时有 CLI、参数、命令、动画，所以后续适合拆。

例如 `MavlinkReceiver` 同时覆盖参数、命令、mission、console，所以后续适合按 handler 拆。

## 推荐的最终结构方向

这是一个未来方向，不要求马上改到这样：

```text
iFly/
  app/
    cli/
      flight_ctrl_cli.*
      flight_ctrl_cli_parameters.*
      flight_ctrl_cli_commands.*
      flight_ctrl_cli_intro.*
    control/
      pid.*
      controller modules...
    fly_sys/
      parameters/
        sys_parameters.*
        parameter_manager.*
      state/
        vehicle_state.hpp
        sensor_state.hpp
        control_state.hpp
        communication_state.hpp
      ground_station/
        mavlink_link.*
        mavlink_send.*
        mavlink_receiver.*
        mavlink_parameter_handler.*
        mavlink_command_handler.*
        mavlink_console.*
      system_bus.*
    mixer/
    proto/
    tick/
  dev/
    serial_io_base.hpp
    usb_dev/
    uart_dev/
    can_dev/
  lib/
  task/
    task_all_init.*
    task_mavlink.cpp
    task_pid_ctrl.cpp
    task_led_ctrl.cpp
```

这个结构的核心思想是：

```text
协议归协议，控制归控制，参数归参数，任务只负责调度。
```

## 学习架构时最重要的原则

架构不是把文件拆得越多越好，而是让每个模块的责任清楚。

你后面写新功能时，可以先问自己三个问题：

1. 这个功能属于哪个领域？
2. 它应该依赖谁？
3. 它以后变复杂时，应该往哪里长？

如果这三个问题能回答清楚，代码就不容易乱。

