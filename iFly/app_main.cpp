#include "app_main.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "hardware_can.hpp"
#include "hardware_uart.hpp"
#include "main.h"

namespace {

constexpr uint32_t kLogUartRxQueueSize = 1024U;
constexpr uint32_t kCanRxQueueSize = 4096U;
constexpr uint32_t kIdleDelayMs = 1U;
constexpr uint32_t kLogWriteTimeoutMs = 2000U;
constexpr uint32_t kFrameWaitTimeoutMs = 500U;
constexpr uint32_t kStressProgressTimeoutMs = 2000U;
constexpr uint32_t kStressFrameCount = 20000U;
constexpr uint32_t kStressOutstandingLimit = 96U;

constexpr char kBanner[] =
    "\r\n[CAN1 LOOPBACK TEST READY]\r\n"
    "can_baud=1000000 uart5_baud=1000000 mode=single+stress\r\n";

struct UnitTestStats final {
  bool canReady = false;
  bool stdFramePassed = false;
  bool extFramePassed = false;
};

struct StressStats final {
  uint32_t targetFrames = 0U;
  uint32_t sentFrames = 0U;
  uint32_t receivedFrames = 0U;
  uint32_t verifiedFrames = 0U;
  uint32_t mismatchFrames = 0U;
  uint32_t writeBusyCount = 0U;
  uint32_t rxDroppedBytes = 0U;
  uint32_t maxTxUsedBytes = 0U;
  uint32_t maxRxUsedBytes = 0U;
  uint32_t maxOutstandingFrames = 0U;
  uint32_t durationMs = 0U;
  bool timedOut = false;
  bool completed = false;
  bool firstMismatchCaptured = false;
  uint32_t firstMismatchSeq = 0U;
  uint32_t firstMismatchExpectedId = 0U;
  uint32_t firstMismatchActualId = 0U;
  uint8_t firstMismatchExpectedDlc = 0U;
  uint8_t firstMismatchActualDlc = 0U;
  uint8_t firstMismatchExpectedFlags = 0U;
  uint8_t firstMismatchActualFlags = 0U;
  uint8_t firstMismatchExpectedData[8] {};
  uint8_t firstMismatchActualData[8] {};
};

iFly::HardwareUart g_uart5(iFly::UartPortId::kUart5, kLogUartRxQueueSize);
iFly::HardwareCan g_can1(iFly::CanPortId::kCan1, kCanRxQueueSize);

uint32_t ElapsedMs(uint32_t startTick) {
  return HAL_GetTick() - startTick;
}

uint32_t ScalePerSecondX10(uint64_t units, uint32_t durationMs) {
  if (durationMs == 0U) {
    return 0U;
  }

  return static_cast<uint32_t>((units * 10000ULL + (durationMs / 2U)) / durationMs);
}

uint32_t ScaleKiloBitsPerSecondX10(uint64_t bits, uint32_t durationMs) {
  if (durationMs == 0U) {
    return 0U;
  }

  return static_cast<uint32_t>((bits * 10ULL + (durationMs / 2U)) / durationMs);
}

bool WriteAll(const uint8_t *data, uint32_t len) {
  if ((data == nullptr) || (len == 0U)) {
    return true;
  }

  const uint32_t startTick = HAL_GetTick();
  uint32_t offset = 0U;
  while (offset < len) {
    const uint32_t written = g_uart5.Write(data + offset, len - offset);
    offset += written;
    if (offset >= len) {
      return true;
    }

    if (ElapsedMs(startTick) >= kLogWriteTimeoutMs) {
      return false;
    }

    HAL_Delay(kIdleDelayMs);
  }

  return true;
}

void WriteText(const char *text) {
  if (text == nullptr) {
    return;
  }

  (void)WriteAll(reinterpret_cast<const uint8_t *>(text),
                 static_cast<uint32_t>(strlen(text)));
}

void WriteLogf(const char *format, ...) {
  if (format == nullptr) {
    return;
  }

  char buffer[256] {};
  va_list args;
  va_start(args, format);
  const int written = vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  if (written <= 0) {
    return;
  }

  buffer[sizeof(buffer) - 1U] = '\0';
  WriteText(buffer);
}

void FillPacketForSequence(uint32_t seq,
                           bool extendedId,
                           iFly::CanFramePacket *packet) {
  if (packet == nullptr) {
    return;
  }

  *packet = iFly::CanFramePacket {};
  packet->id = extendedId ? (0x1ABCDE0U | (seq & 0xFU)) : (0x180U + (seq & 0x7FU));
  packet->dlc = 8U;
  packet->flags = extendedId ? iFly::kCanFrameFlagExtendedId : 0U;
  packet->data[0] = static_cast<uint8_t>(seq & 0xFFU);
  packet->data[1] = static_cast<uint8_t>((seq >> 8U) & 0xFFU);
  packet->data[2] = static_cast<uint8_t>((seq >> 16U) & 0xFFU);
  packet->data[3] = static_cast<uint8_t>((seq >> 24U) & 0xFFU);
  packet->data[4] = static_cast<uint8_t>(seq ^ 0x5AU);
  packet->data[5] = static_cast<uint8_t>((seq >> 3U) ^ 0xA5U);
  packet->data[6] = static_cast<uint8_t>(0xC0U | (seq & 0x3FU));
  packet->data[7] = static_cast<uint8_t>(0x3FU ^ (seq & 0x3FU));
}

bool PacketMatchesExpected(const iFly::CanFramePacket &actual,
                           const iFly::CanFramePacket &expected) {
  if (actual.id != expected.id) {
    return false;
  }

  if (actual.dlc != expected.dlc) {
    return false;
  }

  const uint8_t expectedFlags =
      static_cast<uint8_t>(expected.flags & (iFly::kCanFrameFlagExtendedId | iFly::kCanFrameFlagRemoteFrame));
  const uint8_t actualFlags =
      static_cast<uint8_t>(actual.flags & (iFly::kCanFrameFlagExtendedId | iFly::kCanFrameFlagRemoteFrame));
  if (actualFlags != expectedFlags) {
    return false;
  }

  return memcmp(actual.data, expected.data, sizeof(expected.data)) == 0;
}

void DrainPendingCanFrames() {
  iFly::CanFramePacket frame {};
  while (g_can1.ReadFrame(&frame)) {
  }
}

bool WaitForCanFrame(iFly::CanFramePacket *frame, uint32_t timeoutMs) {
  if (frame == nullptr) {
    return false;
  }

  const uint32_t startTick = HAL_GetTick();
  while (ElapsedMs(startTick) < timeoutMs) {
    if (g_can1.ReadFrame(frame)) {
      return true;
    }

    HAL_Delay(kIdleDelayMs);
  }

  return false;
}

bool RunSingleFrameCase(const char *name,
                        uint32_t seq,
                        bool extendedId) {
  iFly::CanFramePacket expected {};
  FillPacketForSequence(seq, extendedId, &expected);

  DrainPendingCanFrames();
  const bool sent = g_can1.WriteFrame(expected);
  iFly::CanFramePacket actual {};
  const bool received = sent && WaitForCanFrame(&actual, kFrameWaitTimeoutMs);
  const bool matched = received && PacketMatchesExpected(actual, expected);

  WriteLogf("[UNIT] %s sent=%s recv=%s match=%s tx_used=%lu rx_used=%lu dropped=%lu\r\n",
            name,
            sent ? "PASS" : "FAIL",
            received ? "PASS" : "FAIL",
            matched ? "PASS" : "FAIL",
            static_cast<unsigned long>(g_can1.TxUsed()),
            static_cast<unsigned long>(g_can1.RxUsed()),
            static_cast<unsigned long>(g_can1.RxDropped()));

  if (!matched) {
    WriteLogf(
        "[UNIT] %s expected:id=0x%08lX dlc=%u flags=0x%02X "
        "actual:id=0x%08lX dlc=%u flags=0x%02X\r\n",
        name,
        static_cast<unsigned long>(expected.id),
        static_cast<unsigned>(expected.dlc),
        static_cast<unsigned>(expected.flags),
        static_cast<unsigned long>(actual.id),
        static_cast<unsigned>(actual.dlc),
        static_cast<unsigned>(actual.flags));
  }

  return matched;
}

UnitTestStats RunUnitTests() {
  UnitTestStats stats {};
  stats.canReady = g_can1.IsConnected();
  WriteLogf("[UNIT] can_ready=%s\r\n", stats.canReady ? "PASS" : "FAIL");
  if (!stats.canReady) {
    return stats;
  }

  stats.stdFramePassed = RunSingleFrameCase("std_frame", 1U, false);
  stats.extFramePassed = RunSingleFrameCase("ext_frame", 2U, true);
  return stats;
}

void CaptureFirstMismatch(StressStats *stats,
                          uint32_t seq,
                          const iFly::CanFramePacket &expected,
                          const iFly::CanFramePacket &actual) {
  if ((stats == nullptr) || stats->firstMismatchCaptured) {
    return;
  }

  stats->firstMismatchCaptured = true;
  stats->firstMismatchSeq = seq;
  stats->firstMismatchExpectedId = expected.id;
  stats->firstMismatchActualId = actual.id;
  stats->firstMismatchExpectedDlc = expected.dlc;
  stats->firstMismatchActualDlc = actual.dlc;
  stats->firstMismatchExpectedFlags = expected.flags;
  stats->firstMismatchActualFlags = actual.flags;
  memcpy(stats->firstMismatchExpectedData, expected.data, sizeof(expected.data));
  memcpy(stats->firstMismatchActualData, actual.data, sizeof(actual.data));
}

StressStats RunStressTest(uint32_t frameCount) {
  StressStats stats {};
  stats.targetFrames = frameCount;

  DrainPendingCanFrames();
  const uint32_t droppedBase = g_can1.RxDropped();
  const uint32_t startTick = HAL_GetTick();
  uint32_t lastProgressTick = startTick;

  while (stats.receivedFrames < stats.targetFrames) {
    bool progressed = false;

    while (stats.sentFrames < stats.targetFrames) {
      const uint32_t outstanding = stats.sentFrames - stats.receivedFrames;
      if (outstanding >= kStressOutstandingLimit) {
        break;
      }

      iFly::CanFramePacket packet {};
      FillPacketForSequence(stats.sentFrames, false, &packet);
      if (!g_can1.WriteFrame(packet)) {
        ++stats.writeBusyCount;
        break;
      }

      ++stats.sentFrames;
      progressed = true;
      const uint32_t txUsed = g_can1.TxUsed();
      if (txUsed > stats.maxTxUsedBytes) {
        stats.maxTxUsedBytes = txUsed;
      }
      if (outstanding + 1U > stats.maxOutstandingFrames) {
        stats.maxOutstandingFrames = outstanding + 1U;
      }
    }

    iFly::CanFramePacket rxFrame {};
    while (g_can1.ReadFrame(&rxFrame)) {
      const uint32_t seq = stats.receivedFrames;
      iFly::CanFramePacket expected {};
      FillPacketForSequence(seq, false, &expected);

      ++stats.receivedFrames;
      if (PacketMatchesExpected(rxFrame, expected)) {
        ++stats.verifiedFrames;
      } else {
        ++stats.mismatchFrames;
        CaptureFirstMismatch(&stats, seq, expected, rxFrame);
      }

      progressed = true;
      const uint32_t rxUsed = g_can1.RxUsed();
      if (rxUsed > stats.maxRxUsedBytes) {
        stats.maxRxUsedBytes = rxUsed;
      }
    }

    if (stats.receivedFrames >= stats.targetFrames) {
      stats.completed = true;
      break;
    }

    if (progressed) {
      lastProgressTick = HAL_GetTick();
    } else if (ElapsedMs(lastProgressTick) >= kStressProgressTimeoutMs) {
      stats.timedOut = true;
      break;
    }

    HAL_Delay(kIdleDelayMs);
  }

  stats.durationMs = ElapsedMs(startTick);
  if (stats.durationMs == 0U) {
    stats.durationMs = 1U;
  }
  stats.rxDroppedBytes = g_can1.RxDropped() - droppedBase;
  return stats;
}

void PrintStressStats(const StressStats &stats) {
  const uint32_t frameRateX10 =
      ScalePerSecondX10(static_cast<uint64_t>(stats.verifiedFrames), stats.durationMs);
  const uint32_t payloadKbpsX10 =
      ScaleKiloBitsPerSecondX10(static_cast<uint64_t>(stats.verifiedFrames) * 64ULL, stats.durationMs);

  WriteLogf(
      "[STRESS] status=%s target=%lu sent=%lu recv=%lu verified=%lu mismatch=%lu "
      "dropped=%lu busy=%lu duration=%lu ms fps=%lu.%lu payload=%lu.%lu kbps "
      "max_tx=%lu max_rx=%lu max_outstanding=%lu\r\n",
      stats.completed && !stats.timedOut ? "PASS" : "FAIL",
      static_cast<unsigned long>(stats.targetFrames),
      static_cast<unsigned long>(stats.sentFrames),
      static_cast<unsigned long>(stats.receivedFrames),
      static_cast<unsigned long>(stats.verifiedFrames),
      static_cast<unsigned long>(stats.mismatchFrames),
      static_cast<unsigned long>(stats.rxDroppedBytes),
      static_cast<unsigned long>(stats.writeBusyCount),
      static_cast<unsigned long>(stats.durationMs),
      static_cast<unsigned long>(frameRateX10 / 10U),
      static_cast<unsigned long>(frameRateX10 % 10U),
      static_cast<unsigned long>(payloadKbpsX10 / 10U),
      static_cast<unsigned long>(payloadKbpsX10 % 10U),
      static_cast<unsigned long>(stats.maxTxUsedBytes),
      static_cast<unsigned long>(stats.maxRxUsedBytes),
      static_cast<unsigned long>(stats.maxOutstandingFrames));

  if (stats.firstMismatchCaptured) {
    WriteLogf(
        "[STRESS] first_mismatch seq=%lu expected:id=0x%08lX dlc=%u flags=0x%02X "
        "actual:id=0x%08lX dlc=%u flags=0x%02X\r\n",
        static_cast<unsigned long>(stats.firstMismatchSeq),
        static_cast<unsigned long>(stats.firstMismatchExpectedId),
        static_cast<unsigned>(stats.firstMismatchExpectedDlc),
        static_cast<unsigned>(stats.firstMismatchExpectedFlags),
        static_cast<unsigned long>(stats.firstMismatchActualId),
        static_cast<unsigned>(stats.firstMismatchActualDlc),
        static_cast<unsigned>(stats.firstMismatchActualFlags));
  }
}

void RunCanLoopbackTests() {
  WriteText(kBanner);

  const UnitTestStats unitStats = RunUnitTests();
  const bool unitPassed =
      unitStats.canReady && unitStats.stdFramePassed && unitStats.extFramePassed;
  WriteLogf("[UNIT] summary=%s\r\n", unitPassed ? "PASS" : "FAIL");

  if (!unitPassed) {
    return;
  }

  const StressStats stressStats = RunStressTest(kStressFrameCount);
  PrintStressStats(stressStats);
  WriteLogf("[RESULT] overall=%s\r\n",
            (stressStats.completed && !stressStats.timedOut && (stressStats.mismatchFrames == 0U) &&
             (stressStats.rxDroppedBytes == 0U))
                ? "PASS"
                : "FAIL");
}

} // namespace

extern "C" void app_main(void) {
  g_uart5.Init();
  g_can1.Init();
  HAL_Delay(20);

  RunCanLoopbackTests();

  while (1) {
    HAL_Delay(1000);
  }
}
