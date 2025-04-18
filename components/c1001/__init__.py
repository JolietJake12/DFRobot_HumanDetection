import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import (
    CONF_ID,
    CONF_UPDATE_INTERVAL,
    UNIT_EMPTY,
    UNIT_BEATS_PER_MINUTE,
    STATE_CLASS_MEASUREMENT,
)

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["sensor", "binary_sensor"]
MULTI_CONF = True

c1001_ns = cg.esphome_ns.namespace("c1001")
C1001Component = c1001_ns.class_("C1001Component", cg.PollingComponent, uart.UARTDevice)

CONF_C1001_ID = "c1001_id"
CONF_RESPIRATION_RATE = "respiration_rate"
CONF_HEART_RATE = "heart_rate"
CONF_PRESENCE = "presence"
CONF_MOVEMENT = "movement"
CONF_PERSON_DETECTED = "person_detected"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(C1001Component),
            cv.Optional(CONF_UPDATE_INTERVAL, default="5s"): cv.update_interval,
        }
    )
    .extend(cv.polling_component_schema("5s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    
