import esphome.codegen as cg
from esphome.components import audio, microphone
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@shivela"]
DEPENDENCIES = []

SerialVoiceMic = cg.global_ns.class_(
    "SerialVoiceMic", microphone.Microphone, cg.Component
)

def set_limits(config):
    config[audio.CONF_MIN_BITS_PER_SAMPLE] = 16
    config[audio.CONF_MAX_BITS_PER_SAMPLE] = 16
    config[audio.CONF_MIN_CHANNELS] = 1
    config[audio.CONF_MAX_CHANNELS] = 1
    config[audio.CONF_MIN_SAMPLE_RATE] = 16000
    config[audio.CONF_MAX_SAMPLE_RATE] = 16000
    return config

CONFIG_SCHEMA = cv.All(
    microphone.MICROPHONE_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(SerialVoiceMic),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    set_limits,
)

async def to_code(config):
    cg.add_global(cg.RawStatement('#include "serial_voice_mic.h"'))
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await microphone.register_microphone(var, config)
