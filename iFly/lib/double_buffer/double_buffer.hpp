/**
 * @file double_buffer.hpp
 * @brief 双缓冲工具模板。
 */
#ifndef IFLY_DOUBLE_BUFFER_HPP
#define IFLY_DOUBLE_BUFFER_HPP

#include <stdint.h>

namespace iFly {

/**
 * @brief 双缓冲长度管理基类。
 *
 * @tparam LengthType 长度字段类型。
 */
template <typename LengthType = uint16_t>
class DoubleBufferLengthBase {
public:
  DoubleBufferLengthBase() = default;

  DoubleBufferLengthBase(const DoubleBufferLengthBase &) = delete;
  DoubleBufferLengthBase &operator=(const DoubleBufferLengthBase &) = delete;

  /**
   * @brief 重置两组长度状态并恢复默认活动槽位。
   */
  void ResetLengths() {
    lengths_[0] = LengthType {};
    lengths_[1] = LengthType {};
    activeSlot_ = 0U;
  }

  /**
   * @brief 交换活动槽位与备用槽位。
   */
  void SwapBuffers() {
    activeSlot_ ^= 0x01U;
  }

  /**
   * @brief 设置当前活动槽位的长度。
   *
   * @param length 新的活动长度。
   */
  void SetActiveLength(LengthType length) {
    lengths_[activeSlot_] = length;
  }

  /**
   * @brief 获取当前活动槽位的长度。
   *
   * @return 当前活动长度。
   */
  LengthType ActiveLength() const {
    return lengths_[activeSlot_];
  }

  /**
   * @brief 设置当前备用槽位的长度。
   *
   * @param length 新的备用长度。
   */
  void SetInactiveLength(LengthType length) {
    lengths_[InactiveSlotIndex()] = length;
  }

  /**
   * @brief 获取当前备用槽位的长度。
   *
   * @return 当前备用长度。
   */
  LengthType InactiveLength() const {
    return lengths_[InactiveSlotIndex()];
  }

  /**
   * @brief 清空当前活动槽位的长度。
   */
  void ClearActive() {
    lengths_[activeSlot_] = LengthType {};
  }

  /**
   * @brief 清空当前备用槽位的长度。
   */
  void ClearInactive() {
    lengths_[InactiveSlotIndex()] = LengthType {};
  }

  /**
   * @brief 判断当前活动槽位是否存在有效数据。
   *
   * @return 存在有效数据返回 `true`。
   */
  bool HasActiveData() const {
    return ActiveLength() != LengthType {};
  }

  /**
   * @brief 判断当前备用槽位是否存在有效数据。
   *
   * @return 存在有效数据返回 `true`。
   */
  bool HasInactiveData() const {
    return InactiveLength() != LengthType {};
  }

protected:
  /**
   * @brief 获取当前活动槽位索引。
   *
   * @return 活动槽位索引。
   */
  uint8_t ActiveSlotIndex() const {
    return activeSlot_;
  }

  /**
   * @brief 获取当前备用槽位索引。
   *
   * @return 备用槽位索引。
   */
  uint8_t InactiveSlotIndex() const {
    return static_cast<uint8_t>(activeSlot_ ^ 0x01U);
  }

private:
  LengthType lengths_[2] {}; /**< 两个槽位各自的长度信息。 */
  uint8_t activeSlot_ = 0U; /**< 当前活动槽位索引。 */
};

/**
 * @brief 带固定字节存储区的双缓冲区。
 *
 * @tparam kStorageSize 单个槽位的固定容量。
 * @tparam LengthType 长度字段类型。
 */
template <uint16_t kStorageSize, typename LengthType = uint16_t>
class StaticByteDoubleBuffer : public DoubleBufferLengthBase<LengthType> {
public:
  StaticByteDoubleBuffer() = default;

  /**
   * @brief 使用指定包长直接构造双缓冲区。
   *
   * @param packetSize 单个槽位的有效包长。
   */
  explicit StaticByteDoubleBuffer(uint16_t packetSize) {
    (void)Recreate(packetSize);
  }

  /**
   * @brief 重新设置有效包长并重置状态。
   *
   * @param packetSize 单个槽位的有效包长。
   * @return 设置成功返回 `true`。
   */
  bool Recreate(uint16_t packetSize) {
    packetSize_ = 0U;
    this->ResetLengths();

    if ((packetSize == 0U) || (packetSize > kStorageSize)) {
      return false;
    }

    packetSize_ = packetSize;
    return true;
  }

  /**
   * @brief 清空缓冲区状态。
   */
  void Clear() {
    this->ResetLengths();
  }

  /**
   * @brief 判断双缓冲区是否已创建成功。
   *
   * @return 已创建返回 `true`。
   */
  bool IsCreated() const {
    return packetSize_ > 0U;
  }

  /**
   * @brief 获取当前有效包长。
   *
   * @return 当前包长。
   */
  uint16_t PacketSize() const {
    return packetSize_;
  }

  /**
   * @brief 获取活动槽位的可写缓冲区指针。
   *
   * @return 活动缓冲区首地址。
   */
  uint8_t *ActiveBuffer() {
    return SlotBuffer(this->ActiveSlotIndex());
  }

  /**
   * @brief 获取活动槽位的只读缓冲区指针。
   *
   * @return 活动缓冲区首地址。
   */
  const uint8_t *ActiveBuffer() const {
    return SlotBuffer(this->ActiveSlotIndex());
  }

  /**
   * @brief 获取备用槽位的可写缓冲区指针。
   *
   * @return 备用缓冲区首地址。
   */
  uint8_t *InactiveBuffer() {
    return SlotBuffer(this->InactiveSlotIndex());
  }

  /**
   * @brief 获取备用槽位的只读缓冲区指针。
   *
   * @return 备用缓冲区首地址。
   */
  const uint8_t *InactiveBuffer() const {
    return SlotBuffer(this->InactiveSlotIndex());
  }

private:
  /**
   * @brief 按槽位索引返回缓冲区指针。
   *
   * @param slotIndex 槽位索引。
   * @return 槽位有效时返回缓冲区首地址，否则返回 `nullptr`。
   */
  uint8_t *SlotBuffer(uint8_t slotIndex) {
    return IsCreated() ? storage_[slotIndex] : nullptr;
  }

  /**
   * @brief 按槽位索引返回只读缓冲区指针。
   *
   * @param slotIndex 槽位索引。
   * @return 槽位有效时返回缓冲区首地址，否则返回 `nullptr`。
   */
  const uint8_t *SlotBuffer(uint8_t slotIndex) const {
    return IsCreated() ? storage_[slotIndex] : nullptr;
  }

private:
  uint8_t storage_[2][kStorageSize] {}; /**< 两个固定大小的字节槽位。 */
  uint16_t packetSize_ = 0U; /**< 当前有效包长。 */
};

/**
 * @brief 带固定对象存储区的双缓冲区。
 *
 * @tparam T 存放的对象类型。
 */
template <typename T>
class StaticObjectDoubleBuffer {
public:
  StaticObjectDoubleBuffer() = default;

  StaticObjectDoubleBuffer(const StaticObjectDoubleBuffer &) = delete;
  StaticObjectDoubleBuffer &operator=(const StaticObjectDoubleBuffer &) = delete;

  /**
   * @brief 重置对象缓冲区状态。
   */
  void Recreate() {
    Clear();
  }

  /**
   * @brief 清空活动槽位和备用槽位。
   */
  void Clear() {
    valid_[0] = false;
    valid_[1] = false;
    activeSlot_ = 0U;
    slots_[0] = T {};
    slots_[1] = T {};
  }

  /**
   * @brief 判断活动槽位是否存在有效对象。
   *
   * @return 存在有效对象返回 `true`。
   */
  bool HasActiveData() const {
    return valid_[activeSlot_];
  }

  /**
   * @brief 判断备用槽位是否存在有效对象。
   *
   * @return 存在有效对象返回 `true`。
   */
  bool HasInactiveData() const {
    return valid_[InactiveSlotIndex()];
  }

  /**
   * @brief 获取活动槽位对象。
   *
   * @return 活动槽位对象引用。
   */
  T &ActiveObject() {
    return slots_[activeSlot_];
  }

  /**
   * @brief 获取活动槽位对象的只读引用。
   *
   * @return 活动槽位对象只读引用。
   */
  const T &ActiveObject() const {
    return slots_[activeSlot_];
  }

  /**
   * @brief 获取备用槽位对象。
   *
   * @return 备用槽位对象引用。
   */
  T &InactiveObject() {
    return slots_[InactiveSlotIndex()];
  }

  /**
   * @brief 获取备用槽位对象的只读引用。
   *
   * @return 备用槽位对象只读引用。
   */
  const T &InactiveObject() const {
    return slots_[InactiveSlotIndex()];
  }

  /**
   * @brief 设置备用槽位对象并标记为有效。
   *
   * @param object 待写入的对象。
   */
  void SetInactiveObject(const T &object) {
    slots_[InactiveSlotIndex()] = object;
    valid_[InactiveSlotIndex()] = true;
  }

  /**
   * @brief 清空活动槽位对象。
   */
  void ClearActive() {
    valid_[activeSlot_] = false;
    slots_[activeSlot_] = T {};
  }

  /**
   * @brief 清空备用槽位对象。
   */
  void ClearInactive() {
    valid_[InactiveSlotIndex()] = false;
    slots_[InactiveSlotIndex()] = T {};
  }

  /**
   * @brief 交换活动槽位与备用槽位。
   */
  void SwapBuffers() {
    activeSlot_ ^= 0x01U;
  }

protected:
  /**
   * @brief 获取当前活动槽位索引。
   *
   * @return 活动槽位索引。
   */
  uint8_t ActiveSlotIndex() const {
    return activeSlot_;
  }

  /**
   * @brief 获取当前备用槽位索引。
   *
   * @return 备用槽位索引。
   */
  uint8_t InactiveSlotIndex() const {
    return static_cast<uint8_t>(activeSlot_ ^ 0x01U);
  }

private:
  T slots_[2] {}; /**< 两个对象槽位。 */
  bool valid_[2] {}; /**< 槽位有效标志。 */
  uint8_t activeSlot_ = 0U; /**< 当前活动槽位索引。 */
};

} // namespace iFly

#endif /* IFLY_DOUBLE_BUFFER_HPP */

