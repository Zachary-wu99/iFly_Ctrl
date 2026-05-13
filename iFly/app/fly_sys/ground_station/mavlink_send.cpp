#include "mavlink_send.hpp"

namespace iFly {

MavlinkSend::MavlinkSend(MavlinkLink *link)
    : link_(link)
{
}

void MavlinkSend::BindLink(MavlinkLink *link)
{
  link_ = link;
}

MavlinkSend::StreamHandle MavlinkSend::RegisterStream(
    const StreamConfig &config)
{
  if ((config.callback == nullptr) || (config.interval_ms == 0U)) {
    return kInvalidStreamHandle;
  }

  const uint8_t slot_index = FindFreeStreamSlot();
  if (slot_index >= kMaxStreams) {
    return kInvalidStreamHandle;
  }

  StreamSlot &slot = streams_[slot_index];
  uint32_t generation = slot.generation + 1U;
  if (generation == 0U) {
    generation = 1U;
  }

  slot.name = config.name;
  slot.callback = config.callback;
  slot.handle = MakeStreamHandle(slot_index, generation);
  slot.interval_ms = config.interval_ms;
  slot.start_delay_ms =
      ResolveStartDelay(config.interval_ms, config.start_delay_ms);
  slot.next_send_ms = 0U;
  slot.generation = generation;
  slot.slot_index = slot_index;
  slot.allocated = true;
  slot.enabled = config.enabled;
  slot.scheduled = false;

  return slot.handle;
}

MavlinkSend::StreamHandle MavlinkSend::RegisterStream(
    const char *name,
    StreamCallback callback,
    uint32_t interval_ms,
    uint32_t start_delay_ms)
{
  StreamConfig config {};
  config.name = name;
  config.callback = callback;
  config.interval_ms = interval_ms;
  config.start_delay_ms = start_delay_ms;
  config.enabled = true;
  return RegisterStream(config);
}

bool MavlinkSend::UnregisterStream(StreamHandle handle)
{
  const int16_t slot_index = FindStreamIndex(handle);
  if (slot_index < 0) {
    return false;
  }

  ClearStreamSlot(static_cast<uint8_t>(slot_index));
  return true;
}

bool MavlinkSend::SetStreamInterval(StreamHandle handle, uint32_t interval_ms)
{
  if (interval_ms == 0U) {
    return false;
  }

  const int16_t slot_index = FindStreamIndex(handle);
  if (slot_index < 0) {
    return false;
  }

  streams_[static_cast<uint8_t>(slot_index)].interval_ms = interval_ms;
  return true;
}

bool MavlinkSend::EnableStream(StreamHandle handle,
                               bool enabled,
                               uint32_t start_delay_ms)
{
  const int16_t slot_index = FindStreamIndex(handle);
  if (slot_index < 0) {
    return false;
  }

  StreamSlot &slot = streams_[static_cast<uint8_t>(slot_index)];
  slot.enabled = enabled;
  if (enabled) {
    slot.start_delay_ms = ResolveStartDelay(slot.interval_ms, start_delay_ms);
    slot.scheduled = false;
  }

  return true;
}

bool MavlinkSend::IsStreamEnabled(StreamHandle handle) const
{
  const int16_t slot_index = FindStreamIndex(handle);
  if (slot_index < 0) {
    return false;
  }

  return streams_[static_cast<uint8_t>(slot_index)].enabled;
}

void MavlinkSend::Update(uint32_t now_ms)
{
  for (uint8_t slot_index = 0U; slot_index < kMaxStreams; ++slot_index) {
    StreamSlot &slot = streams_[slot_index];
    if (!slot.allocated || !slot.enabled || (slot.callback == nullptr)) {
      continue;
    }

    if (!slot.scheduled) {
      ScheduleStream(slot, now_ms);
    }

    if (static_cast<int32_t>(now_ms - slot.next_send_ms) < 0) {
      continue;
    }

    const StreamHandle handle = slot.handle;
    (this->*slot.callback)();

    if (!IsStreamHandleMatched(slot, handle, slot_index)) {
      continue;
    }

    if (slot.enabled) {
      slot.next_send_ms = now_ms + slot.interval_ms;
      slot.scheduled = true;
    }
  }
}

void MavlinkSend::ResetStreams(uint32_t now_ms)
{
  for (uint8_t slot_index = 0U; slot_index < kMaxStreams; ++slot_index) {
    StreamSlot &slot = streams_[slot_index];
    if (!slot.allocated) {
      continue;
    }

    ScheduleStream(slot, now_ms);
  }
}

uint8_t MavlinkSend::FindFreeStreamSlot() const
{
  for (uint8_t slot_index = 0U; slot_index < kMaxStreams; ++slot_index) {
    if (!streams_[slot_index].allocated) {
      return slot_index;
    }
  }

  return kMaxStreams;
}

int16_t MavlinkSend::FindStreamIndex(StreamHandle handle) const
{
  if (handle == kInvalidStreamHandle) {
    return -1;
  }

  const uint8_t slot_index = ExtractStreamIndex(handle);
  if (slot_index >= kMaxStreams) {
    return -1;
  }

  return IsStreamHandleMatched(streams_[slot_index], handle, slot_index)
             ? static_cast<int16_t>(slot_index)
             : -1;
}

bool MavlinkSend::IsStreamHandleMatched(const StreamSlot &slot,
                                        StreamHandle handle,
                                        uint8_t slot_index) const
{
  return slot.allocated && (slot.slot_index == slot_index) &&
         (slot.generation == ExtractStreamGeneration(handle));
}

void MavlinkSend::ClearStreamSlot(uint8_t slot_index)
{
  StreamSlot &slot = streams_[slot_index];
  slot.name = nullptr;
  slot.callback = nullptr;
  slot.handle = kInvalidStreamHandle;
  slot.interval_ms = 0U;
  slot.start_delay_ms = 0U;
  slot.next_send_ms = 0U;
  slot.slot_index = kMaxStreams;
  slot.allocated = false;
  slot.enabled = false;
  slot.scheduled = false;
}

void MavlinkSend::ScheduleStream(StreamSlot &slot, uint32_t now_ms)
{
  slot.next_send_ms = now_ms + slot.start_delay_ms;
  slot.scheduled = true;
}

void MavlinkSend::SendMessage(const mavlink_message_t &msg)
{
  if (link_ == nullptr) {
    return;
  }

  link_->SendMessage(msg);
}

MavlinkSend::StreamHandle MavlinkSend::MakeStreamHandle(uint8_t slot_index,
                                                        uint32_t generation)
{
  return (generation << 16U) | static_cast<StreamHandle>(slot_index + 1U);
}

uint8_t MavlinkSend::ExtractStreamIndex(StreamHandle handle)
{
  return static_cast<uint8_t>((handle & 0xFFFFU) - 1U);
}

uint32_t MavlinkSend::ExtractStreamGeneration(StreamHandle handle)
{
  return handle >> 16U;
}

uint32_t MavlinkSend::ResolveStartDelay(uint32_t interval_ms,
                                        uint32_t start_delay_ms)
{
  return (start_delay_ms == kUseIntervalAsStartDelay) ? interval_ms
                                                      : start_delay_ms;
}

} // namespace iFly
