import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import spi
from esphome.const import CONF_ID

DEPENDENCIES = ["esp32", "spi"]
CODEOWNERS = ["@gnumpi"]


fph_xmos_ns = cg.esphome_ns.namespace("fph_xmos")
xmos_flasher = fph_xmos_ns.class_("XMOSFlasher", cg.Component, spi.SPIDevice)



CONFIG_SCHEMA = spi.spi_device_schema(True, "8MHz").extend(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(xmos_flasher),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)
