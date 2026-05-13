#include "mavlink_parameter_service.hpp"

#include <string.h>

#include "common/mavlink.h"
#include "parameter_manager.hpp"

namespace iFly {

namespace {

struct MavlinkParameterBinding final {
  const char *mavlink_name = nullptr; /**< MAVLink 参数名。 */
  const char *project_name = nullptr; /**< 系统参数中心参数名。 */
};

const MavlinkParameterBinding kMavlinkParameterBindings[] = {
    {"MAV_SYS_ID", "MAV_SYS_ID"},
    {"MAV_COMP_ID", "MAV_COMP_ID"},
    {"MAV_TYPE", "MAV_TYPE"},
    {"MAV_AUTOPILOT", "MAV_AUTOPILOT"},
    {"BAT1_N_CELLS", "BAT1_N_CELLS"},
    {"BAT1_V_EMPTY", "BAT1_V_EMPTY"},
    {"BAT1_V_CHARGED", "BAT1_V_CHARGED"},
    {"BAT1_CAPACITY", "BAT1_CAPACITY"},
    {"RC_MAP_ROLL", "RC_MAP_ROLL"},
    {"RC_MAP_PITCH", "RC_MAP_PITCH"},
    {"RC_MAP_THROTTLE", "RC_MAP_THROTTLE"},
    {"RC_MAP_YAW", "RC_MAP_YAW"},
    {"MOT_PWM_MIN", "MOT_PWM_MIN"},
    {"MOT_PWM_IDLE", "MOT_PWM_IDLE"},
    {"MOT_PWM_MAX", "MOT_PWM_MAX"},
    {"SPD_PID_P", "SPD_PID_P"},
    {"SPD_PID_I", "SPD_PID_I"},
    {"SPD_PID_D", "SPD_PID_D"},
    {"SPD_PID_FF", "SPD_PID_FF"},
    {"SPD_PID_IMIN", "SPD_PID_IMIN"},
    {"SPD_PID_IMAX", "SPD_PID_IMAX"},
    {"SPD_PID_OMIN", "SPD_PID_OMIN"},
    {"SPD_PID_OMAX", "SPD_PID_OMAX"},
    {"SPD_PID_FLTD", "SPD_PID_FLTD"},
    {"SPD_PID_DTMIN", "SPD_PID_DTMIN"},
    {"SPD_PID_DTMAX", "SPD_PID_DTMAX"},
    {"SPD_PID_DMODE", "SPD_PID_DMODE"},
    {"ANG_PID_P", "ANG_PID_P"},
    {"ANG_PID_I", "ANG_PID_I"},
    {"ANG_PID_D", "ANG_PID_D"},
    {"ANG_PID_FF", "ANG_PID_FF"},
    {"ANG_PID_IMIN", "ANG_PID_IMIN"},
    {"ANG_PID_IMAX", "ANG_PID_IMAX"},
    {"ANG_PID_OMIN", "ANG_PID_OMIN"},
    {"ANG_PID_OMAX", "ANG_PID_OMAX"},
    {"ANG_PID_FLTD", "ANG_PID_FLTD"},
    {"ANG_PID_DTMIN", "ANG_PID_DTMIN"},
    {"ANG_PID_DTMAX", "ANG_PID_DTMAX"},
    {"ANG_PID_DMODE", "ANG_PID_DMODE"},
    {"POS_PID_P", "POS_PID_P"},
    {"POS_PID_I", "POS_PID_I"},
    {"POS_PID_D", "POS_PID_D"},
    {"POS_PID_FF", "POS_PID_FF"},
    {"POS_PID_IMIN", "POS_PID_IMIN"},
    {"POS_PID_IMAX", "POS_PID_IMAX"},
    {"POS_PID_OMIN", "POS_PID_OMIN"},
    {"POS_PID_OMAX", "POS_PID_OMAX"},
    {"POS_PID_FLTD", "POS_PID_FLTD"},
    {"POS_PID_DTMIN", "POS_PID_DTMIN"},
    {"POS_PID_DTMAX", "POS_PID_DTMAX"},
    {"POS_PID_DMODE", "POS_PID_DMODE"},
};

bool IsNameValid(const char *name)
{
  return (name != nullptr) && (name[0] != '\0');
}

uint16_t MavlinkBindingCount()
{
  return static_cast<uint16_t>(sizeof(kMavlinkParameterBindings) /
                               sizeof(kMavlinkParameterBindings[0]));
}

const MavlinkParameterBinding *MavlinkBindingAt(uint16_t index)
{
  return (index < MavlinkBindingCount()) ? &kMavlinkParameterBindings[index]
                                         : nullptr;
}

ParameterType ToParameterType(uint8_t type)
{
  switch (type) {
    case MAV_PARAM_TYPE_UINT8:
      return ParameterType::kUint8;

    case MAV_PARAM_TYPE_UINT16:
      return ParameterType::kUint16;

    case MAV_PARAM_TYPE_UINT32:
      return ParameterType::kUint32;

    case MAV_PARAM_TYPE_INT32:
      return ParameterType::kInt32;

    case MAV_PARAM_TYPE_REAL32:
      return ParameterType::kFloat;

    default:
      return ParameterType::kBytes;
  }
}

uint8_t ToMavlinkParameterType(ParameterType type)
{
  switch (type) {
    case ParameterType::kBool:
    case ParameterType::kUint8:
      return MAV_PARAM_TYPE_UINT8;

    case ParameterType::kUint16:
      return MAV_PARAM_TYPE_UINT16;

    case ParameterType::kUint32:
      return MAV_PARAM_TYPE_UINT32;

    case ParameterType::kInt32:
      return MAV_PARAM_TYPE_INT32;

    case ParameterType::kFloat:
      return MAV_PARAM_TYPE_REAL32;

    default:
      return MAV_PARAM_TYPE_UINT8;
  }
}

bool IsTypeMatched(ParameterType parameter_type,
                   ParameterType request_type)
{
  return (parameter_type == request_type) ||
         ((parameter_type == ParameterType::kBool) &&
          (request_type == ParameterType::kUint8));
}

float EncodeParameterValue(const ParameterManager::EntryView &parameter)
{
  if (parameter.storage == nullptr) {
    return 0.0f;
  }

  switch (parameter.type) {
    case ParameterType::kBool:
      return (*reinterpret_cast<const bool *>(parameter.storage)) ? 1.0f : 0.0f;

    case ParameterType::kUint8:
      return static_cast<float>(*reinterpret_cast<const uint8_t *>(parameter.storage));

    case ParameterType::kUint16:
      return static_cast<float>(*reinterpret_cast<const uint16_t *>(parameter.storage));

    case ParameterType::kUint32:
      return static_cast<float>(*reinterpret_cast<const uint32_t *>(parameter.storage));

    case ParameterType::kInt32:
      return static_cast<float>(*reinterpret_cast<const int32_t *>(parameter.storage));

    case ParameterType::kFloat:
      return *reinterpret_cast<const float *>(parameter.storage);

    default:
      return 0.0f;
  }
}

} // namespace

MavlinkParameterService::MavlinkParameterService(ParameterManager *parameters)
    : parameters_(parameters)
{
}

void MavlinkParameterService::BindParameterManager(ParameterManager *parameters)
{
  parameters_ = parameters;
}

uint16_t MavlinkParameterService::Count() const
{
  return MavlinkBindingCount();
}

bool MavlinkParameterService::ReadByIndex(uint16_t index,
                                          MavlinkParameterValue *parameter) const
{
  if (parameter == nullptr) {
    return false;
  }

  const MavlinkParameterBinding *binding = MavlinkBindingAt(index);
  if ((binding == nullptr) || !IsNameValid(binding->mavlink_name) ||
      !IsNameValid(binding->project_name)) {
    return false;
  }

  const ParameterManager::EntryView *entry =
      Parameters().Find(binding->project_name);
  if (entry == nullptr) {
    return false;
  }

  parameter->name = binding->mavlink_name;
  parameter->value = EncodeParameterValue(*entry);
  parameter->type = ToMavlinkParameterType(entry->type);
  parameter->count = Count();
  parameter->index = index;
  return true;
}

bool MavlinkParameterService::ReadByName(const char *name,
                                         MavlinkParameterValue *parameter) const
{
  const int16_t index = IndexOf(name);
  if (index < 0) {
    return false;
  }

  return ReadByIndex(static_cast<uint16_t>(index), parameter);
}

int16_t MavlinkParameterService::IndexOf(const char *name) const
{
  if (!IsNameValid(name)) {
    return -1;
  }

  const uint16_t count = Count();
  for (uint16_t index = 0U; index < count; ++index) {
    const MavlinkParameterBinding *binding = MavlinkBindingAt(index);
    if ((binding != nullptr) && (binding->mavlink_name != nullptr) &&
        (strcmp(binding->mavlink_name, name) == 0)) {
      return static_cast<int16_t>(index);
    }
  }

  return -1;
}

bool MavlinkParameterService::WriteValue(const char *name,
                                         float value,
                                         uint8_t type)
{
  const int16_t index = IndexOf(name);
  if (index < 0) {
    return false;
  }

  const MavlinkParameterBinding *binding =
      MavlinkBindingAt(static_cast<uint16_t>(index));
  if ((binding == nullptr) || !IsNameValid(binding->project_name)) {
    return false;
  }

  ParameterManager &parameters = Parameters();
  const ParameterManager::EntryView *entry =
      parameters.Find(binding->project_name);
  const ParameterType request_type = ToParameterType(type);
  if ((entry == nullptr) ||
      (entry->access != ParameterManager::AccessMode::kReadWrite) ||
      !IsTypeMatched(entry->type, request_type)) {
    return false;
  }

  switch (entry->type) {
    case ParameterType::kBool: {
      const bool typed_value = value != 0.0f;
      return parameters.WriteRaw(binding->project_name,
                                 &typed_value,
                                 sizeof(typed_value));
    }

    case ParameterType::kUint8: {
      const uint8_t typed_value = static_cast<uint8_t>(value);
      return parameters.WriteRaw(binding->project_name,
                                 &typed_value,
                                 sizeof(typed_value));
    }

    case ParameterType::kUint16: {
      const uint16_t typed_value = static_cast<uint16_t>(value);
      return parameters.WriteRaw(binding->project_name,
                                 &typed_value,
                                 sizeof(typed_value));
    }

    case ParameterType::kUint32: {
      const uint32_t typed_value = static_cast<uint32_t>(value);
      return parameters.WriteRaw(binding->project_name,
                                 &typed_value,
                                 sizeof(typed_value));
    }

    case ParameterType::kInt32: {
      const int32_t typed_value = static_cast<int32_t>(value);
      return parameters.WriteRaw(binding->project_name,
                                 &typed_value,
                                 sizeof(typed_value));
    }

    case ParameterType::kFloat:
      return parameters.WriteRaw(binding->project_name, &value, sizeof(value));

    default:
      return false;
  }
}

ParameterManager &MavlinkParameterService::Parameters() const
{
  return (parameters_ != nullptr) ? *parameters_
                                  : ParameterManager::Instance();
}

} // namespace iFly

