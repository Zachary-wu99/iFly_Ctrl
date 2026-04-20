// 双缓冲工具模板。
// 为 USB、UART、CAN 等模块提供双槽切换和活跃槽管理能力。
#ifndef IFLY_DOUBLE_BUFFER_HPP
#define IFLY_DOUBLE_BUFFER_HPP

#include <stdint.h>

namespace iFly {

template <typename LengthType = uint16_t>
class DoubleBufferLengthBase {
public:
  DoubleBufferLengthBase() = default;

  DoubleBufferLengthBase(const DoubleBufferLengthBase &) = delete;
  /*禁用拷贝*/
  DoubleBufferLengthBase &operator=(const DoubleBufferLengthBase &) = delete;

  void ResetLengths() {
    lengths_[0] = LengthType {};
    lengths_[1] = LengthType {};
    activeSlot_ = 0U;
  }

  void SwapBuffers() {
    activeSlot_ ^= 0x01U;
  }

  void SetActiveLength(LengthType length) {
    lengths_[activeSlot_] = length;
  }

  LengthType ActiveLength() const {
    return lengths_[activeSlot_];
  }

  void SetInactiveLength(LengthType length) {
    lengths_[InactiveSlotIndex()] = length;
  }

  LengthType InactiveLength() const {
    return lengths_[InactiveSlotIndex()];
  }

  void ClearActive() {
    lengths_[activeSlot_] = LengthType {};
  }

  void ClearInactive() {
    lengths_[InactiveSlotIndex()] = LengthType {};
  }

  bool HasActiveData() const {
    return ActiveLength() != LengthType {};
  }

  bool HasInactiveData() const {
    return InactiveLength() != LengthType {};
  }

protected:
  uint8_t ActiveSlotIndex() const {
    return activeSlot_;
  }

  uint8_t InactiveSlotIndex() const {
    return static_cast<uint8_t>(activeSlot_ ^ 0x01U);
  }

private:
  LengthType lengths_[2] {};
  uint8_t activeSlot_ = 0U;
};

template <uint16_t kStorageSize, typename LengthType = uint16_t>
class StaticByteDoubleBuffer : public DoubleBufferLengthBase<LengthType> {
public:
  StaticByteDoubleBuffer() = default;
  explicit StaticByteDoubleBuffer(uint16_t packetSize) {
    (void)Recreate(packetSize);
  }

  bool Recreate(uint16_t packetSize) {
    packetSize_ = 0U;
    this->ResetLengths();

    if ((packetSize == 0U) || (packetSize > kStorageSize)) {
      return false;
    }

    packetSize_ = packetSize;
    return true;
  }

  void Clear() {
    this->ResetLengths();
  }

  bool IsCreated() const {
    return packetSize_ > 0U;
  }

  uint16_t PacketSize() const {
    return packetSize_;
  }

  uint8_t *ActiveBuffer() {
    return SlotBuffer(this->ActiveSlotIndex());
  }

  const uint8_t *ActiveBuffer() const {
    return SlotBuffer(this->ActiveSlotIndex());
  }

  uint8_t *InactiveBuffer() {
    return SlotBuffer(this->InactiveSlotIndex());
  }

  const uint8_t *InactiveBuffer() const {
    return SlotBuffer(this->InactiveSlotIndex());
  }

private:
  uint8_t *SlotBuffer(uint8_t slotIndex) {
    return IsCreated() ? storage_[slotIndex] : nullptr;
  }

  const uint8_t *SlotBuffer(uint8_t slotIndex) const {
    return IsCreated() ? storage_[slotIndex] : nullptr;
  }

private:
  uint8_t storage_[2][kStorageSize] {};
  uint16_t packetSize_ = 0U;
};

template <typename T>
class StaticObjectDoubleBuffer {
public:
  StaticObjectDoubleBuffer() = default;

  StaticObjectDoubleBuffer(const StaticObjectDoubleBuffer &) = delete;
  StaticObjectDoubleBuffer &operator=(const StaticObjectDoubleBuffer &) = delete;

  void Recreate() {
    Clear();
  }

  void Clear() {
    valid_[0] = false;
    valid_[1] = false;
    activeSlot_ = 0U;
    slots_[0] = T {};
    slots_[1] = T {};
  }

  bool HasActiveData() const {
    return valid_[activeSlot_];
  }

  bool HasInactiveData() const {
    return valid_[InactiveSlotIndex()];
  }

  T &ActiveObject() {
    return slots_[activeSlot_];
  }

  const T &ActiveObject() const {
    return slots_[activeSlot_];
  }

  T &InactiveObject() {
    return slots_[InactiveSlotIndex()];
  }

  const T &InactiveObject() const {
    return slots_[InactiveSlotIndex()];
  }

  void SetInactiveObject(const T &object) {
    slots_[InactiveSlotIndex()] = object;
    valid_[InactiveSlotIndex()] = true;
  }

  void ClearActive() {
    valid_[activeSlot_] = false;
    slots_[activeSlot_] = T {};
  }

  void ClearInactive() {
    valid_[InactiveSlotIndex()] = false;
    slots_[InactiveSlotIndex()] = T {};
  }

  void SwapBuffers() {
    activeSlot_ ^= 0x01U;
  }

protected:
  uint8_t ActiveSlotIndex() const {
    return activeSlot_;
  }

  uint8_t InactiveSlotIndex() const {
    return static_cast<uint8_t>(activeSlot_ ^ 0x01U);
  }

private:
  T slots_[2] {};
  bool valid_[2] {};
  uint8_t activeSlot_ = 0U;
};

} // namespace iFly

#endif /* IFLY_DOUBLE_BUFFER_HPP */
