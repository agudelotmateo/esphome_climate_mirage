#include "mirage.h"
#include "esphome/core/log.h"
#include "esphome/components/remote_base/mirage_protocol.h"

namespace esphome
{
  namespace mirage
  {
    static const char *const TAG = "mirage.climate";

    const uint8_t MIRAGE_STATE_LENGTH = 14;
    // NOT USED: 0 (constant, 0x56), 2-3, 6-13
    // USED:
    //   - 1: Temperature
    //   - 4: Mode (COOL / DRY / FAN_ONLY) and Fan Speed (Low / MED / HIGH)
    //   - 5: Swing, Display and OFF (C0)

    const uint8_t MIRAGE_COOL = 0x20;
    const uint8_t MIRAGE_DRY = 0x30;
    const uint8_t MIRAGE_AUTO = 0x40;
    const uint8_t MIRAGE_FAN = 0x50;

    const uint8_t MIRAGE_FAN_AUTO = 0;
    const uint8_t MIRAGE_FAN_HIGH = 1;
    const uint8_t MIRAGE_FAN_MED = 3;
    const uint8_t MIRAGE_FAN_LOW = 2;

    const uint8_t MIRAGE_SWING_BOTH = 3;
    const uint8_t MIRAGE_SWING_VERTICAL = 2;
    const uint8_t MIRAGE_SWING_HORIZONTAL = 1;
    const uint8_t MIRAGE_SWING_OFF = 0;

    const uint8_t MIRAGE_DISPLAY_TOGGLE_MASK = 4;

    const uint8_t MIRAGE_POWER_OFF = 0xC0;

    const uint8_t MIRAGE_TEMP_OFFSET = 0x5C;

    void MirageClimate::transmit_state()
    {
      this->last_transmit_time_ = millis(); // setting the time of the last transmission.
      uint8_t remote_state[MIRAGE_STATE_LENGTH] = {0};
      remote_state[0] = 0x56;
      remote_state[1] = MIRAGE_TEMP_OFFSET; // Starting temperature

      auto powered_on = this->mode != climate::CLIMATE_MODE_OFF;
      if (powered_on)
      {
        remote_state[5] = 0x1A;
      }

      // Swing
      switch (this->swing_mode)
      {
      case climate::CLIMATE_SWING_BOTH:
        remote_state[5] = MIRAGE_SWING_BOTH;
        break;
      case climate::CLIMATE_SWING_VERTICAL:
        remote_state[5] = MIRAGE_SWING_VERTICAL;
        break;
      case climate::CLIMATE_SWING_HORIZONTAL:
        remote_state[5] = MIRAGE_SWING_HORIZONTAL;
        break;
      case climate::CLIMATE_SWING_OFF:
        remote_state[5] = MIRAGE_SWING_OFF;
      default:
        break;
      }

      // Mode
      switch (this->mode)
      {
      case climate::CLIMATE_MODE_COOL:
        remote_state[4] |= MIRAGE_COOL;
        break;
      case climate::CLIMATE_MODE_DRY:
        remote_state[4] |= MIRAGE_DRY;
        break;
      case climate::CLIMATE_MODE_FAN_ONLY:
        remote_state[4] |= MIRAGE_FAN;
        break;
      case climate::CLIMATE_MODE_OFF:
        remote_state[5] = MIRAGE_POWER_OFF;
      default:
        break;
      }

      // Temperature
      auto temp = (uint8_t)roundf(clamp(this->target_temperature, MIRAGE_TEMP_MIN, MIRAGE_TEMP_MAX));
      remote_state[1] += temp;

      // Fan speed
      switch (this->fan_mode.value())
      {
      case climate::CLIMATE_FAN_LOW:
        remote_state[4] |= MIRAGE_FAN_LOW;
        break;
      case climate::CLIMATE_FAN_MEDIUM:
        remote_state[4] |= MIRAGE_FAN_MED;
        break;
      case climate::CLIMATE_FAN_HIGH:
        remote_state[4] |= MIRAGE_FAN_HIGH;
        break;
      default:
        break;
      }

      ESP_LOGI(TAG,
               "Sending: %02X %02X %02X %02X   %02X %02X %02X %02X   %02X %02X %02X %02X   %02X %02X",
               remote_state[0], remote_state[1], remote_state[2], remote_state[3], remote_state[4], remote_state[5],
               remote_state[6], remote_state[7], remote_state[8], remote_state[9], remote_state[10], remote_state[11],
               remote_state[12], remote_state[13]);

      esphome::remote_base::MirageData in;
      for (uint8_t i = 0; i < MIRAGE_STATE_LENGTH; i++)
      {
        in.data.push_back(remote_state[i]);
      }

      // Send code
      auto transmit = this->transmitter_->transmit();
      auto *data = transmit.get_data();

      esphome::remote_base::MirageProtocol obj = esphome::remote_base::MirageProtocol();
      obj.encode(data, in);

      transmit.perform();
    }

    bool MirageClimate::on_receive(remote_base::RemoteReceiveData data)
    {
      // Check if the esp isn't currently transmitting.
      if (millis() - this->last_transmit_time_ < 500)
      {
        ESP_LOGV(TAG, "Blocked receive because of current transmission");
        return false;
      }

      esphome::remote_base::MirageProtocol obj = esphome::remote_base::MirageProtocol();
      optional<esphome::remote_base::MirageData> optional_data_decoded = obj.decode(data);
      if (!optional_data_decoded)
      {
        ESP_LOGV(TAG, "Wrong data");
        return false;
      }

      const esphome::remote_base::MirageData &data_decoded = *optional_data_decoded; // Dereference the optional to get the MirageData
      obj.dump(data_decoded);

      // Swing
      // auto display_toggle = data_decoded.data[5] & MIRAGE_DISPLAY_TOGGLE_MASK;
      uint8_t swing = data_decoded.data[5] & (~MIRAGE_DISPLAY_TOGGLE_MASK);
      switch (swing)
      {
      case MIRAGE_SWING_BOTH:
        this->swing_mode = climate::CLIMATE_SWING_BOTH;
        break;
      case MIRAGE_SWING_VERTICAL:
        this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
        break;
      case MIRAGE_SWING_HORIZONTAL:
        this->swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
        break;
      case MIRAGE_SWING_OFF:
        this->swing_mode = climate::CLIMATE_SWING_OFF;
      default:
        break;
      }

      // Mode
      if (data_decoded.data[5] == MIRAGE_POWER_OFF)
      {
        this->mode = climate::CLIMATE_MODE_OFF;
      }
      else
      {
        auto mode = data_decoded.data[4] & 0x70;
        ESP_LOGV(TAG, "Mode: %02X", mode);
        switch (mode)
        {
        case MIRAGE_COOL:
          this->mode = climate::CLIMATE_MODE_COOL;
          break;
        case MIRAGE_DRY:
          this->mode = climate::CLIMATE_MODE_DRY;
          break;
        case MIRAGE_FAN:
          this->mode = climate::CLIMATE_MODE_FAN_ONLY;
          break;
        }
      }

      // Temperature
      int temp = data_decoded.data[1] - MIRAGE_TEMP_OFFSET;
      ESP_LOGVV(TAG, "Temperature Climate: %u", temp);
      this->target_temperature = temp;

      // Fan Speed
      auto fan = data_decoded.data[4] & 0x03;
      ESP_LOGVV(TAG, "Fan: %02X", fan);
      switch (fan)
      {
      case MIRAGE_FAN_HIGH:
        this->fan_mode = climate::CLIMATE_FAN_HIGH;
        break;
      case MIRAGE_FAN_MED:
        this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
        break;
      case MIRAGE_FAN_LOW:
        this->fan_mode = climate::CLIMATE_FAN_LOW;
        break;
      case MIRAGE_FAN_AUTO:
      default:
        this->fan_mode = climate::CLIMATE_FAN_AUTO;
        break;
      }

      this->publish_state();
      return true;
    }

  } // namespace mirage
} // namespace esphome