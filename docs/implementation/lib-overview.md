---
title: 库层概览
description: iFly/lib 目录中的底层模块、职责和实现技术
---

# 库层概览

`iFly/lib/` 提供底层基础设施。应用层与设备抽象层通过这些模块访问时间基准、缓冲、通信和外设驱动。

## 模块清单

| 模块目录 | 主要文件 | 作用 | 实现技术 |
| --- | --- | --- | --- |
| `freelock_queue` | `lock_free_queue.hpp/.cpp` | 字节环形队列 | `std::atomic`、保留 1 字节空位、CAS 更新尾指针 |
| `double_buffer` | `double_buffer.hpp` | 收发缓冲切换 | 双缓冲、活动槽/备用槽交换 |
| `soft_timer` | `soft_timer.hpp/.cpp` | 1 ms 软定时器服务 | 固定任务槽、优先级调度、延后执行 |
| `systick_timer` | `systick_time.hpp/.cpp` | 毫秒、微秒、纳秒时间基准 | `HAL_GetTick()`、`DWT->CYCCNT`、回绕扩展 |
| `shell` | `shell.hpp/.cpp` | 命令行会话与命令分发 | 状态机、参数注册、函数注册 |
| `uart` | `uart_dma.hpp/.cpp` | UART DMA 服务 | HAL 回调桥接、DMA 环形接收、TX 双缓冲 |
| `usb_cdc` | `usb_cdc.hpp/.cpp` | USB CDC ACM 协议层 | 描述符、EP0 控制传输、Bulk/Interrupt 端点 |
| `can` | `can.hpp/.cpp` | CAN 服务 | 帧封装、HAL 回调桥接、队列收发 |
| `pwm` | `pwm.hpp/.cpp` | PWM 输出控制 | HAL 定时器通道封装 |
| `led` | `led.hpp/.cpp` | LED GPIO 控制 | 逻辑状态与物理电平映射 |
| `platform` | `platform_handle.hpp` | 通用句柄到 HAL 句柄的转换 | 平台适配内联函数 |
| `usermath` | `usermath.hpp` | 常用模板数学函数 | `Min`、`Max`、`Clamp` |

## 当前在主链路中的核心模块

当前运行链路直接依赖的库模块包括：

- `systick_timer`
- `soft_timer`
- `shell`
- `freelock_queue`
- `double_buffer`
- `uart`
- `usb_cdc`

## 实现共性

这些底层模块存在以下共性：

- 单例服务对象用于集中管理共享硬件资源，例如 `SoftTimerService`、`UartDmaService`、`UsbCdcAcm`
- 热路径使用固定大小缓冲，避免在任务分发与中断回调中执行动态分配
- HAL 回调通过 `extern "C"` 转发到 C++ 服务对象
- 上层接口统一为面向对象或函数式接口，下层与 HAL 句柄的差异通过 `platform_handle.hpp` 隔离
