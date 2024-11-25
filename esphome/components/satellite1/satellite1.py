import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins

from esphome.components.spi import SPIDevice, register_spi_device, spi_device_schema

from esphome.const import CONF_ID

namespace = cg.esphome_ns.namespace("satellite1")
Satellite1 = namespace.class_("Satellite1", SPIDevice, cg.Component)
Satellite1SPIService = namespace.class_("Satellite1SPIService", cg.Parented.template(Satellite1))

CONF_SATELLITE1 = "satellite1"
CONF_XMOS_RST_PIN = "xmos_rst_pin"


SAT1_CONFIG_SCHEMA = (
     cv.Schema({
        cv.GenerateID(): cv.declare_id(Satellite1),
        cv.Optional(CONF_XMOS_RST_PIN, default="GPIO12"): pins.gpio_output_pin_schema,
    }).extend(spi_device_schema(True, "1Hz"))
)


async def register_satellite1(config) :
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await register_spi_device(var, config)
    
    rst_pin = await cg.gpio_pin_expression(config[CONF_XMOS_RST_PIN])
    cg.add(var.set_xmos_rst_pin(rst_pin))
    return var
