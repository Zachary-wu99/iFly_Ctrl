---
title: CLI、参数与 PID
description: FlightCtrlCli、ProjectParameterManager 与 Pid 的用法说明
---

# CLI、参数与 PID

## `FlightCtrlCli`

`FlightCtrlCli` 位于 `iFly/app/cli/flight_ctrl_cli.*`，负责组合以下能力：

- `Shell` 会话管理与命令分发
- CLI 传输通道注册与切换
- 工程参数读写入口
- `status`、`pid.sample`、`sys.reboot` 等函数注册

当前工程中，`InitAllTasks()` 的初始化顺序如下：

```cpp
#include "flight_ctrl_cli.hpp"
#include "usb_uart.hpp"

namespace {

constexpr uint16_t kUsbRxQueueSize = 120U;
iFly::FlightCtrlCli g_cli;
iFly::UsbUart g_usb(kUsbRxQueueSize);

} // namespace

void InitCliRuntime()
{
  g_cli.Init();
  (void)g_cli.RegisterTransport("usb", &g_usb);
  (void)g_cli.UseTransport("usb");
  g_usb.Init();
}
```

`FlightCtrlCli::Init()` 内部会执行以下操作：

- 调用 `ProjectParameterManager::ResetToDefaults()`
- 清空并重新注册 `Shell` 命令、函数和参数
- 设置提示符、密码和会话动画
- 将参数中心中的 `control.rate_pid` 同步到内部 `Pid` 实例

## 当前已注册的 CLI 参数

| CLI 名称 | 对应工程参数 |
| --- | --- |
| `pid.kp` | `control.rate_pid.kp` |
| `pid.ki` | `control.rate_pid.ki` |
| `pid.kd` | `control.rate_pid.kd` |
| `pid.kff` | `control.rate_pid.kff` |
| `pid.i_min` | `control.rate_pid.integral_min` |
| `pid.i_max` | `control.rate_pid.integral_max` |
| `pid.out_min` | `control.rate_pid.output_min` |
| `pid.out_max` | `control.rate_pid.output_max` |
| `pid.d_cutoff_hz` | `control.rate_pid.derivative_cutoff_hz` |
| `sys.loop_hz` | `system.control_loop_hz` |
| `sys.arm_locked` | `system.arm_locked` |

此外，CLI 还暴露了两个只读参数：

- `sys.transport`
- `sys.uptime_ms`

## `ProjectParameterManager`

`ProjectParameterManager` 提供按名字访问的统一参数入口。参数元数据由 `GetProjectParameterBindings()` 返回的静态表构建，运行时持有的对象是 `ProjectParameters`。

读取与写入示例：

```cpp
#include "project_parameter_manager.hpp"

void UseParameters()
{
  auto &pm = iFly::ProjectParameterManager::Instance();

  uint32_t loop_hz = 0U;
  (void)pm.Read("system.control_loop_hz", &loop_hz);

  (void)pm.Write("system.arm_locked", false);
  (void)pm.Write("control.rate_pid.kp", 1.20f);
}
```

绑定参数变化回调示例：

```cpp
#include "project_parameter_manager.hpp"

static void OnPasswordUpdated(const char *name, void *context)
{
  (void)name;
  (void)context;
}

void BindParameterCallback()
{
  auto &pm = iFly::ProjectParameterManager::Instance();
  (void)pm.SetChangeHandler("cli.password", &OnPasswordUpdated, nullptr);
}
```

## `Pid`

`Pid` 位于 `iFly/app/ctrl/pid.*`。接口由三个部分组成：

- `Pid::Config`：控制器静态配置
- `Pid::UpdateInput`：单次控制更新输入
- `Pid::UpdateResult`：单次控制输出与中间量

单次更新示例：

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

## CLI 命令示例

当前 `FlightCtrlCli` 注册的函数包括：

- `status`
- `sys.reboot`
- `pid.reset`
- `pid.sample`
- `transport.list`
- `transport.use`

交互示例：

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
call transport.use usb
```
