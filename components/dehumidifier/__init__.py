import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.components import uart, binary_sensor, sensor, switch, number, select

from esphome.const import *

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["binary_sensor", "sensor", "switch", "number", "select"]


dehumidifier_ns = cg.esphome_ns.namespace("dehumidifier")
DehumidifierComponent = dehumidifier_ns.class_("Dehumidifier", cg.Component, uart.UARTDevice)

# helper classes
DehumidifierSwitch = dehumidifier_ns.class_("DehumidifierSwitch", switch.Switch)
DehumidifierSelect = dehumidifier_ns.class_("DehumidifierSelect", select.Select)
DehumidifierNumber = dehumidifier_ns.class_("DehumidifierNumber", number.Number)


CONF_POWER_SWITCH = "power"
CONF_FAN_HIGH = "fan_high"
CONF_MODE = "mode"
CONF_TARGET_HUMIDITY = "target_humidity"

CONF_TEMPERATURE = "temperature"
CONF_HUMIDITY = "humidity"
CONF_COMPRESSOR = "compressor"
CONF_TANK_FULL = "tank_full"

CONFIG_SCHEMA = (
    cv.Schema({
        cv.GenerateID(): cv.declare_id(DehumidifierComponent),
        cv.Optional(CONF_TEMPERATURE, {"name": "Temperature"}): 
            sensor.sensor_schema(device_class=DEVICE_CLASS_TEMPERATURE,unit_of_measurement=UNIT_CELSIUS,accuracy_decimals=0,state_class=STATE_CLASS_MEASUREMENT),
        cv.Optional(CONF_HUMIDITY, {"name": "Humidity"}): 
            sensor.sensor_schema(device_class=DEVICE_CLASS_HUMIDITY,unit_of_measurement=UNIT_PERCENT,accuracy_decimals=0,state_class=STATE_CLASS_MEASUREMENT),
        cv.Optional(CONF_COMPRESSOR, {"name": "Compressor"}): binary_sensor.binary_sensor_schema(),
        cv.Optional(CONF_TANK_FULL, {"name": "Tank Full"}): binary_sensor.binary_sensor_schema(),

        cv.Optional(CONF_POWER_SWITCH, {"name": "Power"}): 
            switch.switch_schema(DehumidifierSwitch),
        cv.Optional(CONF_FAN_HIGH, {"name": "Fan High"}): 
            switch.switch_schema(DehumidifierSwitch),
        cv.Optional(CONF_MODE, {"name": "Mode"}): 
            select.select_schema(DehumidifierSelect),
        cv.Optional(CONF_TARGET_HUMIDITY, {"name": "Target Humidity"}): 
            number.number_schema(DehumidifierNumber, unit_of_measurement=UNIT_PERCENT),
    })
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)


    if CONF_TEMPERATURE in config:
        temp_sens = await sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature_sensor(temp_sens))
    
    if CONF_HUMIDITY in config:
        humi_sens = await sensor.new_sensor(config[CONF_HUMIDITY])
        cg.add(var.set_humidity_sensor(humi_sens))

    if CONF_COMPRESSOR in config:
        comp_sens = await binary_sensor.new_binary_sensor(config[CONF_COMPRESSOR])
        cg.add(var.set_compressor_sensor(comp_sens))

    if CONF_TANK_FULL in config:
        tank_sens = await binary_sensor.new_binary_sensor(config[CONF_TANK_FULL])
        cg.add(var.set_tank_full_sensor(tank_sens))


    if CONF_POWER_SWITCH in config:
        power_sw = await switch.new_switch(config[CONF_POWER_SWITCH])
        cg.add(var.set_power_switch(power_sw))
        cg.add(power_sw.set_parent(var))

    if CONF_FAN_HIGH in config:
        fan_sw = await switch.new_switch(config[CONF_FAN_HIGH])
        cg.add(var.set_fan_high_switch(fan_sw))
        cg.add(fan_sw.set_parent(var))

    if CONF_MODE in config:
        mode_sel = await select.new_select(config[CONF_MODE], options=["Cold", "Auto", "Humidity"])
        cg.add(var.set_mode_select(mode_sel))
        cg.add(mode_sel.set_parent(var))
        
    if CONF_TARGET_HUMIDITY in config:
        target_num = await number.new_number(config[CONF_TARGET_HUMIDITY], min_value=30, max_value=90, step=5)
        cg.add(var.set_target_humidity_number(target_num))
        cg.add(target_num.set_parent(var))
