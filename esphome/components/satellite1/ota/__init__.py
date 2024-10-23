import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import (
    CONF_ID,
    CONF_URL,

)
from esphome.components.http_request import (
    CONF_HTTP_REQUEST_ID,
    HttpRequestComponent,
)

from esphome.components.ota import BASE_OTA_SCHEMA, ota_to_code, OTAComponent
from esphome.core import coroutine_with_priority

from .. import (
    CONF_SATELLITE1,
    satellite1_ns,
    Satellite1, 
    Satellite1SPIService
)


CODEOWNERS = ["@gnumpi"]

AUTO_LOAD = ["md5"]
DEPENDENCIES = ["network", "http_request", "satellite1"]

CONF_MD5 = "md5"
CONF_MD5_URL = "md5_url"

SatelliteFlasher = satellite1_ns.class_("SatelliteFlasher", OTAComponent, Satellite1SPIService)
SatelliteFlasherAction = satellite1_ns.class_(
    "SatelliteFlasherAction", automation.Action
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SatelliteFlasher),
            cv.GenerateID(CONF_HTTP_REQUEST_ID): cv.use_id(HttpRequestComponent),
            cv.GenerateID(CONF_SATELLITE1): cv.use_id(Satellite1)
        }
    )
    .extend(BASE_OTA_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


@coroutine_with_priority(52.0)
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_parented(var, config[CONF_SATELLITE1])
    await ota_to_code(var, config)
    await cg.register_component(var, config)
    http_comp = await cg.get_variable(config[CONF_HTTP_REQUEST_ID])
    cg.add( var.set_http_request_component(http_comp) )



OTA_SATELLITE_FLASH_ACTION_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(SatelliteFlasher),
            cv.Optional(CONF_MD5_URL): cv.templatable(cv.url),
            cv.Optional(CONF_MD5): cv.templatable(
                cv.All(cv.string, cv.Length(min=32, max=32))
            ),
            cv.Required(CONF_URL): cv.templatable(cv.url),
        }
    ),
    cv.has_exactly_one_key(CONF_MD5, CONF_MD5_URL),
)


@automation.register_action(
    "ota.satellite1.flash",
    SatelliteFlasherAction,
    OTA_SATELLITE_FLASH_ACTION_SCHEMA,
)
async def ota_voice_kit_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)

    if md5_url := config.get(CONF_MD5_URL):
        template_ = await cg.templatable(md5_url, args, cg.std_string)
        cg.add(var.set_md5_url(template_))

    if md5_str := config.get(CONF_MD5):
        template_ = await cg.templatable(md5_str, args, cg.std_string)
        cg.add(var.set_md5(template_))

    template_ = await cg.templatable(config[CONF_URL], args, cg.std_string)
    cg.add(var.set_url(template_))

    return var