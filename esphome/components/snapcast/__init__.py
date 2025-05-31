import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID
from esphome.core import CORE


DEPENDENCIES = ["network", "audio"]

def AUTO_LOAD():
    if CORE.is_esp8266 or CORE.is_libretiny:
        return ["async_tcp", "json", "socket"]
    return ["json", "socket"]

snapcast_ns = cg.esphome_ns.namespace("snapcast")
SnapcastClient = snapcast_ns.class_("SnapcastClient", cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(SnapcastClient),
})

async def to_code(config):
    pass
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)    
