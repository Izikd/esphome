/*
 * sonoff_tx_ultimate.cpp - Sonoff TX Ultimate Touch support for ESPHome
 *
 * Sonoff TX Ultimate uses touch panel controller (IC CA51F353S3) which 
 * communicates over UART.
 * Communication looks somewhat similiar to Sonoff D1 (implemented here in sonoff_d1.cpp).
 *
 * Knowledge was greatly referenced from:
 * - https://github.com/blakadder/tx-ultimate
 * - https://github.com/SmartHome-yourself/sonoff-tx-ultimate-for-esphome
 */

#include "sonoff_tx_ultimate.h"

namespace esphome {
namespace sonoff_tx_ultimate {

struct TouchEventHeader {
  union {
    struct {
      uint8_t magic[2]; /* 0xAA, 0x55 */
      uint8_t version; /* 0x1 */
      uint8_t opcode; /* 0x2 - Touch event */
    };
    uint8_t const_prefix[4];
  };
  uint8_t data_len;
} __attribute__((packed));

struct TouchEventData {
  union {
    /* 
     * Release
     */
    struct {
      uint8_t ch;
    } len_1;
    /*
     * Press / Release
     */
    struct {
      /* 
       * Sent on "release" on "mini-swipe" (i.e. swipe between close channels),
       * and includes the channel where the press was released.
       * We won't use this value other than determining 'press' vs. 'release' event.
       * When not 0, we will convert it to a 'release' event; otherwise it's a 'press' event.
       */
      uint8_t release_event; /* non-zero on release event */
      uint8_t ch;
    } len_2;
    /*
     * Swipe
     */
    struct {
      uint8_t ch;
      /* Some obscure value that changes */
      uint8_t unused[2];
    } len_3;
  };
} __attribute__((packed));

struct TouchEventFooter {
  /* CRC-16/CCITT-FALSE - starting from the version byte, not including the footer */
  uint16_t crc16;
} __attribute__((packed));


static const char *const TAG = "sonoff_tx_ultimate";

/* These are the only expected values for these fields */
static const uint8_t HEADER_CONST_PREFIX[] = {0xAA, 0x55, 0x1, 0x2};

/* Data length defines the data type */
static const uint8_t HEADER_DATA_LEN_RELEASE = 1;
static const uint8_t HEADER_DATA_LEN_PRESS_RELEASE = 2;
static const uint8_t HEADER_DATA_LEN_SWIPE = 3;

/*
 * When a press is too long (5 sec) it times out.
 * Many refer to it as a "long press", but we implement it here
 * as a simple release.
 * ESPHome knowns to deal with long press in SW, so no need to complicate here.
 */
static const uint8_t CH_RELEASE_TIMEOUT = 0x10;

static const uint16_t TOUCH_EVENT_MAX_BYTES = sizeof(TouchEventHeader) + sizeof(TouchEventData) + sizeof(TouchEventFooter);


void SonoffTXUltimate::setup() {
  this->power_pin_->setup();
  this->power_pin_->digital_write(true);
}

/*
 * Although only header is provided, we assume that the header is followed by the data.
 * Output is in big-endian format, as RXed/TXed.
 */
static uint16_t crc16_calc(const TouchEventHeader *header)
{
  const uint8_t *data;
  uint16_t len;

  /* Starts at version */
  data = &header->version;

  /* Substract 2 bytes of 'magic' */
  len = sizeof(*header) - 2 + header->data_len;

  /* Adjust params for CRC-16/CCITT-FALSE */
  return convert_big_endian(crc16be(data, len, 0xffff, 0x1021));
}

void SonoffTXUltimate::skip_command_() {
  size_t garbage = 0;
  // Read out everything from the UART FIFO
  while (this->available()) {
    uint8_t value = this->read();
    ESP_LOGW(TAG, "[%04u] Skip %02zu: 0x%02x", this->read_count_, garbage, value);
    garbage++;
  }

  if (garbage) {
    ESP_LOGW(TAG, "[%04u] Skip %d bytes", this->read_count_, garbage);
  }
}

// This assumes some data is already available
bool SonoffTXUltimate::read_command_(TouchEvent &event) {
  uint8_t buf[TOUCH_EVENT_MAX_BYTES];
  unsigned int buf_len = 0;
  TouchEventHeader *header;
  TouchEventData *data;
  TouchEventFooter *footer;
  uint16_t crc16_actual;
  bool success = false;
  const char *event_name;

  // Read header
  if (!this->read_array((uint8_t *)&buf[buf_len], sizeof(*header))) {
    ESP_LOGW(TAG, "[%04u] RX: timeout on reading header", this->read_count_);
    goto error;
  }

  header = (TouchEventHeader *)&buf[buf_len];
  buf_len += sizeof(*header);

  ESP_LOGVV(TAG, "[%04u] Header: %s", this->read_count_, format_hex_pretty((uint8_t *)header, sizeof(*header)).c_str());

  if (memcmp(header->const_prefix, HEADER_CONST_PREFIX, sizeof(header->const_prefix)) != 0) {
    ESP_LOGW(TAG, "[%04u] RX: wrong header prefix (%s, must be %s)", this->read_count_,
             format_hex_pretty(header->const_prefix, sizeof(header->const_prefix)).c_str(),
             format_hex_pretty(HEADER_CONST_PREFIX, sizeof(HEADER_CONST_PREFIX)).c_str());
    goto error;
  }

  if (header->data_len > sizeof(data)) {
    ESP_LOGW(TAG, "[%04u] RX: Data length is unexpected (%u, max expected %u)", this->read_count_, 
             header->data_len, sizeof(data));
    goto error;
  }

  // Read data + footer
  if (!this->read_array(&buf[buf_len], header->data_len + sizeof(*footer))) {
    ESP_LOGW(TAG, "[%04u] RX: timeout on reading data + footer", this->read_count_);
    goto error;
  }

  data = (TouchEventData *)&buf[buf_len];
  buf_len += header->data_len;
  
  ESP_LOGVV(TAG, "[%04u] Data: %s", this->read_count_, format_hex_pretty((uint8_t *)data, header->data_len).c_str());

  footer = (TouchEventFooter *)&buf[buf_len];
  buf_len += sizeof(*footer);

  /*
   * Footer CRC16 footer check
   */
  crc16_actual = crc16_calc(header);
  ESP_LOGVV(TAG, "[%04u] Footer CRC = %04x; Calc CRC = %04x", this->read_count_, crc16_actual, footer->crc16);

  if (crc16_actual != footer->crc16) {
    ESP_LOGW(TAG, "[%04u] RX: Invalid CRC16", this->read_count_);
    goto error;
  }

  // Crunch data
  switch (header->data_len) {
  case HEADER_DATA_LEN_RELEASE:
    event.ch = data->len_1.ch;
    event.state = false;
    event_name = "Release (type 1)";
    break;
  case HEADER_DATA_LEN_PRESS_RELEASE:
    event.ch = data->len_2.ch;
    event.state = (data->len_2.release_event ? false : true);
    event_name = (data->len_2.release_event ? "Release (type 2)" : "Press");
    break;
  case HEADER_DATA_LEN_SWIPE:
    /* Swipe left/right event, always a release */
    event.ch = data->len_3.ch;
    event.state = false;
    event_name = "Swipe";
    break;
  default:
    goto error;
    break;
  }

  /* 
   * Removing the long press/release bit will give us the channel.
   * We're essentially converting a long release to just another release;
   * ESPHome gives the option to diff between long/short press anyway.
   */
  if (event.ch & CH_RELEASE_TIMEOUT) {
    event.ch &= ~CH_RELEASE_TIMEOUT;
  }

  ESP_LOGV(TAG, "[%04u] %s; ch=%u",
           this->read_count_, event_name, event.ch);

  success = true;
  goto done;

error:
  success = false;

done:
  this->skip_command_();
  this->read_count_++;

  return success;
}

void SonoffTXUltimate::process_command_(const TouchEvent &event) {
  bool press_and_release = false;

  if (event.ch > SonoffTXUltimateTouchBinarySensor::CH_MAX) {
    press_and_release = true;
  }

  for (auto *touch_binary_sensor : this->touch_binary_sensors_) {
    /*
     * On high channels: there is no 'press' event on these, but only 'release' event;
     * so we need to emulate it.
     * On press_only mode: we ignore the 'release' event, so we emulate both events.
     */
    if ((event.ch > SonoffTXUltimateTouchBinarySensor::CH_MAX) ||
        (touch_binary_sensor->press_only_get() && (event.state == true))) {
      touch_binary_sensor->process(event.ch, true);
      touch_binary_sensor->process(event.ch, false);
    } else {
      touch_binary_sensor->process(event.ch, event.state);
    }
  }
}

void SonoffTXUltimate::dump_config() {
  ESP_LOGCONFIG(TAG, "Sonoff TX Ultimate Touch");
  LOG_PIN("  Power Pin: ", this->power_pin_);
}

void SonoffTXUltimate::loop() {
  TouchEvent event = {};

  if (!this->available())
    return;

  if (!this->read_command_(event))
    return;

  this->process_command_(event);
}

}  // namespace sonoff_tx_ultimate
}  // namespace esphome
