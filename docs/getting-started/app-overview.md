---
title: 应用层总览
description: iFly/app 目录中的模块职责、接口和当前接入状态
---

# 应用层总览

`iFly/app/` 目录提供应用层接口，当前包含 `cli`、`ctrl`、`observer`、`paramete`、`proto`、`task_mage`、`tick` 七类模块。

## 模块清单

| 模块目录 | 主要对外符号 | 作用 | 当前接入状态 |
| --- | --- | --- | --- |
| `app/cli` | `FlightCtrlCli` | 组合 `Shell`、参数中心和业务函数 | 已在 `InitAllTasks()` 中接入 |
| `app/ctrl` | `Pid` | 控制器参数配置与单次更新 | 已被 `FlightCtrlCli` 持有 |
| `app/observer` | `ObserverChannel`、`ObserverHub` | 单发布者、多消费者的快照分发 | 已实现，当前未在主任务链路中接入 |
| `app/paramete` | `ProjectParameters`、`ProjectParameterManager` | 工程参数树、按名读写、变更回调 | 已在 CLI 中接入 |
| `app/proto/crsf` | `CrsfProtocol` | CRSF 帧编码、解码与流式解析 | 已实现，当前未在主任务链路中接入 |
| `app/proto/sbus` | `SbusProtocol` | SBUS 帧编码、解码与流式解析 | 已实现，当前未在主任务链路中接入 |
| `app/task_mage` | `TaskCreatePeriodic`、`TaskDispatch` 等 | 对 `SoftTimerService` 的任务语义封装 | 已在主循环中接入 |
| `app/tick` | `NowMs`、`NowUs`、`NonBlockingDelayMs` | 对 `systick_timer` 的上层时间接口封装 | 已被 CLI 和任务链路使用 |

## 当前应用链路

当前工程中的应用层调用关系如下：

```text
app_main()
  -> InitAllTasks()
    -> FlightCtrlCli::Init()
    -> UsbUart::Init()
    -> TaskCreatePeriodic(pid_ctrl)
    -> TaskCreatePeriodic(led_ctrl)
    -> TaskCreatePeriodic(cli_poll)
  -> while (1) { TaskDispatch(); }
```

## `iFly/task/` 与 `iFly/app/task_mage/` 的关系

- `iFly/app/task_mage/` 提供任务框架接口，例如 `TaskCreatePeriodic()`、`TaskDelete()`、`TaskDispatch()`。
- `iFly/task/` 保存当前工程实际创建的三个任务：`task_pid_ctrl.cpp`、`task_led_ctrl.cpp`、`task_cli_poll.cpp`。
- `app_main()` 不直接调用业务逻辑，而是通过 `TaskDispatch()` 驱动 `iFly/task/` 中已经注册的任务。

## 头文件引入方式

`CMakeLists.txt` 已经把 `iFly/app/*`、`iFly/dev/*`、`iFly/lib/*` 的目录加入 `target_include_directories(...)`。应用层示例可以直接写成：

```cpp
#include "flight_ctrl_cli.hpp"
#include "project_parameter_manager.hpp"
#include "task.hpp"
#include "tick.hpp"
```
