// 应用层主循环实现。
// 负责初始化 CLI 运行时，并周期轮询上层业务。
#include "app_main.h"

#include <stdint.h>

#include "flight_ctrl_cli.hpp"
#include "hardware_uart.hpp"
#include "main.h"
#include "tick.hpp"
#include "usb_uart.hpp"

namespace {

constexpr uint32_t kCliRxQueueSize = 1024U;
constexpr uint32_t kMainLoopDelayMs = 1U;
constexpr uint32_t kCliPollPeriodMs = 50U;
constexpr char kDefaultCliTransport[] = "usb";

iFly::HardwareUart g_uart5(iFly::UartPortId::kUart5, kCliRxQueueSize);
iFly::UsbUart g_usb_cli(kCliRxQueueSize);
iFly::FlightCtrlCli g_flight_ctrl_cli;

// 初始化 CLI 运行时依赖并绑定默认传输通道。
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

// 作为应用主入口，初始化后持续轮询 CLI。
extern "C" void app_main(void)
{
  InitCliRuntime();
  iFly::tick::DelayMs(20U);

  iFly::tick::NonBlockingDelayMs cli_poll_delay {};
  g_flight_ctrl_cli.Poll();
  cli_poll_delay.Start(kCliPollPeriodMs);

  while (1) {
    if (cli_poll_delay.ConsumeIfExpired()) {
      g_flight_ctrl_cli.Poll();
      cli_poll_delay.Start(kCliPollPeriodMs);
    }

    iFly::tick::DelayMs(kMainLoopDelayMs);
  }
}
