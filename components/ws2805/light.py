import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light, esp32
from esphome.components.esp32 import const
from esphome import pins
from esphome.core import CORE
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

ws2805_ns = cg.esphome_ns.namespace("ws2805")
WS2805LightOutput = ws2805_ns.class_("WS2805LightOutput", light.AddressableLight)

def validate_rmt_usage(config):
    variant = esp32.get_esp32_variant()
    if variant == const.VARIANT_ESP32:
        max_channels = 8
    elif variant in (const.VARIANT_ESP32S2, const.VARIANT_ESP32S3):
        max_channels = 4
    elif variant in (const.VARIANT_ESP32C3, const.VARIANT_ESP32C6):
        max_channels = 2
    else:
        max_channels = 8

    if "ws2805" not in CORE.data:
        CORE.data["ws2805"] = 0
    CORE.data["ws2805"] += 1

    if CORE.data["ws2805"] > max_channels:
        raise cv.Invalid(
            f"Too many WS2805 instances ({CORE.data['ws2805']}) for {variant}. "
            f"The hardware supports a maximum of {max_channels} RMT channels."
        )

    return config

CONFIG_SCHEMA = cv.All(
    light.ADDRESSABLE_LIGHT_SCHEMA.extend({
    cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(WS2805LightOutput),
    cv.Required(CONF_PIN): pins.internal_gpio_output_pin_schema,
    cv.Required(CONF_NUM_LEDS): cv.positive_int,
    cv.Optional(CONF_COLOR_INTERLOCK, default=False): cv.boolean,
    cv.Optional(CONF_COLD_WHITE_COLOR_TEMPERATURE, default="153 mireds"): cv.color_temperature,
    cv.Optional(CONF_WARM_WHITE_COLOR_TEMPERATURE, default="500 mireds"): cv.color_temperature,
    cv.Optional("max_refresh_rate", default="4ms"): cv.positive_time_period_microseconds,
}).extend(cv.COMPONENT_SCHEMA),
    validate_rmt_usage
)

async def to_code(config):
    esp32.include_builtin_idf_component("esp_driver_rmt")
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID], config[CONF_NUM_LEDS], config[CONF_PIN][CONF_NUMBER])
    await cg.register_component(var, config)
    await light.register_light(var, config)

    cg.add(var.set_color_interlock(config[CONF_COLOR_INTERLOCK]))
    cg.add(var.set_cold_white_temperature(config[CONF_COLD_WHITE_COLOR_TEMPERATURE]))
    cg.add(var.set_warm_white_temperature(config[CONF_WARM_WHITE_COLOR_TEMPERATURE]))
    if "max_refresh_rate" in config:
        cg.add(var.set_max_refresh_rate(config["max_refresh_rate"]))
