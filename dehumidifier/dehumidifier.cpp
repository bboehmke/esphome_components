#include "esphome/core/log.h"
#include "dehumidifier.h"

namespace esphome::dehumidifier {

static const char *TAG = "dehumidifier";

void Dehumidifier::setup() {
    // nothing to setup
}

void Dehumidifier::send_update() {
    uint8_t cmd[10];

    // header
    cmd[0] = 0xBB;
    cmd[1] = 0x51;

    cmd[4] = 0x00; // reserved
    cmd[6] = 0x00; // timer -> unused
    cmd[7] = 0x00; // reserved
    cmd[8] = 0x00; // reserved

    // Power
    if (this->power_switch != nullptr && this->power_switch->state) {
        // Power is ON
        cmd[2] = 0x80;
    } else {
        // Power is OFF
        cmd[2] = 0x00;
    }

    // mode and target humidity
    size_t mode_index = (this->mode_select != nullptr) ? this->mode_select->active_index().value_or(0) : 0;
    int target_humidity = (this->target_humidity_number != nullptr) ? static_cast<int>(this->target_humidity_number->state) : 50;  

    if (mode_index == 0) { // Cold
        cmd[3] = 0x01;
        cmd[5] = 0x01;

    } else if (mode_index == 1) { // Auto
        cmd[3] = 0x00;
        cmd[5] = 0x00;

    } else { // Humidity
        cmd[3] = 0x02;
        // Target humidity setting
        if (target_humidity < 30) target_humidity = 30;
        if (target_humidity > 90) target_humidity = 90;
        cmd[5] = static_cast<uint8_t>((target_humidity-30)/5)+2; 
    }

    // Fan speed
    if (this->fan_high_switch != nullptr && this->fan_high_switch->state) {
        cmd[3] |= 0x20;
    }

    // unknown bits, always set
    cmd[3] |= 0x10;

    // Calculate checksum
    uint16_t sum = 0;
    for (size_t i = 2; i <= 8; ++i) {
        sum += cmd[i];
    }
    cmd[9] = sum & 0xFF;

    // Send command
    char hexbuf[3 * PACKET_SIZE + 1];
    size_t pos = 0;
    for (size_t i = 0; i < PACKET_SIZE; ++i) {
    pos += snprintf(hexbuf + pos, sizeof(hexbuf) - pos, "%02X%s",
                    this->buffer[i], (i + 1 < PACKET_SIZE) ? " " : "");
    }

    ESP_LOGD(TAG, "Sending command: %s", hexbuf);
    this->write_array(cmd, sizeof(cmd));
}

void Dehumidifier::handle_packet() {
    if (this->temperature_sensor != nullptr) {
        uint8_t temp_byte = this->buffer[10];
        int temperature = static_cast<int>(temp_byte);
        this->temperature_sensor->publish_state(temperature);
    }
    if (this->humidity_sensor != nullptr) {
        uint8_t humidity_byte = this->buffer[4];
        int humidity = static_cast<int>(humidity_byte);
        this->humidity_sensor->publish_state(humidity);
    }
    if (this->compressor_sensor != nullptr) {
        uint8_t comp_byte = this->buffer[7];
        bool comp_on = (comp_byte & 0x80) != 0;
        this->compressor_sensor->publish_state(comp_on);
    }
    if (this->tank_full_sensor != nullptr) {
        uint8_t tank_byte = this->buffer[9];
        bool tank_full = (tank_byte & 0x04) != 0;
        this->tank_full_sensor->publish_state(tank_full);
    }

    if (this->power_switch != nullptr) {
        uint8_t power_byte = this->buffer[2];
        bool power_on = (power_byte & 0x80) != 0;
        this->power_switch->publish_state(power_on);
    }
    if (this->fan_high_switch != nullptr) {
        uint8_t fan_byte = this->buffer[3];
        bool fan_high = (fan_byte & 0x20) != 0;
        this->fan_high_switch->publish_state(fan_high);
    }
    if (this->mode_select != nullptr) {
        uint8_t mode_byte = this->buffer[3] & 0x03;
        size_t mode_index = 0;
        if (mode_byte == 0x01) {
            mode_index = 0; // Cold
        } else if (mode_byte == 0x00) {
            mode_index = 1; // Auto
        } else {
            mode_index = 2; // Humidity
        }
        this->mode_select->publish_state(mode_index);
    }
    if (this->target_humidity_number != nullptr) {
        uint8_t target_byte = this->buffer[5];
        int target_humidity = 50; // default
        if (target_byte >= 2) {
            target_humidity = 30 + (static_cast<int>(target_byte) - 2) * 5;
        }
        this->target_humidity_number->publish_state(static_cast<float>(target_humidity));
    }
}

void Dehumidifier::loop() {
    // read all data that is currently available
    while (this->available()) {
        // read one byte
        int read_int = this->read();
        if (read_int < 0) {
            continue;
        }
        uint8_t c = static_cast<uint8_t>(read_int);

        // each packet starts with 0xBB 0x51
        // so we look for 0xBB first, if found we look for 0x51 next
        // only if both found we start collecting the full packet
        switch (this->state) {
            case ParseState::WAIT_HEADER1:
                if (c == 0xBB) {
                    this->state = ParseState::WAIT_HEADER2;
                }
                break;

            case ParseState::WAIT_HEADER2:
                if (c == 0x51) {
                    // header complete -> add to buffer and start collecting rest of packet
                    this->buf_index = 0;
                    this->buffer[this->buf_index++] = 0xBB;
                    this->buffer[this->buf_index++] = 0x51;
                    this->state = ParseState::COLLECT;

                } else if (c == 0xBB) {
                    // got BB again, stay in this state -> do nothing
                } else {
                    // invalid, go back to waiting for first header byte
                    this->state = ParseState::WAIT_HEADER1;
                }
                break;

            case ParseState::COLLECT:
                // store next byte to buffer
                if (this->buf_index < PACKET_SIZE) {
                    this->buffer[this->buf_index++] = c;
                }

                // check if complete packet collected
                if (this->buf_index >= PACKET_SIZE) {
                    // calculate checksum of packet data
                    uint16_t sum = 0;
                    for (size_t i = 2; i < PACKET_SIZE - 1; ++i) {
                        sum += this->buffer[i];
                    }
                    uint8_t checksum_calc = sum & 0xFF;

                    // create hex dump of full packet for logging
                    char hexbuf[3 * PACKET_SIZE + 1];
                    size_t pos = 0;
                    for (size_t i = 0; i < PACKET_SIZE; ++i) {
                        pos += snprintf(hexbuf + pos, sizeof(hexbuf) - pos, "%02X%s",
                                        this->buffer[i], (i + 1 < PACKET_SIZE) ? " " : "");
                    }

                    // validate checksum
                    if (checksum_calc == this->buffer[PACKET_SIZE - 1]) {
                        ESP_LOGD(TAG, "Valid packet received: %s", hexbuf);

                        // valid checksum -> handle packet
                        this->handle_packet();
                    } else {
                        ESP_LOGW(TAG, "Checksum mismatch: computed=0x%02X packet=0x%02X. Full packet: %s",
                                checksum_calc, this->buffer[PACKET_SIZE - 1], hexbuf);
                    }

                    // processing done -> go back to waiting for first header byte
                    this->state = ParseState::WAIT_HEADER1;
                    this->buf_index = 0;
                }
                break;
        }
    }
}

void Dehumidifier::dump_config(){
    // no config
}

void DehumidifierSwitch::write_state(bool state) {
    this->publish_state(state);

    if (this->parent != nullptr) {
        this->parent->send_update();
    }
}

void DehumidifierSelect::control(size_t index) {
    this->publish_state(index);

    if (this->parent != nullptr) {
        this->parent->send_update();
    }
}

void DehumidifierNumber::control(float value) {
    this->publish_state(value);

    if (this->parent != nullptr) {
        this->parent->send_update();
    }
}

}
