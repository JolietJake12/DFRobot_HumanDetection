import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from . import c1001_ns, C1001Component, CONF_PERSON_DETECTED, CONF_C1001_ID

# Sleep binary sensors
CONF_ABNORMAL_STRUGGLE = "abnormal_struggle"
CONF_SLEEP_DISTURBANCE = "sleep_disturbance"

# CONF_C1001_ID already imported from __init__.py

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_C1001_ID): cv.use_id(C1001Component),
        cv.Optional(CONF_PERSON_DETECTED): binary_sensor.binary_sensor_schema(
            device_class="occupancy",
        ),
        cv.Optional(CONF_ABNORMAL_STRUGGLE): binary_sensor.binary_sensor_schema(
            device_class="problem",
            icon="mdi:exclamation",
        ),
        cv.Optional(CONF_SLEEP_DISTURBANCE): binary_sensor.binary_sensor_schema(
            device_class="problem",
            icon="mdi:sleep-off",
        ),
    }
)


async def to_code(config):
    paren = await cg.get_variable(config[CONF_C1001_ID])

    if CONF_PERSON_DETECTED in config:
        conf = config[CONF_PERSON_DETECTED]
        sens = await binary_sensor.new_binary_sensor(conf)
        cg.add(paren.set_person_detected_binary_sensor(sens))
        
    if CONF_ABNORMAL_STRUGGLE in config:
        conf = config[CONF_ABNORMAL_STRUGGLE]
        sens = await binary_sensor.new_binary_sensor(conf)
        cg.add(paren.set_abnormal_struggle_sensor(sens))
        
    if CONF_SLEEP_DISTURBANCE in config:
        conf = config[CONF_SLEEP_DISTURBANCE]
        sens = await binary_sensor.new_binary_sensor(conf)
        cg.add(paren.set_sleep_disturbance_sensor(sens))