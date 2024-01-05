#pragma once

/*
 * sonoff_tx_ultimate.cpp - Sonoff TX Ultimate Touch support for ESPHome
 */

#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "binary_sensor/sonoff_tx_ultimate_touch_binary_sensor.h"

namespace esphome {
namespace sonoff_tx_ultimate {

class SonoffTXUltimate : public uart::UARTDevice, public Component {
 public:
  // Component methods
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return esphome::setup_priority::DATA; }

  // Custom methods
  void set_power_pin(InternalGPIOPin *pin) { this->power_pin_ = pin; };
  
  void register_touch_binary_sensor(SonoffTXUltimateTouchBinarySensor *touch_binary_sensor) {
    this->touch_binary_sensors_.push_back(touch_binary_sensor);
  }

 protected:
  struct TouchEvent {
    bool state;
    uint8_t ch;
  };

  InternalGPIOPin *power_pin_;
  bool buttons_{false};
  unsigned int read_count_{0};
  std::vector<SonoffTXUltimateTouchBinarySensor *> touch_binary_sensors_;

  void skip_command_();
  bool read_command_(TouchEvent &event);
  void process_command_(const TouchEvent &event);
};

}  // namespace sonoff_tx_ultimate
}  // namespace esphome
