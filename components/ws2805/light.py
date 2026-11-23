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
    CONF_CONSTANT_BRIGHTNESS,
    CONF_NUMBER
)

CONF_CHANNEL_ORDER = "channel_order"
CONF_DIN_PIN = "din_pin"
CONF_FDIN_PIN = "fdin_pin"
CONF_ISR_PRIORITY = "isr_priority"
CONF_RMT_PREENCODE = "rmt_preencode"
CONF_RMT_NO_DMA_BLOCKS = "rmt_no_dma_blocks"
CHANNEL_ORDERS = {
    "RGBWWCW": (0, 1, 2, 3, 4),
    "RGBCWWW": (0, 1, 2, 4, 3),
    "GRBWWCW": (1, 0, 2, 3, 4),
    "GRBCWWW": (1, 0, 2, 4, 3),
    "BRGWWCW": (1, 2, 0, 3, 4),
    "BRGCWWW": (1, 2, 0, 4, 3),
}

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

    instances = CORE.data.get("ws2805", 0) + 1
    if instances > max_channels:
        raise cv.Invalid(
            f"Too many WS2805 instances ({instances}) for {variant}. "
            f"The hardware supports a maximum of {max_channels} RMT channels."
        )

    validate_expert_rmt_usage(config, variant)
    CORE.data["ws2805"] = instances
    return config

def validate_expert_rmt_usage(config, variant):
    # This lower bound covers known non-DMA targets. The actual RMT driver
    # also accounts for contiguous memory, channel direction and other users.
    if variant == const.VARIANT_ESP32:
        block_limit = 8
    elif variant in (const.VARIANT_ESP32S2, const.VARIANT_ESP32C3,
                     const.VARIANT_ESP32C5, const.VARIANT_ESP32C6, const.VARIANT_ESP32H2):
        block_limit = 4
    else:
        return

    blocks = config.get(CONF_RMT_NO_DMA_BLOCKS, 2)
    has_fdin = bool(config.get(CONF_FDIN_PIN, {}).get(CONF_NUMBER, 0))
    used, expert_enabled = CORE.data.get("ws2805_rmt_blocks", (0, False))
    requested = used + blocks * (2 if has_fdin else 1)
    expert_enabled = expert_enabled or blocks == 4
    # Preserve existing validation for default-only configurations and IDF 4.x.
    if expert_enabled and requested > block_limit and esp32.idf_version() >= cv.Version(5, 0, 0):
        raise cv.Invalid(
            f"WS2805 DIN/FDIN outputs require {requested} RMT blocks, but {variant} has "
            f"{block_limit}. Use rmt_no_dma_blocks: 2 or reduce the number of outputs."
        )
    CORE.data["ws2805_rmt_blocks"] = (requested, expert_enabled)

CONFIG_SCHEMA = cv.All(
    light.ADDRESSABLE_LIGHT_SCHEMA.extend({
    cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(WS2805LightOutput),
    cv.Required(CONF_PIN): pins.internal_gpio_output_pin_schema,
    cv.Optional(CONF_DIN_PIN): pins.internal_gpio_output_pin_schema,
    cv.Required(CONF_NUM_LEDS): cv.positive_int,
    cv.Optional(CONF_CHANNEL_ORDER, default="GRBWWCW"): cv.one_of(*CHANNEL_ORDERS, upper=True),
    cv.Optional(CONF_FDIN_PIN): pins.internal_gpio_output_pin_schema,
    cv.Optional(CONF_COLOR_INTERLOCK, default=False): cv.boolean,
    cv.Optional(CONF_COLD_WHITE_COLOR_TEMPERATURE, default="153 mireds"): cv.color_temperature,
    cv.Optional(CONF_WARM_WHITE_COLOR_TEMPERATURE, default="500 mireds"): cv.color_temperature,
    cv.Optional(CONF_CONSTANT_BRIGHTNESS, default=False): cv.boolean,
    cv.Optional("cct_transition_speed", default="3s"): cv.positive_time_period_milliseconds,
    cv.Optional("dithering", default=False): cv.boolean,
    # Advanced RMT bit timing — defaults center in the WS2805 datasheet ranges.
    # The RMT resolution is auto-detected per build target, so no clock divider
    # is exposed here.
    cv.Optional("bit0_high_ns", default=300): cv.positive_int,
    cv.Optional("bit0_low_ns", default=800): cv.positive_int,
    cv.Optional("bit1_high_ns", default=800): cv.positive_int,
    cv.Optional("bit1_low_ns", default=800): cv.positive_int,
    cv.Optional("reset_pulse_us", default=300): cv.positive_int,
    cv.Optional(CONF_ISR_PRIORITY, default=3): cv.int_range(min=1, max=3),
    cv.Optional(CONF_RMT_PREENCODE, default=False): cv.boolean,
    cv.Optional(CONF_RMT_NO_DMA_BLOCKS, default=2): cv.one_of(2, 4, int=True),
}).extend(cv.COMPONENT_SCHEMA),
    validate_rmt_usage
)

async def to_code(config):
    esp32.include_builtin_idf_component("esp_driver_rmt")
    pin_value = config[CONF_DIN_PIN] if CONF_DIN_PIN in config else config[CONF_PIN]
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID], config[CONF_NUM_LEDS], pin_value[CONF_NUMBER])
    await cg.register_component(var, config)
    await light.register_light(var, config)

    cg.add(var.set_color_interlock(config[CONF_COLOR_INTERLOCK]))
    cg.add(var.set_cold_white_temperature(config[CONF_COLD_WHITE_COLOR_TEMPERATURE]))
    cg.add(var.set_warm_white_temperature(config[CONF_WARM_WHITE_COLOR_TEMPERATURE]))
    cg.add(var.set_constant_brightness(config[CONF_CONSTANT_BRIGHTNESS]))

    channel_order = CHANNEL_ORDERS[config[CONF_CHANNEL_ORDER]]
    cg.add(var.set_channel_order(*channel_order))

    if CONF_FDIN_PIN in config:
        cg.add(var.set_fdin_pin(config[CONF_FDIN_PIN][CONF_NUMBER]))

    if "cct_transition_speed" in config:
        cg.add(var.set_transition_speed(config["cct_transition_speed"]))
    if "dithering" in config:
        cg.add(var.set_dithering(config["dithering"]))
    if "bit0_high_ns" in config:
        cg.add(var.set_bit0_high_ns(config["bit0_high_ns"]))
    if "bit0_low_ns" in config:
        cg.add(var.set_bit0_low_ns(config["bit0_low_ns"]))
    if "bit1_high_ns" in config:
        cg.add(var.set_bit1_high_ns(config["bit1_high_ns"]))
    if "bit1_low_ns" in config:
        cg.add(var.set_bit1_low_ns(config["bit1_low_ns"]))
    if "reset_pulse_us" in config:
        cg.add(var.set_reset_pulse_us(config["reset_pulse_us"]))

    if CONF_ISR_PRIORITY in config:
        cg.add(var.set_isr_priority(config[CONF_ISR_PRIORITY]))
    if config[CONF_RMT_PREENCODE]:
        cg.add(var.set_rmt_preencode(True))
    if config[CONF_RMT_NO_DMA_BLOCKS] == 4:
        cg.add(var.set_rmt_no_dma_blocks(4))
