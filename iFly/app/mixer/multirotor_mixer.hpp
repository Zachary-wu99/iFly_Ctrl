/**
 * @file multirotor_mixer.hpp
 * @brief 多旋翼电机混控器接口。
 */
#ifndef IFLY_APP_MIXER_MULTIROTOR_MIXER_HPP
#define IFLY_APP_MIXER_MULTIROTOR_MIXER_HPP

#include <stdint.h>

namespace iFly {

/**
 * @brief 多旋翼混控构型。
 */
enum class MixerFrame : uint8_t {
  kX4 = 4U, /**< 四旋翼 X 构型。 */
  kX6 = 6U, /**< 六旋翼 X 构型。 */
};

/**
 * @brief 混控构型特征。
 *
 * @tparam frame 混控构型。
 */
template <MixerFrame frame>
struct MixerFrameTraits;

/**
 * @brief 四旋翼 X 构型特征。
 */
template <>
struct MixerFrameTraits<MixerFrame::kX4> final {
  static constexpr uint8_t kMotorCount = 4U; /**< 电机数量。 */
};

/**
 * @brief 六旋翼 X 构型特征。
 */
template <>
struct MixerFrameTraits<MixerFrame::kX6> final {
  static constexpr uint8_t kMotorCount = 6U; /**< 电机数量。 */
};

/**
 * @brief 多旋翼电机混控器。
 *
 * @tparam frame 混控构型。
 */
template <MixerFrame frame>
class MultirotorMixer final {
public:
  static constexpr uint8_t kMotorCount = MixerFrameTraits<frame>::kMotorCount; /**< 电机数量。 */

  /**
   * @brief 单次混控输入。
   */
  struct Input final {
    float throttle = 0.0f; /**< 油门输入。 */
    float roll = 0.0f; /**< 横滚输入。 */
    float pitch = 0.0f; /**< 俯仰输入。 */
    float yaw = 0.0f; /**< 偏航输入。 */
  };

  /**
   * @brief 单个电机的输入混控系数。
   */
  struct MotorInputConfig final {
    float throttle = 0.0f; /**< 油门系数。 */
    float roll = 0.0f; /**< 横滚系数。 */
    float pitch = 0.0f; /**< 俯仰系数。 */
    float yaw = 0.0f; /**< 偏航系数。 */
  };

  /**
   * @brief 混控器配置参数。
   */
  struct Config final {
    MotorInputConfig motor[kMotorCount] {}; /**< 各电机混控系数。 */
    float output_min = 0.0f; /**< 输出下限。 */
    float output_max = 1.0f; /**< 输出上限。 */
  };

  /**
   * @brief 单次混控输出。
   */
  struct Output final {
    float motor[kMotorCount] {}; /**< 各电机输出。 */
  };

  MultirotorMixer() = default;

  /**
   * @brief 使用给定配置构造混控器。
   *
   * @param config 混控器配置参数。
   */
  explicit MultirotorMixer(const Config &config)
      : config_(config)
  {
  }

  /**
   * @brief 应用新的混控器配置。
   *
   * @param config 混控器配置参数。
   */
  void Configure(const Config &config)
  {
    config_ = config;
  }

  /**
   * @brief 获取当前生效配置。
   *
   * @return 配置只读引用。
   */
  const Config &GetConfig() const
  {
    return config_;
  }

  /**
   * @brief 执行一次电机混控计算。
   *
   * @param input 本次混控输入。
   * @return 本次混控输出。
   */
  Output Mix(const Input &input) const
  {
    Output output {};

    for (uint8_t index = 0U; index < kMotorCount; ++index) {
      const MotorInputConfig &motor_config = config_.motor[index];
      const float mixed = (input.throttle * motor_config.throttle) +
                          (input.roll * motor_config.roll) +
                          (input.pitch * motor_config.pitch) +
                          (input.yaw * motor_config.yaw);
      output.motor[index] = Clamp(mixed, config_.output_min, config_.output_max);
    }

    return output;
  }

private:
  /**
   * @brief 将输出值限制在指定区间内。
   *
   * @param value 待限制的输出值。
   * @param lower 输出下限。
   * @param upper 输出上限。
   * @return 限制后的输出值。
   */
  static float Clamp(float value, float lower, float upper)
  {
    if (lower > upper) {
      return value;
    }

    if (value < lower) {
      return lower;
    }

    if (value > upper) {
      return upper;
    }

    return value;
  }

  Config config_ {}; /**< 当前生效配置。 */
};

/**
 * @brief 四旋翼 X 构型混控器。
 */
using X4Mixer = MultirotorMixer<MixerFrame::kX4>;

/**
 * @brief 六旋翼 X 构型混控器。
 */
using X6Mixer = MultirotorMixer<MixerFrame::kX6>;

} // namespace iFly

#endif /* IFLY_APP_MIXER_MULTIROTOR_MIXER_HPP */
