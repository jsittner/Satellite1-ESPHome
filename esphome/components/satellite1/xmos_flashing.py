import hashlib
import logging
from pathlib import Path
import re

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation, external_files
from esphome.const import (
    CONF_FILE,
    CONF_ID,
    CONF_PATH,
    CONF_RAW_DATA_ID,
    CONF_TRIGGER_ID,
    CONF_TYPE,
    CONF_URL,
)

from esphome.components.http_request import (
    CONF_HTTP_REQUEST_ID,
    HttpRequestComponent,
)


from esphome.core import CORE, HexInt
from esphome.external_files import download_content

from . import satellite1 as sat


_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@gnumpi"]

AUTO_LOAD = ["md5"]
DEPENDENCIES = ["network", "http_request", "satellite1"]
DOMAIN = "file"

TYPE_LOCAL = "local"
TYPE_WEB = "web"
TYPE_FILE = "file"
TYPE_STRING = "string"

CONF_MD5 = "md5"
CONF_MD5_FILE = "md5_file"
CONF_MD5_URL = "md5_url"
CONF_IMAGE_FILE = "image_file"

CONF_XMOS_FLASHER_ID = "xmos_flasher_id"
CONF_EMBED_XMOS_IMAGE = "embed_xmos_image"
CONF_XMOS_FIRMWARE = "xmos_firmware"

CONF_ON_PROGRESS_UPDATE = "on_progress_update"
CONF_ON_FLASHING_SUCCESS = "on_flashing_success"
CONF_ON_FLASHING_FAILED = "on_flashing_failed"


FlashImage = sat.namespace.struct("FlashImage")
XMOSFlasher = sat.namespace.class_("XMOSFlasher", sat.Satellite1SPIService, cg.Component )
XMOSFlashAction = sat.namespace.class_(
    "XMOSFlashAction", automation.Action
)

XMOSFlashEmbeddedAction = sat.namespace.class_(
    "XMOSFlashEmbeddedAction", automation.Action
)

FlashProgressUpdateTrigger = sat.namespace.class_(
    "XMOSFlashingProgressUpdateTrigger", automation.Trigger
)
FlashProgressSuccessTrigger = sat.namespace.class_(
    "XMOSFlasherSuccessTrigger", automation.Trigger
)
FlashProgressFailedTrigger = sat.namespace.class_(
    "XMOSFlasherFailedTrigger", automation.Trigger
)

def _generate_local_file_path(value: dict) -> Path:
    """
    Generates a local file path for a downloadable file by hashing the provided URL.
    """
    url = value[CONF_URL]
    h = hashlib.new("sha256")
    h.update(url.encode())
    key = h.hexdigest()[:8]
    base_dir = external_files.compute_local_file_dir(DOMAIN)
    return base_dir / key


def _resolve_local_file(file_conf:dict) -> Path:
    """
    Checks if a local file exists, and if not, downloads it from a provided URL. 
    Returning the local file path if successful.

    """
    if file_conf[CONF_TYPE] == TYPE_LOCAL:
        local_file = Path(CORE.relative_config_path(file_conf[CONF_PATH]))
        if not local_file.exists(local_file):
            raise cv.Invalid( f"Can't locate {local_file}.")
        return local_file
    elif file_conf[CONF_TYPE] == TYPE_WEB:
        local_file = _generate_local_file_path(file_conf)
        if not local_file.exists():
            download_content( file_conf[CONF_URL], local_file )
            if not local_file.exists():
                raise cv.Invalid( f"Couldn't download {file_conf[CONF_URL]} to {local_file}.")
        return local_file
    raise cv.Invalid( f"Couldn't get file for: {file_conf}" )


def _validate_md5_str(value:str) -> str:
    """
    Validates whether the given string is a valid MD5 checksum.
    """
    if re.match(r"^[a-fA-F0-9]{32}$", value) is None:
        raise cv.Invalid(f"Invalid md5 string: {value}")
    return value


def _get_expected_md5(flash_config:dict) -> str:
    """
    Returns the expected MD5 checksum from the configuration, which can either be 
    provided directly as a string or as a file path containing the checksum.
    """
    if CONF_MD5 in flash_config:
        return flash_config[CONF_MD5]
    md5_key = CONF_MD5_FILE if CONF_MD5_FILE in flash_config else CONF_MD5_URL
    local_file_path = _resolve_local_file(flash_config[md5_key])
    with open(local_file_path, "r" ) as f:
        md5_str = f.readline().strip()
    return _validate_md5_str(md5_str)


def _check_image_md5_sum(image_file:Path, expected_md5:str) -> bool:
    """
    Calculates the MD5 checksum of the given file and compares it with the provided value.
    """
    with open( image_file, "rb" ) as f:
        file_md5 = hashlib.md5( f.read() ).hexdigest()
    return file_md5 == expected_md5



LOCAL_FILE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_PATH): cv.file_,
    }
)

WEB_FILE_SCHEMA = cv.All(
    {
        cv.Required(CONF_URL): cv.url,
    },
)

TYPED_FILE_SCHEMA = cv.typed_schema(
    {
        TYPE_LOCAL: LOCAL_FILE_SCHEMA,
        TYPE_WEB: WEB_FILE_SCHEMA,
    },
)


def _validate_file_shorthand(value):
    value = cv.string_strict(value)
    if value.startswith("http://") or value.startswith("https://"):
        return _file_schema(
            {
                CONF_TYPE: TYPE_WEB,
                CONF_URL: value,
            }
        )
    return _file_schema(
        {
            CONF_TYPE: TYPE_LOCAL,
            CONF_PATH: value,
        }
    )

def _file_schema(value):
    if isinstance(value, str):
        return _validate_file_shorthand(value)
    return TYPED_FILE_SCHEMA(value)


def _validate_embed_xmos_image_schema(image_config:dict) -> dict:
    md5_sum = _get_expected_md5(image_config)
    img_local_path = _resolve_local_file(image_config[CONF_IMAGE_FILE])
    if not img_local_path.match("*factory*"):
        _LOGGER.warning(f"Image filename {img_local_path} doesn't contain 'factory' string, make sure you are point to the right file.")
    if not _check_image_md5_sum(img_local_path, md5_sum):
        raise cv.Invalid( f"XMOS-Image: md5 sum does not match.")
    
    return image_config


XMOS_IMAGE_SCHEMA = cv.Schema(
        {
            cv.Required(CONF_XMOS_FIRMWARE): cv.version_number,
            cv.Required(CONF_IMAGE_FILE): cv.templatable(cv.url),
            cv.Optional(CONF_MD5_URL): cv.templatable(cv.url),
            cv.Optional(CONF_MD5): cv.templatable(_validate_md5_str),
        }
)
#  cv.has_exactly_one_key(CONF_MD5, CONF_MD5_URL),



EMBED_XMOS_IMAGE_SCHEMA = cv.All(
    XMOS_IMAGE_SCHEMA.extend(
        {
            cv.Required(CONF_IMAGE_FILE): _file_schema,
            cv.Optional(CONF_MD5_FILE): _file_schema,
            cv.GenerateID(CONF_RAW_DATA_ID): cv.declare_id(cg.uint8),
        }
    ),
    cv.has_exactly_one_key(CONF_MD5, CONF_MD5_FILE, CONF_MD5_URL),
    _validate_embed_xmos_image_schema
)


FLASHER_CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(XMOSFlasher),
            cv.GenerateID(CONF_HTTP_REQUEST_ID): cv.use_id(HttpRequestComponent),
            cv.Optional(CONF_EMBED_XMOS_IMAGE): EMBED_XMOS_IMAGE_SCHEMA,
            
            cv.Optional(CONF_ON_PROGRESS_UPDATE): automation.validate_automation({
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(FlashProgressUpdateTrigger),
            }),
            cv.Optional(CONF_ON_FLASHING_SUCCESS): automation.validate_automation({
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(FlashProgressSuccessTrigger),
            }),
            cv.Optional(CONF_ON_FLASHING_FAILED): automation.validate_automation({
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(FlashProgressFailedTrigger),
            }),
        }
    )
    .extend(cv.COMPONENT_SCHEMA),
)



def _prepare_flash_image_pgm(flash_config: dict) -> tuple[str, tuple[str], int]:
    local_flash_image = _resolve_local_file(flash_config[CONF_IMAGE_FILE])    
    md5_sum = _get_expected_md5(flash_config)
    with open( local_flash_image, "rb" ) as f:
        hex_strings = tuple(map(HexInt, f.read()))
    return md5_sum, hex_strings, len(hex_strings)


async def register_xmos_flasher(satellite1_id, flasher_config):
    var = cg.new_Pvariable(flasher_config[CONF_ID])
    await cg.register_parented(var, satellite1_id)
    http_comp = await cg.get_variable(flasher_config[CONF_HTTP_REQUEST_ID])
    cg.add( var.set_http_request_component(http_comp))

    if image_conf := flasher_config.get(CONF_EMBED_XMOS_IMAGE):
        expected_md5_sum, hex_strings, num_of_bytes = _prepare_flash_image_pgm(image_conf)
        prog_arr = cg.progmem_array(image_conf[CONF_RAW_DATA_ID], hex_strings)
        cg.add( var.set_embedded_image(prog_arr, num_of_bytes, expected_md5_sum) )

    for conf in flasher_config.get(CONF_ON_PROGRESS_UPDATE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var )
        await automation.build_automation(trigger, [], conf)
    
    for conf in flasher_config.get(CONF_ON_FLASHING_SUCCESS, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var )
        await automation.build_automation(trigger, [], conf)

    for conf in flasher_config.get(CONF_ON_FLASHING_FAILED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var )
        await automation.build_automation(trigger, [], conf)


XMOS_FLASH_ACTION_SCHEMA = cv.All(
    XMOS_IMAGE_SCHEMA.extend(
        {
            cv.GenerateID(CONF_XMOS_FLASHER_ID): cv.use_id(XMOSFlasher)    
        }
    )
)

XMOS_FlASH_EMBEDDED_ACTION_SCHEMA = automation.maybe_simple_id(
    {
        cv.GenerateID(CONF_XMOS_FLASHER_ID): cv.use_id(XMOSFlasher)
    }
)


@automation.register_action(
    "satellite1.flash_xmos",
    XMOSFlashAction,
    XMOS_FLASH_ACTION_SCHEMA,
)
async def flash_xmos_action_to_code(config, action_id, template_arg, args):
    flasher = await cg.get_variable(config[CONF_XMOS_FLASHER_ID])
    var = cg.new_Pvariable(action_id, template_arg, flasher)

    if md5_url := config.get(CONF_MD5_URL):
        template_ = await cg.templatable(md5_url, args, cg.std_string)
        cg.add(var.set_md5_url(template_))

    if md5_str := config.get(CONF_MD5):
        template_ = await cg.templatable(md5_str, args, cg.std_string)
        cg.add(var.set_md5(template_))

    template_ = await cg.templatable(config[CONF_IMAGE_FILE], args, cg.std_string)
    cg.add(var.set_url(template_))

    return var


@automation.register_action(
    "satellite1.flash_xmos_with_embedded_firmware", 
    XMOSFlashEmbeddedAction, 
    XMOS_FlASH_EMBEDDED_ACTION_SCHEMA)
async def flash_xmos_embedded_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_XMOS_FLASHER_ID])
    return var
