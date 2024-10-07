import esphome.codegen as cg
import esphome.config_validation as cv

from esphome.const import (
    CONF_ID,
 )


CODEOWNERS = ["@gnumpi"]

testing_ns = cg.esphome_ns.namespace("runtime_testing")
TaskMonitor = testing_ns.class_("TaskMonitor", cg.Component )



CONFIG_SCHEMA = (
    cv.Schema({
        cv.GenerateID(): cv.declare_id(TaskMonitor),
    })
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    return var






