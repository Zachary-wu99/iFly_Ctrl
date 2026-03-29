#include "app_main.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "soft_timer.hpp"
#include "usb_uart.hpp"

iFly::UsbUart g_usb_uart;

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

  while (1) {
  

  }
}
