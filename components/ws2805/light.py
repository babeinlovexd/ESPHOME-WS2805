import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light
from esphome import pins
from esphome.const import CONF_OUTPUT_ID, CONF_PIN, CONF_NUM_LEDS

# Declare the namespace and class
ws2805_ns = cg.esphome_ns.namespace("ws2805")
WS2805LightOutput = ws2805_ns.class_("WS2805LightOutput", light.AddressableLight, cg.Component)

# Include the headers from our external component
cg.add_library("makuna/NeoPixelBus", "2.8.0")

CONFIG_SCHEMA = light.ADDRESSABLE_LIGHT_SCHEMA.extend({
    cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(WS2805LightOutput),
    cv.Required(CONF_PIN): pins.internal_gpio_output_pin_number, # Allow GPIO strings
    cv.Required(CONF_NUM_LEDS): cv.positive_int,
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    # Register the AddressableLight
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID], config[CONF_NUM_LEDS], config[CONF_PIN])
    await cg.register_component(var, config)
    await light.register_light(var, config)
