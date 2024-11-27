import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome import automation
from esphome.components.audio_dac import AudioDac, audio_dac_ns
from esphome.const import CONF_ID, CONF_PIN

CODEOWNERS = ["@gnumpi"]
DEPENDENCIES = ["i2c"]

pcm5122_ns = cg.esphome_ns.namespace("pcm5122")
PCM5122 = pcm5122_ns.class_("PCM5122", AudioDac, cg.Component, i2c.I2CDevice)
CONF_PCM5122 = "pcm5122_id"


PCMGPIOPin = pcm5122_ns.class_( 
    "PCMGPIOPin", 
    cg.GPIOPin, 
    cg.Parented.template(PCM5122)
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(PCM5122),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x18))
)

PIN_SCHEMA = cv.All(
    {
        cv.GenerateID(): cv.declare_id(PCMGPIOPin),
        cv.Required(CONF_PCM5122): cv.use_id(PCM5122),
        cv.Required(CONF_PIN): cv.int_range(min=0, max=7),
        cv.Optional(CONF_INVERTED, default=False): cv.boolean,
    }
)


@pins.PIN_SCHEMA_REGISTRY.register(CONF_PCM5122, PIN_SCHEMA)
async def pcm5122_pin_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_parented(var, config[CONF_PCM5122])

    cg.add(var.set_pin(config[CONF_PORT], config[CONF_PIN]))
    cg.add(var.set_inverted(config[CONF_INVERTED]))

    return var

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
