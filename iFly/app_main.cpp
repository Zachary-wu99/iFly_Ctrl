#include "app_main.h"

#include <stdint.h>

#include "usb_uart.hpp"

namespace {

/*
 * 应用层 USB 接收临时缓冲区。
 *
 * 每次主循环调用 app_main() 时，会先尝试从 usb_uart 接口中读取一批数据到这里，
 * 然后再按示例逻辑进行处理。
 */
uint8_t g_usb_rx_temp[64] {};

/*
 * 给上层使用的 USB 虚拟串口对象。
 *
 * 该对象内部最终仍然依赖 UsbCdcAcm 和无锁队列，但对应用层来说只暴露串口风格接口。
 */
iFly::usb_uart g_usb_uart;


} // namespace

extern "C" void app_main(void)
{
  /*
   * 首次进入时完成一次性初始化。
   *
   * 此时 main.c 中已经先执行过：
   * - HAL_Init()
   * - SystemClock_Config()
   * - MX_GPIO_Init()
   * - MX_USB_OTG_FS_PCD_Init()
   *
   * 因此这里只需要初始化上层 usb_uart 对象即可。
   */
    g_usb_uart.Init();

  while(1)
  {
    /*
   * 示例逻辑：USB 回环测试。
   *
   * 处理流程如下：
   * 1. 从 usb_uart 接收队列中读取本轮已收到的数据；
   * 2. 若读取到数据，则原样写回发送队列；
   * 3. 主机串口工具即可看到回环数据，用于验证 USB CDC 链路是否正常。
   */
  const uint32_t rx_len = g_usb_uart.Read(g_usb_rx_temp, sizeof(g_usb_rx_temp));
  if (rx_len > 0U)
  {
    uint32_t sent = 0U;
    while (sent < rx_len)
    {
      const uint32_t pushed = g_usb_uart.Write(&g_usb_rx_temp[sent], rx_len - sent);
      if (pushed == 0U)
      {
        /*
         * 当前发送队列已满。
         *
         * 为保持示例简洁，这里先结束本轮发送，等待下一次 app_main() 再继续尝试。
         */
        break;
      }
      sent += pushed;
    }
  }
  }
}
