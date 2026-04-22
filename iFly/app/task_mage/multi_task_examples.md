# 多任务拆分示例

你现在这套工程已经有现成的任务管理接口：

- `iFly::TaskCreatePeriodic(...)`
- `iFly::TaskCreateOneShot(...)`
- `iFly::TaskDispatch()`

要做到“多个任务，每个任务一个 `.cpp` 文件”，推荐按下面这个结构来做：

```text
iFly/app/task_mage/
├── task.cpp
├── task.hpp
├── task_led_blink.cpp
├── task_boot_once.cpp
├── task_cli_poll.cpp
├── task_registry.cpp
├── task_registry.hpp
└── multi_task_examples.md
```

## 一、推荐做法

1. 每个任务单独放在一个 `.cpp` 文件里。
2. 每个任务文件里只做一件事：保存自己的上下文、写自己的回调、提供自己的初始化函数。
3. 再用一个 `task_registry.cpp` 统一把所有任务注册起来。
4. 在 `app_main.cpp` 主循环里持续调用 `iFly::TaskDispatch()`，否则任务不会执行。
5. 在 `CMakeLists.txt` 里把新增的 `.cpp` 文件加进去。

## 二、两个关键点

### 1. `TaskDispatch()` 必须在主循环里跑

你现在的任务框架不是抢占式 RTOS 任务，而是主循环里手动派发：

```cpp
(void)iFly::TaskDispatch();
```

如果主循环里不调用它，任务虽然创建成功了，也不会真正执行。

### 2. 周期任务默认不是立即执行

`TaskCreatePeriodic()` 的 `start_delay_ms` 默认值是 `kUsePeriodAsStartDelay`，也就是：

- 周期是 `500ms`
- 默认第一次执行也会等 `500ms`

如果你想“创建后立刻开始第一次执行”，要显式传 `0U`：

```cpp
iFly::TaskCreatePeriodic(callback,
                         context,
                         500U,
                         iFly::SoftTimerService::kLowestPriority,
                         0U,
                         "task_name");
```

## 三、示例代码

下面这个示例里有 3 个任务：

- `task_led_blink.cpp`：500ms 翻转一次绿色 LED
- `task_boot_once.cpp`：系统启动 1000ms 后执行一次
- `task_cli_poll.cpp`：50ms 轮询一次 CLI

### 1. `task_led_blink.cpp`

```cpp
#include "task.hpp"
#include "led.hpp"
#include "main.h"

namespace app_task {
namespace {

iFly::Led g_green_led;
iFly::TaskHandle g_led_blink_handle = iFly::kInvalidTaskHandle;

void LedBlinkTask(void *context)
{
  iFly::Led *led = static_cast<iFly::Led *>(context);
  if ((led == nullptr) || !led->IsReady()) {
    return;
  }

  (void)led->Toggle();
}

} // namespace

bool InitLedBlinkTask()
{
  const iFly::LedConfig led_config {
      LED_G_GPIO_Port,
      LED_G_Pin,
      iFly::LedActiveLevel::kHigh,
      false,
  };

  if (!g_green_led.Init(led_config)) {
    return false;
  }

  g_led_blink_handle = iFly::TaskCreatePeriodic(&LedBlinkTask,
                                                &g_green_led,
                                                500U,
                                                iFly::SoftTimerService::kLowestPriority,
                                                0U,
                                                "led_blink");

  return g_led_blink_handle != iFly::kInvalidTaskHandle;
}

} // namespace app_task
```

### 2. `task_boot_once.cpp`

```cpp
#include "task.hpp"
#include "led.hpp"
#include "main.h"

namespace app_task {
namespace {

iFly::Led g_red_led;
iFly::TaskHandle g_boot_once_handle = iFly::kInvalidTaskHandle;

void BootOnceTask(void *context)
{
  iFly::Led *led = static_cast<iFly::Led *>(context);
  if ((led == nullptr) || !led->IsReady()) {
    return;
  }

  (void)led->On();
}

} // namespace

bool InitBootOnceTask()
{
  const iFly::LedConfig led_config {
      LED_R_GPIO_Port,
      LED_R_Pin,
      iFly::LedActiveLevel::kHigh,
      false,
  };

  if (!g_red_led.Init(led_config)) {
    return false;
  }

  g_boot_once_handle = iFly::TaskCreateOneShot(&BootOnceTask,
                                               &g_red_led,
                                               1000U,
                                               iFly::SoftTimerService::kLowestPriority,
                                               "boot_once");

  return g_boot_once_handle != iFly::kInvalidTaskHandle;
}

} // namespace app_task
```

### 3. `task_cli_poll.cpp`

这个例子演示“通过 `context` 给任务传对象指针”。

```cpp
#include "task.hpp"
#include "flight_ctrl_cli.hpp"

namespace app_task {
namespace {

struct CliPollContext final {
  iFly::FlightCtrlCli *cli = nullptr;
};

CliPollContext g_cli_poll_context {};
iFly::TaskHandle g_cli_poll_handle = iFly::kInvalidTaskHandle;

void CliPollTask(void *context)
{
  CliPollContext *ctx = static_cast<CliPollContext *>(context);
  if ((ctx == nullptr) || (ctx->cli == nullptr)) {
    return;
  }

  ctx->cli->Poll();
}

} // namespace

bool InitCliPollTask(iFly::FlightCtrlCli *cli)
{
  if (cli == nullptr) {
    return false;
  }

  g_cli_poll_context.cli = cli;

  g_cli_poll_handle = iFly::TaskCreatePeriodic(&CliPollTask,
                                               &g_cli_poll_context,
                                               50U,
                                               iFly::SoftTimerService::kLowestPriority,
                                               0U,
                                               "cli_poll");

  return g_cli_poll_handle != iFly::kInvalidTaskHandle;
}

} // namespace app_task
```

### 4. `task_registry.hpp`

```cpp
#ifndef IFLY_APP_TASK_REGISTRY_HPP
#define IFLY_APP_TASK_REGISTRY_HPP

namespace iFly {
class FlightCtrlCli;
}

namespace app_task {

bool InitAllTasks(iFly::FlightCtrlCli *cli);

} // namespace app_task

#endif /* IFLY_APP_TASK_REGISTRY_HPP */
```

### 5. `task_registry.cpp`

这个文件专门负责统一注册所有任务。

```cpp
#include "task_registry.hpp"
#include "flight_ctrl_cli.hpp"

namespace app_task {

bool InitLedBlinkTask();
bool InitBootOnceTask();
bool InitCliPollTask(iFly::FlightCtrlCli *cli);

bool InitAllTasks(iFly::FlightCtrlCli *cli)
{
  bool ok = true;

  ok = InitLedBlinkTask() && ok;
  ok = InitBootOnceTask() && ok;
  ok = InitCliPollTask(cli) && ok;

  return ok;
}

} // namespace app_task
```

## 四、`app_main.cpp` 怎么改

你现在的 `app_main.cpp` 里 CLI 是在 `while (1)` 里手动轮询的。  
如果你改成任务方式，可以这样写：

```cpp
#include "app_main.h"

#include <stdint.h>

#include "flight_ctrl_cli.hpp"
#include "hardware_uart.hpp"
#include "main.h"
#include "task.hpp"
#include "task_registry.hpp"
#include "tick.hpp"
#include "usb_uart.hpp"

namespace {

constexpr uint32_t kCliRxQueueSize = 1024U;
constexpr uint32_t kMainLoopDelayMs = 1U;
constexpr char kDefaultCliTransport[] = "usb";

iFly::HardwareUart g_uart5(iFly::UartPortId::kUart5, kCliRxQueueSize);
iFly::UsbUart g_usb_cli(kCliRxQueueSize);
iFly::FlightCtrlCli g_flight_ctrl_cli;

void InitCliRuntime()
{
  g_uart5.Init();
  g_usb_cli.Init();

  g_flight_ctrl_cli.Init();
  (void)g_flight_ctrl_cli.RegisterTransport("uart5", &g_uart5);
  (void)g_flight_ctrl_cli.RegisterTransport("usb", &g_usb_cli);
  (void)g_flight_ctrl_cli.UseTransport(kDefaultCliTransport);
}

} // namespace

extern "C" void app_main(void)
{
  InitCliRuntime();
  iFly::tick::DelayMs(20U);

  (void)app_task::InitAllTasks(&g_flight_ctrl_cli);

  while (1) {
    (void)iFly::TaskDispatch();
    iFly::tick::DelayMs(kMainLoopDelayMs);
  }
}
```

## 五、`CMakeLists.txt` 怎么加

你新增了 `.cpp` 文件后，还要加入 `target_sources(...)`：

```cmake
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    iFly/app_main.cpp
    iFly/app/cli/flight_ctrl_cli.cpp
    iFly/app/ctrl/pid.cpp
    iFly/app/paramete/project_parameter_manager.cpp
    iFly/app/paramete/project_parameters.cpp
    iFly/app/task_mage/task.cpp
    iFly/app/task_mage/task_registry.cpp
    iFly/app/task_mage/task_led_blink.cpp
    iFly/app/task_mage/task_boot_once.cpp
    iFly/app/task_mage/task_cli_poll.cpp
    iFly/app/tick/tick.cpp
    iFly/app/proto/crsf/crsf_protocol.cpp
    iFly/app/proto/sbus/sbus_protocol.cpp
    iFly/dev/can_dev/hardware_can.cpp
    iFly/dev/uart_dev/hardware_uart.cpp
    iFly/dev/usb_dev/usb_uart.cpp
    iFly/lib/freelock_queue/lock_free_queue.cpp
    iFly/lib/shell/shell.cpp
    iFly/lib/soft_timer/soft_timer.cpp
    iFly/lib/systick_timer/systick_time.cpp
    iFly/lib/uart/uart_dma.cpp
    iFly/lib/usb_cdc/usb_cdc.cpp
    iFly/lib/can/can.cpp
    iFly/lib/pwm/pwm.cpp
    iFly/lib/led/led.cpp
)
```

## 六、以后继续扩展时怎么写

以后你再加新任务，基本就重复这个模式：

1. 新建一个新的任务文件，比如 `task_imu_update.cpp`
2. 在里面写这个任务自己的回调和初始化函数
3. 在 `task_registry.cpp` 里把它注册进去
4. 在 `CMakeLists.txt` 里把这个 `.cpp` 加进去

比如：

```cpp
bool InitImuUpdateTask()
{
  return iFly::TaskCreatePeriodic(&ImuUpdateTask,
                                  &g_imu_context,
                                  2U,
                                  10U,
                                  0U,
                                  "imu_update") != iFly::kInvalidTaskHandle;
}
```

## 七、注意事项

### 1. 不要在任务回调里写长时间阻塞代码

例如不要在任务回调里长时间调用：

```cpp
iFly::tick::DelayMs(1000U);
```

因为你这套任务是非抢占式的，一个任务卡住，别的任务也会被拖住。

### 2. 优先级是“值越小优先级越高”

例如：

```cpp
iFly::SoftTimerService::kHighestPriority
iFly::SoftTimerService::kLowestPriority
```

如果你自己写数字：

- `0` 很高
- `255` 很低

### 3. 最大任务数目前是 16 个

你的任务上限来自：

```cpp
iFly::SoftTimerService::kMaxTasks
```

当前值是 `16U`。

### 4. 建议一个任务文件只暴露一个初始化函数

比如：

```cpp
bool InitLedBlinkTask();
bool InitCliPollTask(iFly::FlightCtrlCli *cli);
bool InitBootOnceTask();
```

这样后面维护最清楚。

## 八、最小理解版本

如果你只记一个模式，就记这个：

```cpp
// task_xxx.cpp
#include "task.hpp"

namespace app_task {
namespace {

struct TaskContext final {
  int value = 0;
};

TaskContext g_context {};
iFly::TaskHandle g_task_handle = iFly::kInvalidTaskHandle;

void XxxTask(void *context)
{
  TaskContext *ctx = static_cast<TaskContext *>(context);
  if (ctx == nullptr) {
    return;
  }

  ctx->value++;
}

} // namespace

bool InitXxxTask()
{
  g_task_handle = iFly::TaskCreatePeriodic(&XxxTask,
                                           &g_context,
                                           100U,
                                           iFly::SoftTimerService::kLowestPriority,
                                           0U,
                                           "xxx_task");

  return g_task_handle != iFly::kInvalidTaskHandle;
}

} // namespace app_task
```

然后：

- 在 `task_registry.cpp` 里调用 `InitXxxTask()`
- 在 `app_main.cpp` 里调用 `iFly::TaskDispatch()`
- 在 `CMakeLists.txt` 里把 `task_xxx.cpp` 加进去

这就是“一个任务一个 `.cpp` 文件”的完整做法。
