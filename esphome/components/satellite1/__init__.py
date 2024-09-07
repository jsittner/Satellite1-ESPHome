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

DEPENDENCIES = ["spi"]
CODEOWNERS = ["@gnumpi"]

satellite1_ns = cg.esphome_ns.namespace("satellite1")
Satellite1 = satellite1_ns.class_("Satellite1", SPIDevice, cg.Component )

Satellite1SPIService = satellite1_ns.class_("SatelliteSPIService", cg.Parented.template(Satellite1) )

Satellite1GPIOPin = satellite1_ns.class_("Satellite1GPIOPin", cg.GPIOPin)





XMOSPort = satellite1_ns.enum("XMOSPort", is_class=True)
XMOS_PORT = {
    0: XMOSPort.PORT_A, 
    1: XMOSPort.PORT_B,
"INPUT_A" : XMOSPort.PORT_B,
"OUTPUT_A" : XMOSPort.PORT_A      
}


CONFIG_SCHEMA = (
    cv.Schema({
        cv.GenerateID(): cv.declare_id(Satellite1)
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

# TODO: validate that all pins of a port are set to the same pin mode




async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await register_spi_device(var, config)
    return var


@pins.PIN_SCHEMA_REGISTRY.register(CONF_SATELLITE1, PIN_SCHEMA)
async def satellite1_pin_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    parent = await cg.get_variable(config[CONF_SATELLITE1])
    cg.add(var.set_parent(parent))

    cg.add(var.set_pin(config[CONF_PORT], config[CONF_PIN]))
    cg.add(var.set_inverted(config[CONF_INVERTED]))
    port_mode = {
        CONF_INPUT : config[CONF_PORT] in ["INPUT_A"],
        CONF_OUTPUT: config[CONF_PORT] in ["OUTPUT_A"]
    }
    cg.add(var.set_flags(pins.gpio_flags_expr(port_mode)))
    

    return var
