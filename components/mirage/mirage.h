#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome {
namespace mirage {

/// Simple enum to represent models.

// Temperature
const float MIRAGE_TEMP_MAX = 32.0;
const float MIRAGE_TEMP_MIN = 16.0;

class MirageClimate : public climate_ir::ClimateIR {
 public:
  uint8_t swing_position = 0;
  MirageClimate()
      : climate_ir::ClimateIR(MIRAGE_TEMP_MIN, MIRAGE_TEMP_MAX, 1.0f, true, true,
                              {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM, climate::CLIMATE_FAN_HIGH},
                              {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL, climate::CLIMATE_SWING_HORIZONTAL, climate::CLIMATE_SWING_BOTH}) {}

  void set_supports_heat(bool supports_heat) { this->supports_heat_ = false; }

  void setup() override {
    climate_ir::ClimateIR::setup();
  }

 protected:
  /// Transmit via IR the state of this climate controller.
  void transmit_state() override;
  /// Handle received IR Buffer
  bool on_receive(remote_base::RemoteReceiveData data) override;
  /// Set the time of the last transmission.
  int32_t last_transmit_time_{};
};

}  // namespace mirage
}  // namespace esphome
