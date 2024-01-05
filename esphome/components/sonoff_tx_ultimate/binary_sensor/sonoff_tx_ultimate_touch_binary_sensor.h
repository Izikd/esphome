#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace sonoff_tx_ultimate {

class SonoffTXUltimateTouchBinarySensor : public binary_sensor::BinarySensor {
 public:
  /*
   * Channels
   */
  static const uint8_t CH_INVALID =     0x0;
  /* Channels range; 10 channels (sent on both; press and release) */
  static const uint8_t CH_MIN =         0x1;
  static const uint8_t CH_MAX =         0xA;
  /* Multi press (sent on release) */
  static const uint8_t CH_MULTI =       0xB;
  /* Swipe right (sent on release) */
  static const uint8_t CH_SWIPE_RIGHT = 0xC;
  /* Swipe left (sent on release) */
  static const uint8_t CH_SWIPE_LEFT =  0xD;
  /* Bit is set for long press (sent on release) for CH_MIN to CH_MAX */
  static const uint8_t CH_LONG_BIT =    0x10;
  /* 
   * Channels range for long release; 10 channels.
   * (sent on release, instead of release in range of CH_MIN and CH_MAX)
   */
  static const uint8_t CH_LONG_MIN =    0x11;
  static const uint8_t CH_LONG_MAX =    0x1A;

  void add_channel(uint8_t ch) { 
    this->channels_bitmap |= (1U << ch);
  }

  void process(uint8_t ch, bool state) {
    if (!((1U << ch) & this->channels_bitmap))
      return;

    this->publish_state(state);
  }

 protected:
  uint32_t channels_bitmap{0};
};

}  // namespace sonoff_tx_ultimate
}  // namespace esphome
