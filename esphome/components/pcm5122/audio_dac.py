import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome import automation
from esphome.components.audio_dac import AudioDac, audio_dac_ns
from esphome.const import CONF_ID, CONF_MODE

CODEOWNERS = ["@gnumpi"]
DEPENDENCIES = ["i2c"]

pcm5122_ns = cg.esphome_ns.namespace("pcm5122")
PCM5122 = pcm5122_ns.class_("PCM5122", AudioDac, cg.Component, i2c.I2CDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(PCM5122),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x18))
)



async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)