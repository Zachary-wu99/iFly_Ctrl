/**
 * @file platform_handle.hpp
 * @brief 平台相关硬件句柄转换接口。
 */
#ifndef IFLY_PLATFORM_HANDLE_HPP
#define IFLY_PLATFORM_HANDLE_HPP

#if defined(STM32F405xx)
#include "can.h"
#include "gpio.h"
#include "tim.h"
#include "usb_otg.h"
#include "usart.h"
#else
#error "Unsupported MCU: add handle conversions for this platform."
#endif

namespace iFly {
namespace platform {

/**
 * @brief 将通用句柄转换为 CAN HAL 句柄。
 *
 * @param handle 通用硬件句柄。
 * @return 转换后的 `CAN_HandleTypeDef*`。
 */
inline CAN_HandleTypeDef *AsCanHandle(void *handle) {
  return static_cast<CAN_HandleTypeDef *>(handle);
}

/**
 * @brief 将只读通用句柄转换为只读 CAN HAL 句柄。
 *
 * @param handle 只读通用硬件句柄。
 * @return 转换后的 `const CAN_HandleTypeDef*`。
 */
inline const CAN_HandleTypeDef *AsCanHandle(const void *handle) {
  return static_cast<const CAN_HandleTypeDef *>(handle);
}

/**
 * @brief 将通用句柄转换为 UART HAL 句柄。
 *
 * @param handle 通用硬件句柄。
 * @return 转换后的 `UART_HandleTypeDef*`。
 */
inline UART_HandleTypeDef *AsUartHandle(void *handle) {
  return static_cast<UART_HandleTypeDef *>(handle);
}

/**
 * @brief 将只读通用句柄转换为只读 UART HAL 句柄。
 *
 * @param handle 只读通用硬件句柄。
 * @return 转换后的 `const UART_HandleTypeDef*`。
 */
inline const UART_HandleTypeDef *AsUartHandle(const void *handle) {
  return static_cast<const UART_HandleTypeDef *>(handle);
}

/**
 * @brief 将通用句柄转换为 TIM HAL 句柄。
 *
 * @param handle 通用硬件句柄。
 * @return 转换后的 `TIM_HandleTypeDef*`。
 */
inline TIM_HandleTypeDef *AsTimHandle(void *handle) {
  return static_cast<TIM_HandleTypeDef *>(handle);
}

/**
 * @brief 将只读通用句柄转换为只读 TIM HAL 句柄。
 *
 * @param handle 只读通用硬件句柄。
 * @return 转换后的 `const TIM_HandleTypeDef*`。
 */
inline const TIM_HandleTypeDef *AsTimHandle(const void *handle) {
  return static_cast<const TIM_HandleTypeDef *>(handle);
}

/**
 * @brief 将通用句柄转换为 GPIO 端口指针。
 *
 * @param handle 通用硬件句柄。
 * @return 转换后的 `GPIO_TypeDef*`。
 */
inline GPIO_TypeDef *AsGpioPort(void *handle) {
  return static_cast<GPIO_TypeDef *>(handle);
}

/**
 * @brief 将只读通用句柄转换为只读 GPIO 端口指针。
 *
 * @param handle 只读通用硬件句柄。
 * @return 转换后的 `const GPIO_TypeDef*`。
 */
inline const GPIO_TypeDef *AsGpioPort(const void *handle) {
  return static_cast<const GPIO_TypeDef *>(handle);
}

/**
 * @brief 将通用句柄转换为 USB PCD HAL 句柄。
 *
 * @param handle 通用硬件句柄。
 * @return 转换后的 `PCD_HandleTypeDef*`。
 */
inline PCD_HandleTypeDef *AsPcdHandle(void *handle) {
  return static_cast<PCD_HandleTypeDef *>(handle);
}

/**
 * @brief 将只读通用句柄转换为只读 USB PCD HAL 句柄。
 *
 * @param handle 只读通用硬件句柄。
 * @return 转换后的 `const PCD_HandleTypeDef*`。
 */
inline const PCD_HandleTypeDef *AsPcdHandle(const void *handle) {
  return static_cast<const PCD_HandleTypeDef *>(handle);
}

} // namespace platform
} // namespace iFly

#endif /* IFLY_PLATFORM_HANDLE_HPP */
