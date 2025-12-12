#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/select/select.h"
#include "esphome/components/number/number.h"


namespace esphome::dehumidifier {

class Dehumidifier : public uart::UARTDevice, public Component {
    public:
        void setup() override;
        void loop() override;
        void dump_config() override;

        // Send updated settings to the dehumidifier
        void send_update();


        // Setters for sensors and controls
        void set_temperature_sensor(sensor::Sensor *sensor) {
            this->temperature_sensor = sensor;
        }
        void set_humidity_sensor(sensor::Sensor *sensor) {
            this->humidity_sensor = sensor;
        }
        void set_compressor_sensor(binary_sensor::BinarySensor *sensor) {
            this->compressor_sensor = sensor;
        }
        void set_tank_full_sensor(binary_sensor::BinarySensor *sensor) {
            this->tank_full_sensor = sensor;
        }

        void set_power_switch(switch_::Switch *sw) {
            this->power_switch = sw;
        }
        void set_fan_high_switch(switch_::Switch *sw) {
            this->fan_high_switch = sw;
        }
        void set_mode_select(select::Select *sel) {
            this->mode_select = sel;
        }
        void set_target_humidity_number(number::Number *num) {
            this->target_humidity_number = num;
        }

    private:
        // Packet parsing buffer and packet size
        static constexpr size_t PACKET_SIZE = 15;
        uint8_t buffer[PACKET_SIZE];
        size_t buf_index = 0;

        // Parsing state machine
        enum class ParseState : uint8_t { WAIT_HEADER1, WAIT_HEADER2, COLLECT };
        ParseState state = ParseState::WAIT_HEADER1;

        // Handle a complete and validated packet
        void handle_packet();

        // Sensors and controls
        sensor::Sensor *temperature_sensor{nullptr};
        sensor::Sensor *humidity_sensor{nullptr};
        binary_sensor::BinarySensor *compressor_sensor{nullptr};
        binary_sensor::BinarySensor *tank_full_sensor{nullptr};

        switch_::Switch *power_switch{nullptr};
        switch_::Switch *fan_high_switch{nullptr};
        select::Select *mode_select{nullptr};
        number::Number *target_humidity_number{nullptr};
};

// helper classes for controls used by Dehumidifier

class DehumidifierSwitch : public switch_::Switch {
    public:
        void write_state(bool state) override;
        void set_parent(Dehumidifier *parent) { this->parent = parent; }

    private:
        Dehumidifier *parent{nullptr};
};

class DehumidifierSelect : public select::Select {
    public:
        void control(size_t index) override;
        void set_parent(Dehumidifier *parent) { this->parent = parent; }

    private:
        Dehumidifier *parent{nullptr};
};

class DehumidifierNumber : public number::Number {
    public:
        void control(float value) override;
        void set_parent(Dehumidifier *parent) { this->parent = parent; }

    private:
        Dehumidifier *parent{nullptr};
};

}
