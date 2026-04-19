#include "app_main.h"

#include <stdint.h>

#include "flight_ctrl_cli.hpp"
#include "hardware_uart.hpp"
#include "main.h"
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
  HAL_Delay(20U);

  while (1) {
    g_flight_ctrl_cli.Poll();
    HAL_Delay(kMainLoopDelayMs);
  }
}
