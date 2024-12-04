import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation, pins

from esphome.components.spi import SPIDevice, register_spi_device, spi_device_schema

from esphome.const import CONF_ID, CONF_TRIGGER_ID

namespace = cg.esphome_ns.namespace("satellite1")
Satellite1 = namespace.class_("Satellite1", SPIDevice, cg.Component)
Satellite1SPIService = namespace.class_("Satellite1SPIService", cg.Parented.template(Satellite1))

XMOSConnectedStateTrigger = namespace.class_(
    "XMOSConnectedStateTrigger", automation.Action
)

FlashConnectedStateTrigger = namespace.class_(
    "FlashConnectedStateTrigger", automation.Action
)

XMOSNoResponseStateTrigger = namespace.class_(
    "XMOSNoResponseStateTrigger", automation.Action
)



CONF_SATELLITE1 = "satellite1"
CONF_XMOS_RST_PIN = "xmos_rst_pin"
CONF_ON_XMOS_NO_RESPONSE = "on_xmos_no_response"
CONF_ON_XMOS_CONNECTED = "on_xmos_connected"
CONF_ON_FLASH_CONNECTED = "on_flash_connected"

SAT1_CONFIG_SCHEMA = (
     cv.Schema({
        cv.GenerateID(): cv.declare_id(Satellite1),
        cv.Optional(CONF_XMOS_RST_PIN, default="GPIO12"): pins.gpio_output_pin_schema,

        cv.Optional(CONF_ON_XMOS_CONNECTED): automation.validate_automation({
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(XMOSConnectedStateTrigger),
        }),
        cv.Optional(CONF_ON_FLASH_CONNECTED): automation.validate_automation({
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(FlashConnectedStateTrigger),
        }),
        cv.Optional(CONF_ON_XMOS_NO_RESPONSE): automation.validate_automation({
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(XMOSNoResponseStateTrigger),
        }),

    }).extend(spi_device_schema(True, "1Hz"))
)


async def register_satellite1(config) :
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await register_spi_device(var, config)
    
    rst_pin = await cg.gpio_pin_expression(config[CONF_XMOS_RST_PIN])
    cg.add(var.set_xmos_rst_pin(rst_pin))
    
    for conf in config.get(CONF_ON_XMOS_CONNECTED, []):
         trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var )
         await automation.build_automation(trigger, [], conf)
    
    for conf in config.get(CONF_ON_FLASH_CONNECTED, []):
         trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var )
         await automation.build_automation(trigger, [], conf)
    
    for conf in config.get(CONF_ON_XMOS_NO_RESPONSE, []):
         trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var )
         await automation.build_automation(trigger, [], conf)
    
    return var
