#include "app_main.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "msg.hpp"
#include "soft_timer.hpp"
#include "usb_uart.hpp"

namespace {

using SoftTimer = iFly::SoftTimerService;

/*
 * 这是一个演示消息结构体。
 *
 * 设计原则：
 * - 只放简单字段，便于在裸机环境中复制和传递；
 * - 类型满足 msg.hpp 对“标准布局、可平凡拷贝”的要求；
 * - 每次定时器触发时，发布端就会填充一份这样的消息。
 */
struct DemoCounterMessage final {
  uint32_t counter = 0U;
  uint32_t publish_tick_ms = 0U;
  uint32_t publish_ok_count = 0U;
};

/*
 * 创建一个 Topic 类型。
 *
 * 模板参数说明：
 * - DemoCounterMessage：消息类型；
 * - 4U：最多允许 4 个订阅者；
 * - 8U：每个订阅者本地最多缓存 8 条消息。
 */
using DemoCounterTopic = iFly::MsgTopic<DemoCounterMessage, 4U, 8U>;

/*
 * 1. 创建 Topic 本体；
 * 2. 创建一个发布端 Publication；
 * 3. 创建两个订阅端 Subscription。
 *
 * 这样做是为了同时演示：
 * - Copy()：逐条消费消息；
 * - CopyLatest()：只关心最新一条消息。
 */
DemoCounterTopic g_counter_topic("demo_counter_topic");
DemoCounterTopic::Publication g_counter_pub(g_counter_topic);
DemoCounterTopic::Subscription g_stream_sub(g_counter_topic);
DemoCounterTopic::Subscription g_latest_sub(g_counter_topic);

/*
 * 为了让例子可以直接观察运行结果，这里继续沿用工程里的 USB CDC 串口对象。
 * 主机连接后，可在虚拟串口工具里直接看到示例输出。
 */
iFly::usb_uart g_usb_uart;

/*
 * 下面这些句柄用于保存创建出来的定时器任务。
 * 这样后续既可以判断创建是否成功，也可以演示 DeleteTask() 的用法。
 */
SoftTimer::TaskHandle g_publish_task = SoftTimer::kInvalidTaskHandle;
SoftTimer::TaskHandle g_stream_consume_task = SoftTimer::kInvalidTaskHandle;
SoftTimer::TaskHandle g_latest_watch_task = SoftTimer::kInvalidTaskHandle;
SoftTimer::TaskHandle g_one_shot_task = SoftTimer::kInvalidTaskHandle;
SoftTimer::TaskHandle g_delete_demo_task = SoftTimer::kInvalidTaskHandle;

/*
 * 运行时状态变量。
 */
bool g_demo_started = false;
bool g_banner_sent = false;
uint32_t g_publish_counter = 0U;
uint32_t g_publish_ok_count = 0U;
uint32_t g_publish_fail_count = 0U;
char g_log_buffer[192] {};

/*
 * 通过 USB CDC 输出一段文本。
 *
 * 这里采用“尽力发送”策略：
 * - 如果主机还没枚举完成，就先不发；
 * - 如果当前发送队列满了，就保留下一轮再继续输出。
 */
void UsbWriteText(const char *text) noexcept
{
  if ((text == nullptr) || (!g_usb_uart.IsConnected())) {
    return;
  }

  const uint8_t *data = reinterpret_cast<const uint8_t *>(text);
  uint32_t remaining = static_cast<uint32_t>(strlen(text));
  while (remaining > 0U) {
    const uint32_t pushed = g_usb_uart.Write(data, remaining);
    if (pushed == 0U) {
      break;
    }

    data += pushed;
    remaining -= pushed;
  }
}

/*
 * 简单的格式化日志输出工具。
 *
 * 这样业务示例里就不需要反复手写 snprintf + Write 组合了。
 */
void UsbLog(const char *format, ...) noexcept
{
  va_list args;
  va_start(args, format);
  const int length = vsnprintf(g_log_buffer, sizeof(g_log_buffer), format, args);
  va_end(args);

  if (length <= 0) {
    return;
  }

  g_log_buffer[sizeof(g_log_buffer) - 1U] = '\0';
  UsbWriteText(g_log_buffer);
}

/*
 * 发布定时器回调。
 *
 * 每 1000ms 执行一次，演示“定时器 -> 发布消息”的最基本用法。
 */
void PublishCounterMessage(void *context) noexcept
{
  (void)context;

  DemoCounterMessage message {};
  message.counter = ++g_publish_counter;
  message.publish_tick_ms = SoftTimer::Instance().Now();
  message.publish_ok_count = g_publish_ok_count + 1U;

  if (g_counter_pub.Publish(message)) {
    ++g_publish_ok_count;
    UsbLog("[publish] ok, generation=%lu, counter=%lu, tick=%lu\r\n",
           static_cast<unsigned long>(g_counter_pub.Generation()),
           static_cast<unsigned long>(message.counter),
           static_cast<unsigned long>(message.publish_tick_ms));
  } else {
    ++g_publish_fail_count;
    UsbLog("[publish] failed, fail_count=%lu\r\n",
           static_cast<unsigned long>(g_publish_fail_count));
  }
}

/*
 * 逐条消费订阅者回调。
 *
 * 每 200ms 执行一次，演示：
 * - 如何先用 Updated() 判断有没有新消息；
 * - 如何用 Copy() 一条一条取出队列中的消息；
 * - 如何查看 PendingCount() / LostCount()。
 */
void ConsumeStreamMessages(void *context) noexcept
{
  (void)context;

  if (!g_stream_sub.Updated()) {
    return;
  }

  DemoCounterMessage message {};
  uint32_t generation = 0U;
  while (g_stream_sub.Copy(message, &generation)) {
    UsbLog("[copy] generation=%lu, counter=%lu, tick=%lu, pending=%lu, lost=%lu\r\n",
           static_cast<unsigned long>(generation),
           static_cast<unsigned long>(message.counter),
           static_cast<unsigned long>(message.publish_tick_ms),
           static_cast<unsigned long>(g_stream_sub.PendingCount()),
           static_cast<unsigned long>(g_stream_sub.LostCount()));
  }
}

/*
 * 只取最新值的订阅者回调。
 *
 * 每 1500ms 执行一次，演示：
 * - CopyLatest() 会丢弃更旧的未处理消息；
 * - 这种模式更适合“只关心当前最新状态”的场景。
 */
void WatchLatestMessage(void *context) noexcept
{
  (void)context;

  DemoCounterMessage latest {};
  uint32_t generation = 0U;
  if (!g_latest_sub.CopyLatest(latest, &generation)) {
    return;
  }

  UsbLog("[latest] generation=%lu, counter=%lu, publish_ok=%lu, pending=%lu, lost=%lu\r\n",
         static_cast<unsigned long>(generation),
         static_cast<unsigned long>(latest.counter),
         static_cast<unsigned long>(latest.publish_ok_count),
         static_cast<unsigned long>(g_latest_sub.PendingCount()),
         static_cast<unsigned long>(g_latest_sub.LostCount()));
}

/*
 * 一次性定时器回调。
 *
 * 通过 auto_reload = false 创建，触发一次后会自动回收。
 */
void OneShotHello(void *context) noexcept
{
  (void)context;
  UsbLog("[one-shot] 这是一次性定时器，只会执行一次，当前 tick=%lu\r\n",
         static_cast<unsigned long>(SoftTimer::Instance().Now()));
}

/*
 * 删除定时器示例回调。
 *
 * 这里演示如何删除另一个已经创建好的周期任务。
 */
void DeleteTaskExample(void *context) noexcept
{
  (void)context;

  const bool deleted = SoftTimer::Instance().DeleteTask(g_latest_watch_task);
  if (deleted) {
    g_latest_watch_task = SoftTimer::kInvalidTaskHandle;
  }

  UsbLog("[delete-task] latest 观察任务删除结果=%s\r\n", deleted ? "success" : "failed");
}

/*
 * 统一封装一个“创建定时器任务”的小工具，便于示例代码更整洁。
 */
bool CreateTimerTask(SoftTimer::TaskHandle &handle,
                     SoftTimer::TaskCallback callback,
                     void *context,
                     uint32_t intervalMs,
                     uint32_t startDelayMs,
                     uint8_t priority,
                     bool autoReload) noexcept
{
  SoftTimer::TaskConfig config {};
  config.callback = callback;
  config.context = context;
  config.interval_ms = intervalMs;
  config.start_delay_ms = startDelayMs;
  config.priority = priority;
  config.auto_reload = autoReload;

  handle = SoftTimer::Instance().CreateTask(config);
  return SoftTimer::IsValidTaskHandle(handle);
}

/*
 * 初始化本例所需的 Topic 和定时器。
 *
 * 初始化顺序建议如下：
 * 1. 先建立订阅关系；
 * 2. 再创建定时器；
 * 3. 最后在主循环里反复调用 Dispatch()。
 */
void StartDemoIfNeeded() noexcept
{
  if (g_demo_started) {
    return;
  }

  // (void)g_stream_sub.Subscribe(g_counter_topic);
  // (void)g_latest_sub.Subscribe(g_counter_topic);

  bool ok = true;
  ok = CreateTimerTask(
           g_publish_task, PublishCounterMessage, nullptr, 1000U, 1000U, 5U, true) &&
       ok;
  ok = CreateTimerTask(
           g_stream_consume_task, ConsumeStreamMessages, nullptr, 200U, 200U, 20U, true) &&
       ok;
  ok = CreateTimerTask(
           g_latest_watch_task, WatchLatestMessage, nullptr, 1500U, 1500U, 30U, true) &&
       ok;
  ok = CreateTimerTask(
           g_one_shot_task, OneShotHello, nullptr, 1000U, 3000U, 10U, false) &&
       ok;
  ok = CreateTimerTask(
           g_delete_demo_task, DeleteTaskExample, nullptr, 1000U, 12000U, 15U, false) &&
       ok;

  g_demo_started = ok;
}

/*
 * 只在 USB 连接成功后输出一次欢迎信息。
 */
void PrintBannerIfNeeded() noexcept
{
  if (g_banner_sent || !g_usb_uart.IsConnected()) {
    return;
  }

  g_banner_sent = true;
  UsbWriteText("\r\n================ msg + soft_timer 示例 ================\r\n");
  UsbWriteText("1. PublishCounterMessage(): 每 1 秒发布一条消息\r\n");
  UsbWriteText("2. ConsumeStreamMessages(): 演示 Updated() + Copy()\r\n");
  UsbWriteText("3. WatchLatestMessage(): 演示 CopyLatest()\r\n");
  UsbWriteText("4. OneShotHello(): 演示一次性定时器\r\n");
  UsbWriteText("5. DeleteTaskExample(): 演示 DeleteTask()\r\n");
  UsbWriteText("=======================================================\r\n");
}

} // namespace

extern "C" void app_main(void)
{
  /*
   * 先初始化 USB 串口，方便后续把示例运行结果打印出来。
   */
  g_usb_uart.Init();

  /*
   * 启动一次示例。
   *
   * app_main() 在当前工程里会一直停留在自己的 while(1) 中，
   * 因此这里初始化一次即可。
   */
  StartDemoIfNeeded();

  while (1) {
    PrintBannerIfNeeded();

    /*
     * 软定时器的关键用法：
     * 主循环中需要持续调用 Dispatch()，
     * 到期任务才会在这里被真正执行。
     */
    (void)SoftTimer::Instance().Dispatch();
  }
}
