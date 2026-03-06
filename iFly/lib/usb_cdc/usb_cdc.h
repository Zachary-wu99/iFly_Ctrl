#ifndef IFLY_USB_CDC_H
#define IFLY_USB_CDC_H

#include <stdint.h>
#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 USB CDC 设备协议层。
 * @param hpcd 已完成底层初始化的 PCD 句柄，通常传入 hpcd_USB_OTG_FS。
 *
 * @note 调用顺序要求：
 * 1. 先完成 HAL/PCD 初始化；
 * 2. 再调用本函数接管 USB 设备枚举和数据收发。
 */
void IFly_USBCDC_Init(PCD_HandleTypeDef *hpcd);

/**
 * @brief 向 CDC 发送队列写入数据。
 * @param data 待发送数据指针。
 * @param len  待发送字节数。
 * @return 实际成功写入发送双缓冲的字节数。
 *
 * @note 若返回值小于 len，表示当前双缓冲已满，调用方应稍后继续发送。
 */
uint32_t IFly_USBCDC_Write(const uint8_t *data, uint32_t len);

/**
 * @brief 从 CDC 接收环形缓冲区读取数据。
 * @param data 接收数据输出缓冲区。
 * @param len  希望读取的最大字节数。
 * @return 实际读取到的字节数。
 */
uint32_t IFly_USBCDC_Read(uint8_t *data, uint32_t len);

/**
 * @brief 查询当前接收环形缓冲区中尚未读取的字节数。
 * @return 可读取字节数。
 */
uint32_t IFly_USBCDC_Available(void);

/**
 * @brief 查询 USB 是否已经被主机完成配置。
 * @return 1 表示已配置完成，0 表示尚未配置。
 */
uint8_t IFly_USBCDC_IsConfigured(void);

#ifdef __cplusplus
}
#endif

#endif /* IFLY_USB_CDC_H */
