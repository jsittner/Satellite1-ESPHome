import esphome.codegen as cg
import esphome.config_validation as cv

from esphome.const import CONF_ID

from . import satellite1 as sat
from . import sat_gpio
from . import xmos_flashing as xflash


CODEOWNERS = ["@gnumpi"]

AUTO_LOAD = ["md5"]
DEPENDENCIES = ["spi", "network", "http_request"]

CONF_XMOS_FLASHER = "xmos_flasher"


CONFIG_SCHEMA = sat.SAT1_CONFIG_SCHEMA.extend(
    cv.Schema(
        {
          cv.Optional(CONF_XMOS_FLASHER, default={}) : xflash.FLASHER_CONFIG_SCHEMA
        }
    )
)
    




async def to_code(config):
    sat1_var = await sat.register_satellite1(config)
    await xflash.register_xmos_flasher(config[CONF_ID], config[CONF_XMOS_FLASHER])
    return sat1_var
