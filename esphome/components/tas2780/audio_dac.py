import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome import automation
from esphome.components.audio_dac import AudioDac, audio_dac_ns
from esphome.const import CONF_ID, CONF_MODE

CODEOWNERS = ["@gnumpi"]
DEPENDENCIES = ["i2c"]

tas2780_ns = cg.esphome_ns.namespace("tas2780")
tas2780 = tas2780_ns.class_("TAS2780", AudioDac, cg.Component, i2c.I2CDevice)


RestAction = tas2780_ns.class_(
    "ResetAction", automation.Action, cg.Parented.template(tas2780)
)

ActivateAction = tas2780_ns.class_(
    "ActivateAction", automation.Action, cg.Parented.template(tas2780)
)

DeactivateAction = tas2780_ns.class_(
    "DeactivateAction", automation.Action, cg.Parented.template(tas2780)
)



CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(tas2780),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x18))
)


TAS2780_ACTION_SCHEMA = automation.maybe_simple_id({cv.GenerateID(): cv.use_id(tas2780)})

@automation.register_action("tas2780.deactivate", DeactivateAction, TAS2780_ACTION_SCHEMA)
@automation.register_action("tas2780.activate", ActivateAction, TAS2780_ACTION_SCHEMA)
@automation.register_action("tas2780.reset", RestAction, TAS2780_ACTION_SCHEMA)
async def tas2780_action(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)