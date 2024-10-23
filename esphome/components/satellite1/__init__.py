import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins

from esphome.components.spi import SPIDevice, register_spi_device, spi_device_schema

from esphome.const import (
    CONF_ID,
    CONF_MODE,
    CONF_PIN,
    CONF_PORT,
    CONF_OUTPUT,
    CONF_INPUT,
    CONF_INVERTED
)

CONF_SATELLITE1 = "satellite1"
CONF_XMOS_RST_PIN = "xmos_rst_pin"
CONF_FLASH_SW_PIN = "flash_sw_pin"

DEPENDENCIES = ["spi"]
CODEOWNERS = ["@gnumpi"]

satellite1_ns = cg.esphome_ns.namespace("satellite1")
Satellite1 = satellite1_ns.class_("Satellite1", SPIDevice, cg.Component )

Satellite1SPIService = satellite1_ns.class_("Satellite1SPIService", cg.Parented.template(Satellite1) )

Satellite1GPIOPin = satellite1_ns.class_("Satellite1GPIOPin", cg.GPIOPin, Satellite1SPIService, cg.Parented.template(Satellite1) )





XMOSPort = satellite1_ns.enum("XMOSPort", is_class=True)
XMOS_PORT = {
"INPUT_A" : XMOSPort.INPUT_A,
"INPUT_B" : XMOSPort.INPUT_B,
"OUTPUT_A" : XMOSPort.OUTPUT_A      
}


CONFIG_SCHEMA = (
    cv.Schema({
        cv.GenerateID(): cv.declare_id(Satellite1),
        cv.Optional(CONF_XMOS_RST_PIN, default="GPIO12"): pins.gpio_output_pin_schema,
        cv.Optional(CONF_FLASH_SW_PIN, default="GPIO14"): pins.gpio_output_pin_schema,

    }).extend(spi_device_schema(True, "1Hz"))
)


def validate_mode(value):
    if not (value[CONF_INPUT] or value[CONF_OUTPUT]):
        raise cv.Invalid("Mode must be either input or output")
    if value[CONF_INPUT] and value[CONF_OUTPUT]:
        raise cv.Invalid("Mode must be either input or output")
    return value


PIN_SCHEMA = cv.All(
    {
        cv.GenerateID(): cv.declare_id(Satellite1GPIOPin),
        cv.Required(CONF_SATELLITE1): cv.use_id(Satellite1),
        cv.Required(CONF_PORT): cv.enum(XMOS_PORT),
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


# Satellite1
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await register_spi_device(var, config)
    
    rst_pin = await cg.gpio_pin_expression(config[CONF_XMOS_RST_PIN])
    cg.add(var.set_xmos_rst_pin(rst_pin))
    sw_pin = await cg.gpio_pin_expression(config[CONF_FLASH_SW_PIN])
    cg.add(var.set_flash_sw_pin(sw_pin))
    return var


@pins.PIN_SCHEMA_REGISTRY.register(CONF_SATELLITE1, PIN_SCHEMA)
async def satellite1_pin_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_parented(var, config[CONF_SATELLITE1])

    cg.add(var.set_pin(config[CONF_PORT], config[CONF_PIN]))
    cg.add(var.set_inverted(config[CONF_INVERTED]))
    port_mode = {
        CONF_INPUT : config[CONF_PORT] in ["INPUT_A","INPUT_B"],
        CONF_OUTPUT: config[CONF_PORT] in ["OUTPUT_A"]
    }
    cg.add(var.set_flags(pins.gpio_flags_expr(port_mode)))
    

    return var
