/**
 * @file usermath.hpp
 * @brief 通用数值辅助函数。
 */
#ifndef IFLY_LIB_USERMATH_HPP
#define IFLY_LIB_USERMATH_HPP

namespace iFly::usermath {

/**
 * @brief 返回两个值中的较小值。
 *
 * @tparam T 值类型。
 * @param left 左操作数。
 * @param right 右操作数。
 * @return 两个值中的较小值。
 */
template <typename T>
constexpr T Min(T left, T right) {
  return (left < right) ? left : right;
}

/**
 * @brief 返回两个值中的较大值。
 *
 * @tparam T 值类型。
 * @param left 左操作数。
 * @param right 右操作数。
 * @return 两个值中的较大值。
 */
template <typename T>
constexpr T Max(T left, T right) {
  return (left < right) ? right : left;
}

/**
 * @brief 将输入值限制在给定区间内。
 *
 * @tparam T 值类型。
 * @param value 待限制的值。
 * @param lower 区间下界。
 * @param upper 区间上界。
 * @return 落在 `[lower, upper]` 区间内的值。
 */
template <typename T>
constexpr T Clamp(T value, T lower, T upper) {
  return Min(Max(value, lower), upper);
}

} // namespace iFly::usermath

#endif /* IFLY_LIB_USERMATH_HPP */
