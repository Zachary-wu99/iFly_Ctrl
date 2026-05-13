/**
 * @file sys_state_type.hpp
 * @brief 系统状态变量类型定义。
 */
#ifndef IFLY_APP_FLY_SYS_SYS_STATE_TYPE_HPP
#define IFLY_APP_FLY_SYS_SYS_STATE_TYPE_HPP

#include <stdint.h>

namespace iFly {

inline constexpr uint8_t kCommandArgumentCount = 7U; /**< 命令参数数量。 */
inline constexpr uint8_t kRcChannelCount = 18U; /**< RC 通道数量。 */
inline constexpr uint8_t kOutputChannelCount = 16U; /**< 输出通道数量。 */
inline constexpr uint8_t kBatteryCellCount = 10U; /**< 电池单体电压数量。 */
inline constexpr uint8_t kStatusTextLength = 50U; /**< 状态文本最大长度。 */
inline constexpr uint8_t kVersionHashLength = 8U; /**< 版本哈希长度。 */
inline constexpr uint8_t kUniqueIdLength = 18U; /**< 扩展唯一 ID 长度。 */

/**
 * @brief 飞行器类型，取值与 MAVLink HEARTBEAT 兼容。
 */
enum class VehicleType : uint8_t {
  kGeneric = 0U, /**< 通用飞行器。 */
  kFixedWing = 1U, /**< 固定翼。 */
  kQuadrotor = 2U, /**< 四旋翼。 */
  kHexarotor = 13U, /**< 六旋翼。 */
  kOctorotor = 14U, /**< 八旋翼。 */
  kTricopter = 15U, /**< 三旋翼。 */
  kGenericMultirotor = 43U /**< 通用多旋翼。 */
};

/**
 * @brief 飞控类型，取值与 MAVLink HEARTBEAT 兼容。
 */
enum class AutopilotType : uint8_t {
  kGeneric = 0U, /**< 通用飞控。 */
  kArduPilot = 3U, /**< ArduPilot 飞控。 */
  kInvalid = 8U, /**< 非飞控组件。 */
  kPx4 = 12U /**< PX4 飞控。 */
};

/**
 * @brief 模式标志位，取值与 MAVLink HEARTBEAT 兼容。
 */
enum class ModeFlag : uint8_t {
  kCustomModeEnabled = 1U, /**< 使用自定义模式。 */
  kTestEnabled = 2U, /**< 测试模式已启用。 */
  kAutoEnabled = 4U, /**< 自动模式已启用。 */
  kGuidedEnabled = 8U, /**< 引导模式已启用。 */
  kStabilizeEnabled = 16U, /**< 增稳模式已启用。 */
  kHilEnabled = 32U, /**< 硬件在环模式已启用。 */
  kManualInputEnabled = 64U, /**< 手动输入已启用。 */
  kSafetyArmed = 128U /**< 飞行器已解锁。 */
};

/**
 * @brief 飞行器运行状态，取值与 MAVLink HEARTBEAT 兼容。
 */
enum class VehicleState : uint8_t {
  kUninit = 0U, /**< 未初始化。 */
  kBoot = 1U, /**< 启动中。 */
  kCalibrating = 2U, /**< 校准中。 */
  kStandby = 3U, /**< 待命。 */
  kActive = 4U, /**< 运行中。 */
  kCritical = 5U, /**< 严重告警。 */
  kEmergency = 6U, /**< 紧急状态。 */
  kPowerOff = 7U, /**< 关机中。 */
  kFlightTermination = 8U /**< 终止飞行。 */
};

/**
 * @brief 传感器与控制器状态位，取值与 MAVLink SYS_STATUS 兼容。
 */
enum class SensorFlag : uint32_t {
  kGyro = 1UL, /**< 三轴陀螺仪。 */
  kAccel = 2UL, /**< 三轴加速度计。 */
  kMag = 4UL, /**< 三轴磁力计。 */
  kBaro = 8UL, /**< 气压计。 */
  kGps = 32UL, /**< GPS。 */
  kRateControl = 1024UL, /**< 角速度控制。 */
  kAttitudeControl = 2048UL, /**< 姿态控制。 */
  kYawControl = 4096UL, /**< 偏航控制。 */
  kAltitudeControl = 8192UL, /**< 高度控制。 */
  kPositionControl = 16384UL, /**< 位置控制。 */
  kMotorOutput = 32768UL, /**< 电机输出。 */
  kRcReceiver = 65536UL, /**< RC 接收机。 */
  kAhrs = 2097152UL, /**< AHRS。 */
  kBattery = 33554432UL, /**< 电池。 */
  kPrearmCheck = 268435456UL /**< 解锁前检查。 */
};

/**
 * @brief GPS 定位类型，取值与 MAVLink GPS_RAW_INT 兼容。
 */
enum class GpsFixType : uint8_t {
  kNoGps = 0U, /**< 未连接 GPS。 */
  kNoFix = 1U, /**< GPS 无定位。 */
  kFix2d = 2U, /**< 2D 定位。 */
  kFix3d = 3U, /**< 3D 定位。 */
  kDgps = 4U, /**< 差分定位。 */
  kRtkFloat = 5U, /**< RTK 浮点解。 */
  kRtkFixed = 6U /**< RTK 固定解。 */
};

/**
 * @brief 起落状态，取值与 MAVLink EXTENDED_SYS_STATE 兼容。
 */
enum class LandedState : uint8_t {
  kUndefined = 0U, /**< 未知。 */
  kOnGround = 1U, /**< 地面。 */
  kInAir = 2U, /**< 空中。 */
  kTakeoff = 3U, /**< 起飞中。 */
  kLanding = 4U /**< 降落中。 */
};

/**
 * @brief 状态文本等级，取值与 MAVLink STATUSTEXT 兼容。
 */
enum class Severity : uint8_t {
  kEmergency = 0U, /**< 系统不可用。 */
  kAlert = 1U, /**< 需要立即处理。 */
  kCritical = 2U, /**< 严重错误。 */
  kError = 3U, /**< 错误。 */
  kWarning = 4U, /**< 警告。 */
  kNotice = 5U, /**< 提示。 */
  kInfo = 6U, /**< 信息。 */
  kDebug = 7U /**< 调试。 */
};

/**
 * @brief 命令处理结果，取值与 MAVLink COMMAND_ACK 兼容。
 */
enum class CommandResult : uint8_t {
  kAccepted = 0U, /**< 已接受并执行。 */
  kTemporarilyRejected = 1U, /**< 暂时拒绝。 */
  kDenied = 2U, /**< 参数或状态不允许执行。 */
  kUnsupported = 3U, /**< 不支持该命令。 */
  kFailed = 4U, /**< 执行失败。 */
  kInProgress = 5U, /**< 正在执行。 */
  kCancelled = 6U /**< 已取消。 */
};

/**
 * @brief 心跳状态。
 */
struct Heartbeat final {
  VehicleType type = VehicleType::kGeneric; /**< 飞行器类型。 */
  AutopilotType autopilot = AutopilotType::kGeneric; /**< 飞控类型。 */
  uint8_t base_mode = 0U; /**< 基础模式标志位。 */
  uint32_t custom_mode = 0U; /**< 自定义模式。 */
  VehicleState state = VehicleState::kStandby; /**< 运行状态。 */
};

/**
 * @brief 系统健康状态。
 */
struct SystemStatus final {
  uint32_t sensors_present = 0U; /**< 已安装的传感器与控制器位图。 */
  uint32_t sensors_enabled = 0U; /**< 已启用的传感器与控制器位图。 */
  uint32_t sensors_health = 0U; /**< 工作正常的传感器与控制器位图。 */
  uint16_t load = 0U; /**< 主循环负载，单位为 0.1%。 */
  uint16_t battery_voltage = 0xFFFFU; /**< 电池电压，单位为 mV。 */
  int16_t battery_current = -1; /**< 电池电流，单位为 cA。 */
  int8_t battery_remaining = -1; /**< 剩余电量，单位为 %。 */
  uint16_t comm_drop_rate = 0U; /**< 通信丢包率，单位为 0.01%。 */
  uint16_t comm_errors = 0U; /**< 通信错误计数。 */
};

/**
 * @brief 电池状态。
 */
struct BatteryStatus final {
  uint8_t id = 0U; /**< 电池编号。 */
  uint16_t voltage = 0xFFFFU; /**< 总电压，单位为 mV。 */
  int16_t current = -1; /**< 电流，单位为 cA。 */
  int8_t remaining = -1; /**< 剩余电量，单位为 %。 */
  int16_t temperature = 32767; /**< 温度，单位为 0.01 摄氏度。 */
  uint16_t cell_voltage[kBatteryCellCount] {}; /**< 单体电压，单位为 mV。 */
};

/**
 * @brief GPS 状态。
 */
struct GpsStatus final {
  uint64_t time_usec = 0ULL; /**< 时间戳，单位为 us。 */
  GpsFixType fix_type = GpsFixType::kNoGps; /**< 定位类型。 */
  int32_t latitude = 0; /**< 纬度，单位为 1E-7 deg。 */
  int32_t longitude = 0; /**< 经度，单位为 1E-7 deg。 */
  int32_t altitude = 0; /**< 海拔高度，单位为 mm。 */
  uint16_t horizontal_dop = 0xFFFFU; /**< 水平精度因子，单位为 0.01。 */
  uint16_t vertical_dop = 0xFFFFU; /**< 垂直精度因子，单位为 0.01。 */
  uint16_t ground_speed = 0xFFFFU; /**< 地速，单位为 cm/s。 */
  uint16_t course_over_ground = 0xFFFFU; /**< 地面航向，单位为 0.01 deg。 */
  uint8_t satellites_visible = 0xFFU; /**< 可见卫星数。 */
};

/**
 * @brief 全局位置状态。
 */
struct GlobalPosition final {
  uint32_t time_boot_ms = 0U; /**< 开机时间戳，单位为 ms。 */
  int32_t latitude = 0; /**< 纬度，单位为 1E-7 deg。 */
  int32_t longitude = 0; /**< 经度，单位为 1E-7 deg。 */
  int32_t altitude = 0; /**< 海拔高度，单位为 mm。 */
  int32_t relative_altitude = 0; /**< 相对起飞点高度，单位为 mm。 */
  int16_t velocity_north = 0; /**< 北向速度，单位为 cm/s。 */
  int16_t velocity_east = 0; /**< 东向速度，单位为 cm/s。 */
  int16_t velocity_down = 0; /**< 地向速度，单位为 cm/s。 */
  uint16_t heading = 0xFFFFU; /**< 航向角，单位为 0.01 deg。 */
};

/**
 * @brief 本地 NED 位置状态。
 */
struct LocalPosition final {
  uint32_t time_boot_ms = 0U; /**< 开机时间戳，单位为 ms。 */
  float north = 0.0f; /**< 北向位置，单位为 m。 */
  float east = 0.0f; /**< 东向位置，单位为 m。 */
  float down = 0.0f; /**< 地向位置，单位为 m。 */
  float velocity_north = 0.0f; /**< 北向速度，单位为 m/s。 */
  float velocity_east = 0.0f; /**< 东向速度，单位为 m/s。 */
  float velocity_down = 0.0f; /**< 地向速度，单位为 m/s。 */
};

/**
 * @brief 姿态状态。
 */
struct Attitude final {
  uint32_t time_boot_ms = 0U; /**< 开机时间戳，单位为 ms。 */
  float roll = 0.0f; /**< 横滚角，单位为 rad。 */
  float pitch = 0.0f; /**< 俯仰角，单位为 rad。 */
  float yaw = 0.0f; /**< 偏航角，单位为 rad。 */
  float roll_rate = 0.0f; /**< 横滚角速度，单位为 rad/s。 */
  float pitch_rate = 0.0f; /**< 俯仰角速度，单位为 rad/s。 */
  float yaw_rate = 0.0f; /**< 偏航角速度，单位为 rad/s。 */
};

/**
 * @brief 飞行仪表状态。
 */
struct FlightHud final {
  float airspeed = 0.0f; /**< 空速，单位为 m/s。 */
  float groundspeed = 0.0f; /**< 地速，单位为 m/s。 */
  int16_t heading = 0; /**< 航向角，单位为 deg。 */
  uint16_t throttle = 0U; /**< 油门，单位为 %。 */
  float altitude = 0.0f; /**< 海拔高度，单位为 m。 */
  float climb = 0.0f; /**< 爬升率，单位为 m/s。 */
};

/**
 * @brief RC 通道状态。
 */
struct RcChannels final {
  uint32_t time_boot_ms = 0U; /**< 开机时间戳，单位为 ms。 */
  uint8_t count = 0U; /**< 当前有效通道数量。 */
  uint16_t channel[kRcChannelCount] {}; /**< 通道 PWM，单位为 us。 */
  uint8_t rssi = 0xFFU; /**< 接收信号强度。 */
};

/**
 * @brief 输出通道状态。
 */
struct OutputStatus final {
  uint32_t time_usec = 0U; /**< 时间戳，单位为 us。 */
  uint8_t port = 0U; /**< 输出端口组编号。 */
  uint16_t output[kOutputChannelCount] {}; /**< 输出 PWM，单位为 us。 */
};

/**
 * @brief 扩展运行状态。
 */
struct ExtendedState final {
  LandedState landed_state = LandedState::kUndefined; /**< 起落状态。 */
  uint8_t vtol_state = 0U; /**< 垂直起降状态。 */
};

/**
 * @brief 状态文本。
 */
struct StatusText final {
  Severity severity = Severity::kInfo; /**< 文本等级。 */
  char text[kStatusTextLength] {}; /**< 文本内容。 */
  uint16_t id = 0U; /**< 分片文本 ID。 */
  uint8_t chunk_seq = 0U; /**< 分片序号。 */
};

/**
 * @brief 长命令请求。
 */
struct CommandRequest final {
  uint8_t target_system = 0U; /**< 目标系统 ID。 */
  uint8_t target_component = 0U; /**< 目标组件 ID。 */
  uint16_t command = 0U; /**< 命令 ID。 */
  uint8_t confirmation = 0U; /**< 重发确认序号。 */
  float argument[kCommandArgumentCount] {}; /**< 命令参数。 */
};

/**
 * @brief 命令响应。
 */
struct CommandResponse final {
  uint16_t command = 0U; /**< 已处理的命令 ID。 */
  CommandResult result = CommandResult::kAccepted; /**< 处理结果。 */
  uint8_t progress = 0xFFU; /**< 执行进度，单位为 %。 */
  int32_t result_detail = 0; /**< 扩展结果。 */
  uint8_t target_system = 0U; /**< 接收响应的系统 ID。 */
  uint8_t target_component = 0U; /**< 接收响应的组件 ID。 */
};

/**
 * @brief 手动控制输入。
 */
struct ManualControl final {
  uint8_t target = 0U; /**< 目标系统 ID。 */
  int16_t x = 0; /**< X 轴输入，范围为 -1000 到 1000。 */
  int16_t y = 0; /**< Y 轴输入，范围为 -1000 到 1000。 */
  int16_t z = 0; /**< Z 轴输入，范围为 -1000 到 1000。 */
  int16_t r = 0; /**< R 轴输入，范围为 -1000 到 1000。 */
  uint16_t buttons = 0U; /**< 按键 1 到 16 位图。 */
  uint16_t buttons2 = 0U; /**< 按键 17 到 32 位图。 */
};

/**
 * @brief 固件版本信息。
 */
struct VersionInfo final {
  uint64_t capabilities = 0ULL; /**< 协议能力位图。 */
  uint64_t unique_id = 0ULL; /**< 硬件唯一 ID。 */
  uint32_t flight_software_version = 0U; /**< 飞控固件版本。 */
  uint32_t middleware_software_version = 0U; /**< 中间件版本。 */
  uint32_t os_software_version = 0U; /**< 操作系统版本。 */
  uint32_t board_version = 0U; /**< 硬件版本。 */
  uint16_t vendor_id = 0U; /**< 厂商 ID。 */
  uint16_t product_id = 0U; /**< 产品 ID。 */
  uint8_t flight_version_hash[kVersionHashLength] {}; /**< 飞控版本哈希。 */
  uint8_t middleware_version_hash[kVersionHashLength] {}; /**< 中间件版本哈希。 */
  uint8_t os_version_hash[kVersionHashLength] {}; /**< 系统版本哈希。 */
  uint8_t unique_id_ext[kUniqueIdLength] {}; /**< 扩展硬件唯一 ID。 */
};

/**
 * @brief 系统状态变量集合。
 */
struct SysState final {
  Heartbeat heartbeat {}; /**< 心跳状态。 */
  SystemStatus system_status {}; /**< 系统健康状态。 */
  BatteryStatus battery {}; /**< 电池状态。 */
  GpsStatus gps {}; /**< GPS 状态。 */
  GlobalPosition global_position {}; /**< 全局位置状态。 */
  LocalPosition local_position {}; /**< 本地位置状态。 */
  Attitude attitude {}; /**< 姿态状态。 */
  FlightHud flight_hud {}; /**< 飞行仪表状态。 */
  RcChannels rc_channels {}; /**< RC 通道状态。 */
  OutputStatus output_status {}; /**< 输出通道状态。 */
  ExtendedState extended_state {}; /**< 扩展运行状态。 */
  StatusText status_text {}; /**< 状态文本。 */
  CommandRequest command_request {}; /**< 长命令请求。 */
  CommandResponse command_response {}; /**< 命令响应。 */
  ManualControl manual_control {}; /**< 手动控制输入。 */
  VersionInfo version {}; /**< 固件版本信息。 */
};

} // namespace iFly

#endif /* IFLY_APP_FLY_SYS_SYS_STATE_TYPE_HPP */

