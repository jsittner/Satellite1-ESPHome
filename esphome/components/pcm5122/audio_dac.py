import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome import automation
from esphome.components.audio_dac import AudioDac, audio_dac_ns
from esphome.const import CONF_ID, CONF_MODE

CODEOWNERS = ["@gnumpi"]
DEPENDENCIES = ["i2c"]

pcm5122_ns = cg.esphome_ns.namespace("pcm5122")
pcm5122 = pcm5122_ns.class_("PCM5122", AudioDac, cg.Component, i2c.I2CDevice)
pcmPCM5122_GPIO = pcm5122_ns.class_("PCMGPIOPin", cg.GPIOPin, cg.Parented.template(pcm5122) )

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(pcm5122),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x18))
)

def validate_mode(value):
    if not (value[CONF_INPUT] or value[CONF_OUTPUT]):
        raise cv.Invalid("Mode must be either input or output")
    if value[CONF_INPUT] and value[CONF_OUTPUT]:
        raise cv.Invalid("Mode must be either input or output")
    return value

PIN_SCHEMA = cv.All(
    {
        cv.GenerateID(): cv.declare_id(PCMGPIOPin),
        cv.Required(CONF_PCM5122): cv.use_id(PCM5122),
        cv.Required(CONF_PIN): cv.int_range(min=0, max=7),
        cv.Optional(CONF_MODE, default=CONF_OUTPUT): cv.All(
            {
                cv.Optional(CONF_INPUT, default=False): cv.boolean,
                cv.Optional(CONF_OUTPUT, default=False): cv.boolean,
            },
            validate_mode,
        ),
        cv.Optional(CONF_INVERTED, default=False): cv.boolean,
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
