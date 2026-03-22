# USB CDC 分层重构说明

## 1. 本次重构目标

本目录下的 USB CDC 链路已经重构为下面这套分层架构：

`USB PCD -> 发送双缓冲区 / 接收环形缓冲区 -> UsbCdcAcm -> 无锁队列 -> usb_uart`

重构重点如下：

- 底层 PCD 适配逻辑收敛在 `usb_cdc.cpp` 内部，不再向上层暴露 HAL 细节。
- 接收方向新增传输层环形缓冲区，用于吸收 USB OUT 中断突发数据。
- 发送方向新增双缓冲区，用于让 USB IN 发送与下一包预取并行推进。
- `usb_uart` 公开继承无锁队列类，对应用层直接提供队列接口与串口接口。
- 所有动态内存都只在创建/初始化阶段申请，运行过程中不再申请内存。
- 以后若要移植到别的 USB 控制器或别的 HAL，只需要修改底层 PCD 适配部分。

---

## 2. 文件职责

### 2.1 `usb_cdc.hpp`

该头文件定义了三个核心模块：

- `ByteRingBuffer`
  - 底层接收环形缓冲区。
  - 处于 `USB PCD -> UsbCdcAcm` 之间。
  - USB OUT 中断先把收到的数据压入这里，再由 `UsbCdcAcm` 搬运到上层队列。

- `UsbTxDoubleBuffer`
  - 底层发送双缓冲区。
  - 处于 `UsbCdcAcm -> USB PCD` 之间。
  - 一个槽位正在发时，另一个槽位可以继续从发送队列预取下一包。

- `UsbCdcAcm`
  - USB CDC 协议层核心。
  - 负责标准请求、CDC 类请求、端点状态、上下层缓冲转运。

### 2.2 `usb_cdc.cpp`

该文件包含：

- STM32 HAL PCD 适配器 `Stm32FsPcdAdapter`
- USB 描述符
- `ByteRingBuffer` 实现
- `UsbTxDoubleBuffer` 实现
- `UsbCdcAcm` 协议层实现
- `HAL_PCD_*Callback()` 到 `UsbCdcAcm` 的桥接

### 2.3 `../freelock_queue/lock_free_queue.hpp`

无锁队列基础组件，当前同时提供：

- `LockFreeQueueBase`
- `StaticLockFreeQueue`
- `DynamicLockFreeQueue`

其中本次重构新增的 `DynamicLockFreeQueue` 用于：

- `usb_uart` 的上层接收队列
- `UsbCdcAcm` 的内部发送队列

### 2.4 `../../dev/usb_dev/usb_uart.hpp`

`usb_uart` 现在的定位是：

- 业务层直接使用的 USB 虚拟串口对象
- 同时也是一个接收无锁队列对象

也就是说，`usb_uart` 不只是“包装器”，它本身就承载了
`usb_cdc -> 无锁队列 -> usb_uart` 这一层中的“无锁队列”角色。

---

## 3. 当前数据流

## 3.1 接收方向

接收链路如下：

1. 主机向 BULK OUT 端点发送数据。
2. STM32 HAL 触发 `HAL_PCD_DataOutStageCallback()`。
3. `UsbCdcAcm::OnDataOutStage()` 读取本次收包长度。
4. 数据先从 `rxPacketBuffer_` 压入 `ByteRingBuffer`。
5. `UsbCdcAcm::ServiceRxPath()` 再把环形缓冲区中的数据搬运到 `usb_uart` 继承的无锁队列。
6. 业务层调用 `usb_uart.Read()` 或直接调用基类 `Dequeue()` 取走数据。

### 为什么要多一层接收环形缓冲区？

这样做有三个好处：

- USB OUT 中断处理更短，只负责“收包 + 压入底层环形缓冲区”。
- 当上层一时来不及读数据时，底层还有一层缓冲可以吸收突发流量。
- `usb_cdc` 与 `usb_uart` 的职责边界更加清晰。

---

## 3.2 发送方向

发送链路如下：

1. 业务层调用 `usb_uart.Write(data, len)`。
2. 数据先进入 `UsbCdcAcm` 内部发送无锁队列 `txQueue_`。
3. `UsbCdcAcm::ServiceTxPath()` 从发送队列取数据，拆成不超过 64 字节的 USB 包。
4. 每个 USB 包被装载进 `UsbTxDoubleBuffer` 的两个槽位之一。
5. USB IN 端点空闲时，当前就绪槽位立即发出。
6. 本包发送期间，另一槽位仍可继续从发送队列预取下一包。
7. `HAL_PCD_DataInStageCallback()` 到来后，释放当前槽位并继续推进下一包发送。

### 为什么要用发送双缓冲区？

这样做有两个直接收益：

- 减少 `USB IN 完成 -> 下一包装载 -> 再次启动发送` 之间的空隙。
- 保持 `UsbCdcAcm` 对上层仍然是“写入字节流”，上层不需要自己分包。

---

## 4. 分层职责边界

### 4.1 底层：`Stm32FsPcdAdapter`

当前平台相关代码集中在 `usb_cdc.cpp` 的 `Stm32FsPcdAdapter` 中，主要负责：

- 获取 `hpcd_USB_OTG_FS`
- 配置 FIFO
- 启动 PCD
- 打开/关闭端点
- 发包
- 挂起接收
- 读取 setup 包和接收长度
- 设置地址 / STALL

### 4.2 协议层：`UsbCdcAcm`

负责：

- 处理 USB 标准请求
- 处理 CDC 类请求
- 处理配置、挂起、恢复、端点状态
- 在底层缓冲与上层队列之间做数据搬运

### 4.3 应用接口层：`usb_uart`

负责：

- 暴露 `Init()` / `Write()` / `Read()` 等串口风格接口
- 公开继承无锁队列，让应用层也能直接用队列接口
- 把自身接收队列挂接给 `UsbCdcAcm`

---

## 5. 为什么说移植时只需要改底层？

因为当前上层接口已经与 STM32 HAL PCD 解耦：

- `usb_uart.hpp` 不再依赖 `usb_otg.h`
- `usb_uart` 不再接收 `PCD_HandleTypeDef *`
- `UsbCdcAcm` 头文件不再暴露 HAL 类型
- HAL 回调桥接和端点操作都被关在 `usb_cdc.cpp`

因此如果以后迁移到别的平台，例如：

- STM32 的另一个 USB 控制器
- 其他芯片厂商的 USB Device HAL
- 自研 USB Device Driver

通常只需要改下面这些内容：

- `Stm32FsPcdAdapter` 这一层的端点操作实现
- HAL 回调到 `UsbCdcAcm` 的桥接部分

业务层 `app_main.cpp` 和 `usb_uart` 的使用方式都不需要改。

---

## 6. 动态内存策略

本次实现允许使用动态分配，但遵守以下约束：

- 只允许在初始化/构造阶段申请内存
- 运行期间收发数据时不再申请内存

当前实际分配点如下：

- `UsbUart` 构造时创建应用层接收队列
- `UsbCdcAcm` 构造/初始化时创建：
  - 发送无锁队列
  - 接收环形缓冲区
  - 发送双缓冲区

这满足“初始化时一次性分配、运行期零分配”的要求。

---

## 7. 当前默认容量

### 7.1 `usb_uart` 接收队列

- 总存储：`2049` 字节
- 实际可用：`2048` 字节

### 7.2 `UsbCdcAcm` 内部发送队列

- 总存储：`1025` 字节
- 实际可用：`1024` 字节

### 7.3 传输层接收环形缓冲区

- 总存储：`2049` 字节
- 实际可用：`2048` 字节

### 7.4 发送双缓冲区

- 槽位数：`2`
- 每槽位大小：`64` 字节
- 总物理缓冲：`128` 字节

如需修改吞吐能力，可直接调整 `usb_cdc.hpp` 和 `usb_uart.hpp` 中的容量常量。

---

## 8. 上层推荐用法

```cpp
#include "usb_uart.hpp"

static uint8_t g_usb_rx_temp[64];
static iFly::usb_uart g_usb_uart;

extern "C" void app_main(void)
{
  static bool initialized = false;
  if (!initialized)
  {
    g_usb_uart.Init();
    initialized = true;
    return;
  }

  const uint32_t rx_len = g_usb_uart.Read(g_usb_rx_temp, sizeof(g_usb_rx_temp));
  if (rx_len > 0U)
  {
    uint32_t sent = 0U;
    while (sent < rx_len)
    {
      const uint32_t pushed = g_usb_uart.Write(&g_usb_rx_temp[sent], rx_len - sent);
      if (pushed == 0U)
      {
        break;
      }
      sent += pushed;
    }
  }
}
```

除了 `Read()` / `Write()` 之外，应用层现在也可以直接使用继承来的无锁队列接口，例如：

- `g_usb_uart.Dequeue(...)`
- `g_usb_uart.UsedSize()`
- `g_usb_uart.FreeSize()`
- `g_usb_uart.IsEmpty()`

---

## 9. 调试与验证建议

本工程的编译/烧录/调试脚本位于：

- `E:\project\DragonFly_Project\myfly\fly_project\ps\build_stm32_project.ps1`
- `E:\project\DragonFly_Project\myfly\fly_project\ps\flash_stm32_project.ps1`
- `E:\project\DragonFly_Project\myfly\fly_project\ps\debug_stm32_project.ps1`
- `E:\project\DragonFly_Project\myfly\fly_project\ps\build_flash_debug_stm32_project.ps1`

推荐验证顺序：

1. 先编译，确认本次重构能够通过 CMake/Ninja。
2. 再烧录到板子。
3. 最后接上主机串口工具，验证：
   - 能否正常枚举出 CDC 设备
   - 收到的数据是否能正常回环
   - 连续发送时是否存在明显丢包或卡顿

---

## 10. 后续扩展建议

如果你后面还要继续扩展这套链路，优先建议做下面几项：

- 给 `usb_uart` 增加字符串发送、分隔符读取等更贴近业务的接口
- 在 `UsbCdcAcm` 中增加更细的统计量，例如：
  - 发送完成包数
  - 接收中断次数
  - 接收环形缓冲区峰值占用
- 若业务流量更大，可增大：
  - `kTransportRxRingStorageSize`
  - `kTxQueueStorageSize`
  - `UsbUart::kDefaultRxQueueStorageSize`

这样可以在保持分层结构不变的前提下继续提升吞吐与可观测性。
