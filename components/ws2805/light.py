import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light, esp32
from esphome import pins
from esphome.const import (
    CONF_OUTPUT_ID,
    CONF_PIN,
    CONF_NUM_LEDS,
    CONF_COLOR_INTERLOCK,
    CONF_COLD_WHITE_COLOR_TEMPERATURE,
    CONF_WARM_WHITE_COLOR_TEMPERATURE,
    CONF_NUMBER
)

CODEOWNERS = ["@BabeinlovexD"]
DEPENDENCIES = ["esp32"]

# Declare the namespace and class
ws2805_ns = cg.esphome_ns.namespace("ws2805")
WS2805LightOutput = ws2805_ns.class_("WS2805LightOutput", light.AddressableLight)

CONFIG_SCHEMA = light.ADDRESSABLE_LIGHT_SCHEMA.extend({
    cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(WS2805LightOutput),
    cv.Required(CONF_PIN): pins.internal_gpio_output_pin_schema,
    cv.Required(CONF_NUM_LEDS): cv.positive_int,
    cv.Optional(CONF_COLOR_INTERLOCK, default=False): cv.boolean,
    cv.Optional(CONF_COLD_WHITE_COLOR_TEMPERATURE, default="153 mireds"): cv.color_temperature,
    cv.Optional(CONF_WARM_WHITE_COLOR_TEMPERATURE, default="500 mireds"): cv.color_temperature,
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    # Register the AddressableLight
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID], config[CONF_NUM_LEDS], config[CONF_PIN][CONF_NUMBER])
    await cg.register_component(var, config)
    await light.register_light(var, config)

    # Set CCT parameters
    cg.add(var.set_color_interlock(config[CONF_COLOR_INTERLOCK]))
    cg.add(var.set_cold_white_temperature(config[CONF_COLD_WHITE_COLOR_TEMPERATURE]))
    cg.add(var.set_warm_white_temperature(config[CONF_WARM_WHITE_COLOR_TEMPERATURE]))
