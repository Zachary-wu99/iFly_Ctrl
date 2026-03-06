# USB CDC 模块说明

## 1. 模块位置
- 头文件接口: `iFly/lib/usb_cdc/usb_cdc.h`
- C++ 实现声明: `iFly/lib/usb_cdc/usb_cdc.hpp`
- C++ 实现定义: `iFly/lib/usb_cdc/usb_cdc.cpp`

## 2. 模块目标
本模块实现了一个轻量级 USB CDC ACM 设备，不依赖 ST USB Device 中间件，而是直接基于 `HAL PCD` 回调完成枚举、控制请求和数据收发。

设计约束如下：
- 不使用动态内存。
- 发送使用双缓冲区。
- 接收使用环形缓冲区。
- 直接对接 `USB_OTG_FS` 外设和 HAL 回调。

## 3. 总体结构
模块内部主要由四部分组成：

### 3.1 描述符与常量
在 `usb_cdc.cpp` 顶部定义：
- 设备描述符
- 配置描述符
- 字符串描述符
- 标准请求号、CDC 类请求号、端点号等常量

当前端点布局与 STM32 官方 CDC ACM 示例保持一致：
- `0x81`: CDC Data IN
- `0x01`: CDC Data OUT
- `0x82`: CDC Command IN

## 3.2 控制传输处理
控制传输通过 EP0 完成，主要处理：
- `GET_DESCRIPTOR`
- `SET_ADDRESS`
- `SET_CONFIGURATION`
- `GET_CONFIGURATION`
- `SET_LINE_CODING`
- `GET_LINE_CODING`
- `SET_CONTROL_LINE_STATE`

说明：
- `SET_ADDRESS` 采用与 STM32 官方 USB Device 栈一致的处理方式，先调用 `HAL_PCD_SetAddress()`，再返回状态包。
- EP0 IN 传输支持分包发送。
- EP0 OUT 主要用于接收 `SET_LINE_CODING` 的 7 字节参数。

## 3.3 发送路径
发送使用双缓冲静态队列，结构体为 `TxSlot`。

发送流程：
1. 上层调用 `IFly_USBCDC_Write()`。
2. 数据被拷贝到空闲的 `TxSlot` 中。
3. `TryStartTxTransfer()` 在端点空闲时启动一次 BULK IN 发送。
4. 每次发送完成后，在 `OnDataInStage()` 中继续补发剩余数据。
5. 一个槽位发完后自动释放，再尝试发送下一个槽位。

特点：
- 不依赖调用方缓冲区生命周期。
- 适合中断与主循环并发场景。
- 当两个槽位都占满时，`Write()` 可能只接收部分数据。

## 3.4 接收路径
接收由两级缓冲组成：
- 一级: `rxPacketBuffer_`，保存 USB OUT 端点当前收到的一包数据。
- 二级: `rxRing_`，作为上层读取使用的环形缓冲区。

接收流程：
1. `PrimeOutEndpoint()` 挂起一次 OUT 接收。
2. 主机发来一包数据后，`OnDataOutStage()` 被调用。
3. 数据先进入 `rxPacketBuffer_`。
4. 再通过 `PushRxData()` 搬运到环形缓冲区 `rxRing_`。
5. 上层通过 `IFly_USBCDC_Read()` 读取数据。

说明：
- 若环形缓冲区满，新字节会被丢弃，并累计到 `rxDropped_`。
- 模块本身不主动解析上层协议，只负责搬运字节流。

## 3.5 行为状态流图
下面这张小图描述了当前模块的核心行为：
- 接收侧：只要端点已经挂起接收，主机发来的数据会由 USB 中断主动收下；上层 `Read()` 只是把环形缓冲区里的数据取走。
- 发送侧：上层必须先调用 `Write()` 入队；一旦进入双缓冲队列，后续发包和续发由模块自动完成，不需要上层继续触发。

```text
接收侧

主机发送数据
    |
    v
USB OUT 端点收到一包
    |
    v
OnDataOutStage()
    |
    +--> 数据写入 rxPacketBuffer_
    |
    +--> PushRxData() 搬运到 rxRing_
    |
    +--> PrimeOutEndpoint() 立即挂起下一次接收
    |
    v
等待上层调用 IFly_USBCDC_Read()
    |
    v
上层从 rxRing_ 取走数据

说明：
- 上层不读，USB 仍然继续接收。
- 若 rxRing_ 满，新数据会被丢弃，并累计到 rxDropped_。


发送侧

上层调用 IFly_USBCDC_Write(data, len)
    |
    v
数据拷贝到 txSlots_[0/1]
    |
    v
TryStartTxTransfer()
    |
    +--> 若 USB 已配置、未挂起、端点空闲，则启动首包发送
    |
    v
USB IN 发送中
    |
    v
OnDataInStage()
    |
    +--> 更新当前槽位 sent 进度
    |
    +--> 若还有剩余数据，继续发送下一包
    |
    +--> 若当前槽位已发送完，释放槽位并尝试发送下一个槽位
    |
    v
队列清空后回到空闲

说明：
- 上层不需要为“同一批已经入队的数据”反复触发发送。
- 但新的数据必须再次调用 IFly_USBCDC_Write() 才会进入发送队列。
```
## 4. 公开接口

### 4.1 `IFly_USBCDC_Init(PCD_HandleTypeDef *hpcd)`
作用：初始化 USB CDC 协议层。

调用要求：
- 必须在 `MX_USB_OTG_FS_PCD_Init()` 之后调用。
- 典型调用方式：

```c
MX_USB_OTG_FS_PCD_Init();
IFly_USBCDC_Init(&hpcd_USB_OTG_FS);
```

### 4.2 `IFly_USBCDC_Write(const uint8_t *data, uint32_t len)`
作用：写入发送缓冲区。

返回值：
- 实际成功进入双缓冲队列的字节数。
- 若返回值小于 `len`，表示当前发送缓冲繁忙，需要稍后继续写。

### 4.3 `IFly_USBCDC_Read(uint8_t *data, uint32_t len)`
作用：从环形缓冲区中取出收到的数据。

返回值：
- 实际取出的字节数。

### 4.4 `IFly_USBCDC_Available(void)`
作用：查询当前还有多少接收数据未被上层读取。

### 4.5 `IFly_USBCDC_IsConfigured(void)`
作用：查询主机是否已完成 `SET_CONFIGURATION`。

## 5. HAL 回调绑定关系
本模块直接接管以下 HAL PCD 回调：
- `HAL_PCD_ResetCallback`
- `HAL_PCD_SetupStageCallback`
- `HAL_PCD_DataInStageCallback`
- `HAL_PCD_DataOutStageCallback`
- `HAL_PCD_SuspendCallback`
- `HAL_PCD_ResumeCallback`

它们在 `usb_cdc.cpp` 末尾实现，并转发给 `UsbCdcAcm` 单例。

## 6. 与主循环的典型配合方式
当前工程 `main.c` 中使用的是“读到什么就回发什么”的回环测试方式，核心写法如下：

```c
const uint32_t rx_len = IFly_USBCDC_Read(g_usb_rx_temp, sizeof(g_usb_rx_temp));
if (rx_len > 0U)
{
  uint32_t sent = 0U;
  while (sent < rx_len)
  {
    const uint32_t pushed = IFly_USBCDC_Write(&g_usb_rx_temp[sent], rx_len - sent);
    if (pushed == 0U)
    {
      break;
    }
    sent += pushed;
  }
}
```

如果后续改为业务协议，通常只需要把“回环发送”替换成你的业务解析和回复逻辑即可。

## 7. 已知特性与注意事项

### 7.1 不依赖 USB Device 中间件
优点：
- 代码路径短。
- 行为完全可控。
- 更方便做静态内存设计。

代价：
- 需要自己维护标准请求和类请求的兼容性。
- 若要扩展为 HID、MSC、复合设备，工作量会明显增加。

### 7.2 发送不是“无限缓存”
发送只有两个 `TxSlot`，每个槽位 256 字节。
若上层持续写入速度高于 USB 实际发送速度，`Write()` 会出现部分写入或 0 字节写入。

### 7.3 接收可能丢包
若主机持续发送但上层读取不及时，环形缓冲区可能写满，之后新字节会被丢弃。
如需更高吞吐，可考虑：
- 增大 `kRxRingSize`
- 去掉主循环中的阻塞延时
- 提高上层读取频率

### 7.4 当前测速结果应按“应用层回环能力”理解
如果用当前 `main.c` 的回环逻辑测速，测到的是：
- 当前主循环调度频率
- 当前发送双缓冲机制
- 当前接收环形缓冲读取策略
共同决定的“应用层有效吞吐”，并不是 USB FS 总线极限。

## 8. 后续可优化方向
- 去掉主循环中不必要的 `HAL_Delay()`。
- 让主循环一次处理更多接收数据，而不是小包轮询。
- 在高吞吐场景下调大环形缓冲区。
- 优化发送策略，减少整包边界带来的主机侧等待。
- 如需更完整的 CDC ACM 行为，可继续补充类请求和状态通知处理。

## 9. 适用场景
该模块适合以下场景：
- 需要一个简单稳定的 USB 虚拟串口。
- 工程对内存分配和依赖控制要求严格。
- 需要直接掌控底层 USB 枚举与收发流程。

不太适合：
- 需要快速扩展到多 USB 类复合设备。
- 希望直接复用 ST 中间件生态的上层接口。


