import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

pitop_matrix_ns = cg.global_ns
PiTopMatrix = pitop_matrix_ns.class_("PiTopMatrix", cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(PiTopMatrix),
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    cg.add_global(cg.RawStatement('#include "pitop_matrix.h"'))
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
