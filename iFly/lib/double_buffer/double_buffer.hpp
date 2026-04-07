#ifndef IFLY_DOUBLE_BUFFER_HPP
#define IFLY_DOUBLE_BUFFER_HPP

#include <stdint.h>

namespace iFly {

template <typename LengthType = uint16_t>
class DoubleBufferLengthBase {
public:
  DoubleBufferLengthBase() noexcept = default;

  DoubleBufferLengthBase(const DoubleBufferLengthBase &) = delete;
  /*禁用拷贝*/
  DoubleBufferLengthBase &operator=(const DoubleBufferLengthBase &) = delete;

  void ResetLengths() noexcept {
    lengths_[0] = LengthType {};
    lengths_[1] = LengthType {};
    activeSlot_ = 0U;
  }

  void SwapBuffers() noexcept {
    activeSlot_ ^= 0x01U;
  }

  void SetActiveLength(LengthType length) noexcept {
    lengths_[activeSlot_] = length;
  }

  LengthType ActiveLength() const noexcept {
    return lengths_[activeSlot_];
  }

  void SetInactiveLength(LengthType length) noexcept {
    lengths_[InactiveSlotIndex()] = length;
  }

  LengthType InactiveLength() const noexcept {
    return lengths_[InactiveSlotIndex()];
  }

  void ClearActive() noexcept {
    lengths_[activeSlot_] = LengthType {};
  }

  void ClearInactive() noexcept {
    lengths_[InactiveSlotIndex()] = LengthType {};
  }

  bool HasActiveData() const noexcept {
    return ActiveLength() != LengthType {};
  }

  bool HasInactiveData() const noexcept {
    return InactiveLength() != LengthType {};
  }

protected:
  uint8_t ActiveSlotIndex() const noexcept {
    return activeSlot_;
  }

  uint8_t InactiveSlotIndex() const noexcept {
    return static_cast<uint8_t>(activeSlot_ ^ 0x01U);
  }

private:
  LengthType lengths_[2] {};
  uint8_t activeSlot_ = 0U;
};

template <uint16_t kStorageSize, typename LengthType = uint16_t>
class StaticByteDoubleBuffer : public DoubleBufferLengthBase<LengthType> {
public:
  StaticByteDoubleBuffer() noexcept = default;
  explicit StaticByteDoubleBuffer(uint16_t packetSize) noexcept {
    (void)Recreate(packetSize);
  }

  bool Recreate(uint16_t packetSize) noexcept {
    packetSize_ = 0U;
    this->ResetLengths();

    if ((packetSize == 0U) || (packetSize > kStorageSize)) {
      return false;
    }

    packetSize_ = packetSize;
    return true;
  }

  void Clear() noexcept {
    this->ResetLengths();
  }

  bool IsCreated() const noexcept {
    return packetSize_ > 0U;
  }

  uint16_t PacketSize() const noexcept {
    return packetSize_;
  }

  uint8_t *ActiveBuffer() noexcept {
    return SlotBuffer(this->ActiveSlotIndex());
  }

  const uint8_t *ActiveBuffer() const noexcept {
    return SlotBuffer(this->ActiveSlotIndex());
  }

  uint8_t *InactiveBuffer() noexcept {
    return SlotBuffer(this->InactiveSlotIndex());
  }

  const uint8_t *InactiveBuffer() const noexcept {
    return SlotBuffer(this->InactiveSlotIndex());
  }

private:
  uint8_t *SlotBuffer(uint8_t slotIndex) noexcept {
    return IsCreated() ? storage_[slotIndex] : nullptr;
  }

  const uint8_t *SlotBuffer(uint8_t slotIndex) const noexcept {
    return IsCreated() ? storage_[slotIndex] : nullptr;
  }

private:
  uint8_t storage_[2][kStorageSize] {};
  uint16_t packetSize_ = 0U;
};

template <typename T>
class StaticObjectDoubleBuffer {
public:
  StaticObjectDoubleBuffer() noexcept = default;

  StaticObjectDoubleBuffer(const StaticObjectDoubleBuffer &) = delete;
  StaticObjectDoubleBuffer &operator=(const StaticObjectDoubleBuffer &) = delete;

  void Recreate() noexcept {
    Clear();
  }

  void Clear() noexcept {
    valid_[0] = false;
    valid_[1] = false;
    activeSlot_ = 0U;
    slots_[0] = T {};
    slots_[1] = T {};
  }

  bool HasActiveData() const noexcept {
    return valid_[activeSlot_];
  }

  bool HasInactiveData() const noexcept {
    return valid_[InactiveSlotIndex()];
  }

  T &ActiveObject() noexcept {
    return slots_[activeSlot_];
  }

  const T &ActiveObject() const noexcept {
    return slots_[activeSlot_];
  }

  T &InactiveObject() noexcept {
    return slots_[InactiveSlotIndex()];
  }

  const T &InactiveObject() const noexcept {
    return slots_[InactiveSlotIndex()];
  }

  void SetInactiveObject(const T &object) noexcept {
    slots_[InactiveSlotIndex()] = object;
    valid_[InactiveSlotIndex()] = true;
  }

  void ClearActive() noexcept {
    valid_[activeSlot_] = false;
    slots_[activeSlot_] = T {};
  }

  void ClearInactive() noexcept {
    valid_[InactiveSlotIndex()] = false;
    slots_[InactiveSlotIndex()] = T {};
  }

  void SwapBuffers() noexcept {
    activeSlot_ ^= 0x01U;
  }

protected:
  uint8_t ActiveSlotIndex() const noexcept {
    return activeSlot_;
  }

  uint8_t InactiveSlotIndex() const noexcept {
    return static_cast<uint8_t>(activeSlot_ ^ 0x01U);
  }

private:
  T slots_[2] {};
  bool valid_[2] {};
  uint8_t activeSlot_ = 0U;
};

} // namespace iFly

#endif /* IFLY_DOUBLE_BUFFER_HPP */
