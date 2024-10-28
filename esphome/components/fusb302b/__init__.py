import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv


from esphome.const import CONF_ID

CODEOWNERS = ["@gnumpi"]
DEPENDENCIES = ["i2c"]

pd_ns = cg.esphome_ns.namespace("power_delivery")
FUSB302B = pd_ns.class_("FUSB302B", cg.Component, i2c.I2CDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(FUSB302B),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x22))
)



async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)