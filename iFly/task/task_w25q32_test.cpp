/**
 * @file task_w25q32_test.cpp
 * @brief W25Q32 自检任务。
 */

#include <stdint.h>

#include "task.hpp"
#include "w25q32.hpp"

namespace {

using W25q32TestFlash =
    iFly::W25q32<iFly::SpiPortId::kSpi1,
                 iFly::GpioPortId::kA,
                 iFly::GpioPinId::kPin4,
                 iFly::GpioPinState::kReset>;

/**
 * @brief W25Q32 自检执行阶段。
 */
enum class W25q32TestStep : uint8_t {
  kIdle = 0U, /**< 尚未开始。 */
  kInit, /**< 初始化驱动。 */
  kReadJedecId, /**< 读取 JEDEC ID。 */
  kProbe, /**< 校验芯片型号。 */
  kReadUniqueId, /**< 读取唯一 ID。 */
  kEraseSector, /**< 擦除测试扇区。 */
  kReadErased, /**< 读取擦除后的数据。 */
  kVerifyErased, /**< 校验擦除结果。 */
  kWritePattern, /**< 写入测试数据。 */
  kReadPattern, /**< 读取测试数据。 */
  kVerifyPattern, /**< 校验测试数据。 */
  kPassed, /**< 自检通过。 */
  kFailed /**< 自检失败。 */
};

/**
 * @brief W25Q32 自检结果快照。
 */
struct W25q32TestResult final {
  W25q32TestStep step = W25q32TestStep::kIdle; /**< 当前或失败阶段。 */
  bool completed = false; /**< 自检是否已结束。 */
  bool passed = false; /**< 自检是否通过。 */
  bool jedec_id_matched = false; /**< JEDEC ID 是否匹配 W25Q32。 */
  iFly::W25q32JedecId jedec_id {}; /**< 读取到的 JEDEC ID。 */
  uint8_t unique_id[W25q32TestFlash::kUniqueIdSize] {}; /**< 读取到的唯一 ID。 */
  uint32_t test_address = 0U; /**< 本次擦写测试起始地址。 */
  uint32_t failed_index = 0U; /**< 校验失败的偏移。 */
  uint8_t expected = 0U; /**< 校验失败时的期望值。 */
  uint8_t actual = 0U; /**< 校验失败时的实际值。 */
  iFly::W25q32Status flash_status = iFly::W25q32Status::kOk; /**< 驱动状态。 */
  iFly::SpiStatus spi_status = iFly::SpiStatus::kOk; /**< SPI 状态。 */
  uint32_t spi_error_code = 0U; /**< SPI 底层错误码。 */
};

constexpr uint32_t kW25q32TestStartDelayMs = 1000U;
constexpr uint32_t kW25q32TestLength = 256U;
constexpr uint32_t kW25q32TestSectorIndex = W25q32TestFlash::kSectorCount - 1U;
constexpr uint32_t kW25q32TestAddress =
    W25q32TestFlash::SectorAddress(kW25q32TestSectorIndex);

iFly::TaskHandle w25q32_test_handle = iFly::kInvalidTaskHandle;
W25q32TestFlash w25q32_flash {};
W25q32TestResult w25q32_test_result {};
uint8_t w25q32_test_tx[kW25q32TestLength] {};
uint8_t w25q32_test_rx[kW25q32TestLength] {};

void CaptureDriverStatus()
{
  w25q32_test_result.flash_status = w25q32_flash.LastStatus();
  w25q32_test_result.spi_status = w25q32_flash.LastSpiStatus();
  w25q32_test_result.spi_error_code = w25q32_flash.SpiErrorCode();
}

void FinishTest(W25q32TestStep step, bool passed)
{
  w25q32_test_result.step = passed ? W25q32TestStep::kPassed : step;
  w25q32_test_result.completed = true;
  w25q32_test_result.passed = passed;
  CaptureDriverStatus();
}

void FillTestPattern()
{
  for (uint32_t index = 0U; index < kW25q32TestLength; ++index) {
    w25q32_test_tx[index] =
        static_cast<uint8_t>((index * 31U) ^ 0x5AU ^ (index >> 1U));
    w25q32_test_rx[index] = 0U;
  }
}

bool VerifyErasedData()
{
  for (uint32_t index = 0U; index < kW25q32TestLength; ++index) {
    if (w25q32_test_rx[index] != 0xFFU) {
      w25q32_test_result.failed_index = index;
      w25q32_test_result.expected = 0xFFU;
      w25q32_test_result.actual = w25q32_test_rx[index];
      return false;
    }
  }

  return true;
}

bool VerifyPatternData()
{
  for (uint32_t index = 0U; index < kW25q32TestLength; ++index) {
    if (w25q32_test_rx[index] != w25q32_test_tx[index]) {
      w25q32_test_result.failed_index = index;
      w25q32_test_result.expected = w25q32_test_tx[index];
      w25q32_test_result.actual = w25q32_test_rx[index];
      return false;
    }
  }

  return true;
}

void W25q32TestTask(void *context)
{
  (void)context;

  w25q32_test_result = W25q32TestResult {};
  w25q32_test_result.test_address = kW25q32TestAddress;
  FillTestPattern();

  w25q32_test_result.step = W25q32TestStep::kInit;
  if (!w25q32_flash.Init()) {
    FinishTest(W25q32TestStep::kInit, false);
    return;
  }

  w25q32_test_result.step = W25q32TestStep::kReadJedecId;
  if (!w25q32_flash.ReadJedecId(&w25q32_test_result.jedec_id)) {
    FinishTest(W25q32TestStep::kReadJedecId, false);
    return;
  }

  w25q32_test_result.step = W25q32TestStep::kProbe;
  w25q32_test_result.jedec_id_matched =
      w25q32_test_result.jedec_id.IsW25q32();
  if (!w25q32_test_result.jedec_id_matched || !w25q32_flash.Probe()) {
    FinishTest(W25q32TestStep::kProbe, false);
    return;
  }

  w25q32_test_result.step = W25q32TestStep::kReadUniqueId;
  if (!w25q32_flash.ReadUniqueId(w25q32_test_result.unique_id)) {
    FinishTest(W25q32TestStep::kReadUniqueId, false);
    return;
  }

  // 自检会擦除外部 Flash 最末尾 4KB 扇区，避免覆盖业务数据请调整测试地址。
  w25q32_test_result.step = W25q32TestStep::kEraseSector;
  if (!w25q32_flash.EraseSectorByIndex(kW25q32TestSectorIndex)) {
    FinishTest(W25q32TestStep::kEraseSector, false);
    return;
  }

  w25q32_test_result.step = W25q32TestStep::kReadErased;
  if (!w25q32_flash.Read(kW25q32TestAddress,
                         w25q32_test_rx,
                         kW25q32TestLength)) {
    FinishTest(W25q32TestStep::kReadErased, false);
    return;
  }

  w25q32_test_result.step = W25q32TestStep::kVerifyErased;
  if (!VerifyErasedData()) {
    FinishTest(W25q32TestStep::kVerifyErased, false);
    return;
  }

  w25q32_test_result.step = W25q32TestStep::kWritePattern;
  if (!w25q32_flash.WriteAll(kW25q32TestAddress,
                             w25q32_test_tx,
                             kW25q32TestLength)) {
    FinishTest(W25q32TestStep::kWritePattern, false);
    return;
  }

  w25q32_test_result.step = W25q32TestStep::kReadPattern;
  if (!w25q32_flash.Read(kW25q32TestAddress,
                         w25q32_test_rx,
                         kW25q32TestLength)) {
    FinishTest(W25q32TestStep::kReadPattern, false);
    return;
  }

  w25q32_test_result.step = W25q32TestStep::kVerifyPattern;
  if (!VerifyPatternData()) {
    FinishTest(W25q32TestStep::kVerifyPattern, false);
    return;
  }

  FinishTest(W25q32TestStep::kPassed, true);
}

} // namespace

bool InitW25q32TestTask(void)
{
  w25q32_test_handle = iFly::TaskCreateOneShot(&W25q32TestTask,
                                               nullptr,
                                               kW25q32TestStartDelayMs,
                                               iFly::SoftTimerService::kLowestPriority,
                                               "w25q32_test");

  return w25q32_test_handle != iFly::kInvalidTaskHandle;
}
