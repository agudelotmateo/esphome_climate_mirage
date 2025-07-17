# based on the official daikin's climate_ir component
# https://github.com/esphome/esphome/blob/dev/esphome/components/daikin/climate.py

import esphome.codegen as cg
from esphome.components import climate_ir

AUTO_LOAD = ["climate_ir"]

mirage_ns = cg.esphome_ns.namespace("mirage")
MirageClimate = mirage_ns.class_("MirageClimate", climate_ir.ClimateIR)

CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(MirageClimate)


async def to_code(config):
    await climate_ir.new_climate_ir(config)
