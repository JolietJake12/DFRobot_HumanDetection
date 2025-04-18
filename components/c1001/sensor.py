import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    STATE_CLASS_MEASUREMENT,
    UNIT_EMPTY,
    UNIT_BEATS_PER_MINUTE,
    UNIT_MINUTE,
    UNIT_PERCENT,
)
from esphome.const import CONF_ID
from . import CONF_RESPIRATION_RATE, CONF_HEART_RATE, CONF_PRESENCE, CONF_MOVEMENT, c1001_ns, C1001Component, CONF_C1001_ID

# Additional sleep metrics
CONF_IN_BED = "in_bed"
CONF_SLEEP_STATE = "sleep_state"
CONF_SLEEP_QUALITY = "sleep_quality"
CONF_SLEEP_QUALITY_RATING = "sleep_quality_rating"
CONF_AWAKE_DURATION = "awake_duration"
CONF_LIGHT_SLEEP_DURATION = "light_sleep_duration"
CONF_DEEP_SLEEP_DURATION = "deep_sleep_duration"
CONF_AVERAGE_RESPIRATION = "average_respiration"
CONF_AVERAGE_HEART_RATE = "average_heart_rate"
CONF_TURNOVER_COUNT = "turnover_count"
CONF_LARGE_BODY_MOVEMENT = "large_body_movement"
CONF_MINOR_BODY_MOVEMENT = "minor_body_movement" 
CONF_APNEA_EVENTS = "apnea_events"
CONF_SLEEP_SCORE = "sleep_score"

# CONF_C1001_ID already imported from __init__.py

# Sleep state enum values for user-friendly display
SLEEP_STATES = {
    0: "Deep Sleep",
    1: "Light Sleep",
    2: "Awake",
    3: "None",
}

# Sleep quality rating enum values for user-friendly display
SLEEP_QUALITY_RATINGS = {
    0: "None",
    1: "Good",
    2: "Average",
    3: "Poor",
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_C1001_ID): cv.use_id(C1001Component),
        
        # Basic sensors
        cv.Optional(CONF_RESPIRATION_RATE): sensor.sensor_schema(
            unit_of_measurement=UNIT_BEATS_PER_MINUTE,
            accuracy_decimals=1,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:lungs",
        ),
        cv.Optional(CONF_HEART_RATE): sensor.sensor_schema(
            unit_of_measurement=UNIT_BEATS_PER_MINUTE,
            accuracy_decimals=1,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:heart-pulse",
        ),
        cv.Optional(CONF_PRESENCE): sensor.sensor_schema(
            unit_of_measurement=UNIT_EMPTY,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:human-greeting",
        ),
        cv.Optional(CONF_MOVEMENT): sensor.sensor_schema(
            unit_of_measurement=UNIT_EMPTY,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:motion-sensor",
        ),
        
        # Sleep metrics
        cv.Optional(CONF_IN_BED): sensor.sensor_schema(
            unit_of_measurement=UNIT_EMPTY,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:bed",
        ),
        cv.Optional(CONF_SLEEP_STATE): sensor.sensor_schema(
            unit_of_measurement=UNIT_EMPTY,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:sleep",
        ),
        cv.Optional(CONF_SLEEP_QUALITY): sensor.sensor_schema(
            unit_of_measurement=UNIT_EMPTY,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:star",
        ),
        cv.Optional(CONF_SLEEP_QUALITY_RATING): sensor.sensor_schema(
            unit_of_measurement=UNIT_EMPTY,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:star-half-full",
        ),
        cv.Optional(CONF_AWAKE_DURATION): sensor.sensor_schema(
            unit_of_measurement=UNIT_MINUTE,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:sleep-off",
        ),
        cv.Optional(CONF_LIGHT_SLEEP_DURATION): sensor.sensor_schema(
            unit_of_measurement=UNIT_MINUTE,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:sleep",
        ),
        cv.Optional(CONF_DEEP_SLEEP_DURATION): sensor.sensor_schema(
            unit_of_measurement=UNIT_MINUTE,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:power-sleep",
        ),
        cv.Optional(CONF_AVERAGE_RESPIRATION): sensor.sensor_schema(
            unit_of_measurement=UNIT_BEATS_PER_MINUTE,
            accuracy_decimals=1,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:lungs",
        ),
        cv.Optional(CONF_AVERAGE_HEART_RATE): sensor.sensor_schema(
            unit_of_measurement=UNIT_BEATS_PER_MINUTE,
            accuracy_decimals=1,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:heart-pulse",
        ),
        cv.Optional(CONF_TURNOVER_COUNT): sensor.sensor_schema(
            unit_of_measurement=UNIT_EMPTY,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:rotate-3d-variant",
        ),
        cv.Optional(CONF_LARGE_BODY_MOVEMENT): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:human-handsup",
        ),
        cv.Optional(CONF_MINOR_BODY_MOVEMENT): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:human",
        ),
        cv.Optional(CONF_APNEA_EVENTS): sensor.sensor_schema(
            unit_of_measurement=UNIT_EMPTY,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:lungs-off",
        ),
        cv.Optional(CONF_SLEEP_SCORE): sensor.sensor_schema(
            unit_of_measurement=UNIT_EMPTY,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:medal",
        ),
    }
)


async def to_code(config):
    paren = await cg.get_variable(config[CONF_C1001_ID])

    # Basic sensors
    if CONF_RESPIRATION_RATE in config:
        conf = config[CONF_RESPIRATION_RATE]
        sens = await sensor.new_sensor(conf)
        cg.add(paren.set_respiration_sensor(sens))

    if CONF_HEART_RATE in config:
        conf = config[CONF_HEART_RATE]
        sens = await sensor.new_sensor(conf)
        cg.add(paren.set_heart_rate_sensor(sens))

    if CONF_PRESENCE in config:
        conf = config[CONF_PRESENCE]
        sens = await sensor.new_sensor(conf)
        cg.add(paren.set_presence_sensor(sens))

    if CONF_MOVEMENT in config:
        conf = config[CONF_MOVEMENT]
        sens = await sensor.new_sensor(conf)
        cg.add(paren.set_movement_sensor(sens))
        
    # Sleep metrics
    if CONF_IN_BED in config:
        conf = config[CONF_IN_BED]
        sens = await sensor.new_sensor(conf)
        cg.add(paren.set_in_bed_sensor(sens))
        
    if CONF_SLEEP_STATE in config:
        conf = config[CONF_SLEEP_STATE]
        sens = await sensor.new_sensor(conf)
        cg.add(paren.set_sleep_state_sensor(sens))
        
    if CONF_SLEEP_QUALITY in config:
        conf = config[CONF_SLEEP_QUALITY]
        sens = await sensor.new_sensor(conf)
        cg.add(paren.set_sleep_quality_sensor(sens))
        
    if CONF_SLEEP_QUALITY_RATING in config:
        conf = config[CONF_SLEEP_QUALITY_RATING]
        sens = await sensor.new_sensor(conf)
        cg.add(paren.set_sleep_quality_rating_sensor(sens))
        
    if CONF_AWAKE_DURATION in config:
        conf = config[CONF_AWAKE_DURATION]
        sens = await sensor.new_sensor(conf)
        cg.add(paren.set_awake_duration_sensor(sens))
        
    if CONF_LIGHT_SLEEP_DURATION in config:
        conf = config[CONF_LIGHT_SLEEP_DURATION]
        sens = await sensor.new_sensor(conf)
        cg.add(paren.set_light_sleep_duration_sensor(sens))
        
    if CONF_DEEP_SLEEP_DURATION in config:
        conf = config[CONF_DEEP_SLEEP_DURATION]
        sens = await sensor.new_sensor(conf)
        cg.add(paren.set_deep_sleep_duration_sensor(sens))
        
    if CONF_AVERAGE_RESPIRATION in config:
        conf = config[CONF_AVERAGE_RESPIRATION]
        sens = await sensor.new_sensor(conf)
        cg.add(paren.set_average_respiration_sensor(sens))
        
    if CONF_AVERAGE_HEART_RATE in config:
        conf = config[CONF_AVERAGE_HEART_RATE]
        sens = await sensor.new_sensor(conf)
        cg.add(paren.set_average_heart_rate_sensor(sens))
        
    if CONF_TURNOVER_COUNT in config:
        conf = config[CONF_TURNOVER_COUNT]
        sens = await sensor.new_sensor(conf)
        cg.add(paren.set_turnover_count_sensor(sens))
        
    if CONF_LARGE_BODY_MOVEMENT in config:
        conf = config[CONF_LARGE_BODY_MOVEMENT]
        sens = await sensor.new_sensor(conf)
        cg.add(paren.set_large_body_movement_sensor(sens))
        
    if CONF_MINOR_BODY_MOVEMENT in config:
        conf = config[CONF_MINOR_BODY_MOVEMENT]
        sens = await sensor.new_sensor(conf)
        cg.add(paren.set_minor_body_movement_sensor(sens))
        
    if CONF_APNEA_EVENTS in config:
        conf = config[CONF_APNEA_EVENTS]
        sens = await sensor.new_sensor(conf)
        cg.add(paren.set_apnea_events_sensor(sens))
        
    if CONF_SLEEP_SCORE in config:
        conf = config[CONF_SLEEP_SCORE]
        sens = await sensor.new_sensor(conf)
        cg.add(paren.set_sleep_score_sensor(sens))