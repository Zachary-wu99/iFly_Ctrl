#include "mavlink_cli.hpp"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "common/mavlink.h"
#include "tick.hpp"

namespace iFly {

namespace {

bool ParseFloat(const char *text, float *value)
{
  if ((text == nullptr) || (value == nullptr)) {
    return false;
  }

  char *end = nullptr;
  const float parsed = strtof(text, &end);
  if ((end == text) || (end == nullptr)) {
    return false;
  }

  while ((*end == ' ') || (*end == '\t')) {
    ++end;
  }

  if ((*end != '\0') || (isfinite(parsed) == 0)) {
    return false;
  }

  *value = parsed;
  return true;
}

bool WriteIntegerValue(char *buffer, uint32_t bufferSize, long value)
{
  const int written = snprintf(buffer, bufferSize, "%ld", value);
  return (written > 0) && (static_cast<uint32_t>(written) < bufferSize);
}

bool WriteUnsignedValue(char *buffer, uint32_t bufferSize, unsigned long value)
{
  const int written = snprintf(buffer, bufferSize, "%lu", value);
  return (written > 0) && (static_cast<uint32_t>(written) < bufferSize);
}

bool WriteFloatValue(char *buffer, uint32_t bufferSize, float value)
{
  if ((buffer == nullptr) || (bufferSize == 0U)) {
    return false;
  }

  if (isfinite(value) == 0) {
    const int written = snprintf(buffer, bufferSize, "nan");
    return (written > 0) && (static_cast<uint32_t>(written) < bufferSize);
  }

  bool negative = value < 0.0f;
  float absolute = negative ? -value : value;
  unsigned long integer_part = static_cast<unsigned long>(absolute);
  unsigned long fraction_part =
      static_cast<unsigned long>((absolute - static_cast<float>(integer_part)) *
                                 1000000.0f + 0.5f);

  if (fraction_part >= 1000000UL) {
    ++integer_part;
    fraction_part -= 1000000UL;
  }

  char fraction[7] {};
  int fraction_written = snprintf(fraction, sizeof(fraction), "%06lu",
                                  fraction_part);
  if ((fraction_written <= 0) ||
      (static_cast<uint32_t>(fraction_written) >= sizeof(fraction))) {
    return false;
  }

  uint8_t fraction_length = 6U;
  while ((fraction_length > 0U) && (fraction[fraction_length - 1U] == '0')) {
    --fraction_length;
  }
  fraction[fraction_length] = '\0';

  int written = 0;
  if (fraction_length == 0U) {
    written = snprintf(buffer, bufferSize, "%s%lu",
                       negative ? "-" : "", integer_part);
  } else {
    written = snprintf(buffer, bufferSize, "%s%lu.%s",
                       negative ? "-" : "", integer_part, fraction);
  }
  return (written > 0) && (static_cast<uint32_t>(written) < bufferSize);
}

bool WriteMavlinkValue(const MavlinkParameterValue &value,
                       char *buffer,
                       uint32_t bufferSize)
{
  switch (value.type) {
    case MAV_PARAM_TYPE_UINT8:
    case MAV_PARAM_TYPE_UINT16:
    case MAV_PARAM_TYPE_UINT32:
      return WriteUnsignedValue(buffer, bufferSize,
                                static_cast<unsigned long>(value.value));

    case MAV_PARAM_TYPE_INT32:
      return WriteIntegerValue(buffer, bufferSize,
                               static_cast<long>(value.value));

    case MAV_PARAM_TYPE_REAL32:
      return WriteFloatValue(buffer, bufferSize, value.value);

    default:
      return WriteFloatValue(buffer, bufferSize, value.value);
  }
}

} // namespace

void MavlinkCliService::RegisterParameters()
{
  const uint16_t count = parameter_service_.Count();
  uint8_t index = 0U;
  for (uint16_t parameter_index = 0U;
       (parameter_index < count) && (index < kManagedParameterCount);
       ++parameter_index) {
    MavlinkParameterValue parameter {};
    if (!parameter_service_.ReadByIndex(parameter_index, &parameter) ||
        (parameter.name == nullptr)) {
      continue;
    }

    ManagedParameterContext &context = managed_parameter_contexts_[index];
    context = ManagedParameterContext {};
    context.owner = this;
    context.mavlink_index = parameter_index;

    (void)shell_.RegisterParameter(
        {parameter.name,
         nullptr,
         &MavlinkCliService::GetManagedParameter,
         &MavlinkCliService::SetManagedParameter,
         &context});
    ++index;
  }

  (void)shell_.RegisterParameter(
      {"sys.transport", "active CLI transport",
       &MavlinkCliService::GetTransportParameter, nullptr, this});
  (void)shell_.RegisterParameter(
      {"sys.uptime_ms", "system uptime in milliseconds",
       &MavlinkCliService::GetUptimeParameter, nullptr, this});
}

bool MavlinkCliService::GetTransportParameter(void *context, char *buffer,
                                              uint32_t bufferSize)
{
  MavlinkCliService *cli = reinterpret_cast<MavlinkCliService *>(context);
  if ((cli == nullptr) || (buffer == nullptr) || (bufferSize == 0U)) {
    return false;
  }

  const int written =
      snprintf(buffer, bufferSize, "%s",
               (cli->active_transport_name_ != nullptr) ? cli->active_transport_name_
                                                        : "unbound");
  return (written > 0) &&
         (static_cast<uint32_t>(written) < bufferSize);
}

bool MavlinkCliService::GetUptimeParameter(void *context, char *buffer,
                                           uint32_t bufferSize)
{
  (void)context;

  if ((buffer == nullptr) || (bufferSize == 0U)) {
    return false;
  }

  const int written = snprintf(buffer, bufferSize, "%lu",
                               static_cast<unsigned long>(tick::NowMs()));
  return (written > 0) && (static_cast<uint32_t>(written) < bufferSize);
}

bool MavlinkCliService::GetManagedParameter(void *context, char *buffer,
                                            uint32_t bufferSize)
{
  ManagedParameterContext *parameter =
      reinterpret_cast<ManagedParameterContext *>(context);
  if ((parameter == nullptr) || (parameter->owner == nullptr) ||
      (buffer == nullptr) || (bufferSize == 0U)) {
    return false;
  }

  MavlinkParameterValue value {};
  if (!parameter->owner->parameter_service_.ReadByIndex(parameter->mavlink_index,
                                                        &value)) {
    return false;
  }

  return WriteMavlinkValue(value, buffer, bufferSize);
}

bool MavlinkCliService::SetManagedParameter(void *context, const char *value)
{
  ManagedParameterContext *parameter =
      reinterpret_cast<ManagedParameterContext *>(context);
  if ((parameter == nullptr) || (parameter->owner == nullptr) ||
      (value == nullptr)) {
    return false;
  }

  float parsed = 0.0f;
  MavlinkParameterValue current {};
  if (!ParseFloat(value, &parsed) ||
      !parameter->owner->parameter_service_.ReadByIndex(parameter->mavlink_index,
                                                        &current)) {
    return false;
  }

  if (!parameter->owner->parameter_service_.WriteValue(current.name,
                                                       parsed,
                                                       current.type)) {
    return false;
  }

  return true;
}

} // namespace iFly
