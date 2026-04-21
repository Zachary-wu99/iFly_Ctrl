#ifndef IFLY_PLATFORM_HANDLE_HPP
#define IFLY_PLATFORM_HANDLE_HPP

// lib 层统一使用 void* 保存底层硬件句柄。
// 具体平台到 HAL 句柄类型的转换集中放在这里，后续移植到其他 MCU 时，
// 只需要按新的芯片头文件和句柄类型扩展本文件即可。

#if defined(STM32F405xx)
#include "can.h"
#include "tim.h"
#include "usb_otg.h"
#include "usart.h"
#else
#error "Unsupported MCU: add handle conversions for this platform."
#endif

namespace iFly {
namespace platform {

inline CAN_HandleTypeDef *AsCanHandle(void *handle) {
  return static_cast<CAN_HandleTypeDef *>(handle);
}

inline const CAN_HandleTypeDef *AsCanHandle(const void *handle) {
  return static_cast<const CAN_HandleTypeDef *>(handle);
}

inline UART_HandleTypeDef *AsUartHandle(void *handle) {
  return static_cast<UART_HandleTypeDef *>(handle);
}

inline const UART_HandleTypeDef *AsUartHandle(const void *handle) {
  return static_cast<const UART_HandleTypeDef *>(handle);
}

inline TIM_HandleTypeDef *AsTimHandle(void *handle) {
  return static_cast<TIM_HandleTypeDef *>(handle);
}

inline const TIM_HandleTypeDef *AsTimHandle(const void *handle) {
  return static_cast<const TIM_HandleTypeDef *>(handle);
}

inline PCD_HandleTypeDef *AsPcdHandle(void *handle) {
  return static_cast<PCD_HandleTypeDef *>(handle);
}

inline const PCD_HandleTypeDef *AsPcdHandle(const void *handle) {
  return static_cast<const PCD_HandleTypeDef *>(handle);
}

} // namespace platform
} // namespace iFly

#endif /* IFLY_PLATFORM_HANDLE_HPP */
