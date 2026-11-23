from math import isfinite

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import esp32, light
from esphome import pins
from esphome.core import CORE
from esphome.const import (
    CONF_COLD_WHITE_COLOR_TEMPERATURE,
    CONF_COLOR_INTERLOCK,
    CONF_CONSTANT_BRIGHTNESS,
    CONF_DEFAULT_TRANSITION_LENGTH,
    CONF_ID,
    CONF_INVERTED,
    CONF_MODE,
    CONF_NAME,
    CONF_NUMBER,
    CONF_NUM_LEDS,
    CONF_OUTPUT,
    CONF_OUTPUT_ID,
    CONF_PIN,
    CONF_RMT_SYMBOLS,
    CONF_SETUP_PRIORITY,
    CONF_USE_DMA,
    CONF_WARM_WHITE_COLOR_TEMPERATURE,
)

try:
    from esphome.components import esp32_rmt
except ImportError:
    esp32_rmt = None

CONF_CHANNEL_ORDER = "channel_order"
CONF_CCT_TRANSITION_SPEED = "cct_transition_speed"
CONF_DIN_PIN = "din_pin"
CONF_FDIN_PIN = "fdin_pin"
CONF_ISR_PRIORITY = "isr_priority"
CONF_POWER_ESTIMATION = "power_estimation"
CONF_RMT_PREENCODE = "rmt_preencode"
CONF_RMT_NO_DMA_BLOCKS = "rmt_no_dma_blocks"
CONF_SPARSE_DIMMING = "sparse_dimming"
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

# Symbols per block, possible TX starts, and total shared memory blocks.
# ESP-IDF lets a TX channel extend into the RX block range.
RMT_MEMORY = {
    "ESP32": (64, 8, 8),
    "ESP32S2": (64, 4, 4),
    "ESP32S3": (48, 4, 8),
    "ESP32S31": (48, 4, 8),
    "ESP32P4": (48, 4, 8),
    "ESP32C3": (48, 2, 4),
    "ESP32C5": (48, 2, 4),
    "ESP32C6": (48, 2, 4),
    "ESP32H2": (48, 2, 4),
    "ESP32H4": (48, 2, 4),
    "ESP32H21": (48, 2, 4),
}
DMA_VARIANTS = {"ESP32S3", "ESP32S31", "ESP32P4"}
RMT_USAGE = "ws2805_rmt_usage"
HARDWARE_SETUP_PRIORITY = 800.0

ws2805_ns = cg.esphome_ns.namespace("ws2805")
WS2805LightOutput = ws2805_ns.class_("WS2805LightOutput", light.AddressableLight)


def normalize_config(config):
    if not isinstance(config, dict):
        return config
    if CONF_CCT_TRANSITION_SPEED in config:
        speed = config.pop(CONF_CCT_TRANSITION_SPEED)
        config.setdefault(CONF_DEFAULT_TRANSITION_LENGTH, speed)
    if CONF_DIN_PIN in config:
        config.pop(CONF_PIN, None)
    return config


def validate_white_temperature(value):
    temperature = cv.color_temperature(value)
    if not isfinite(temperature) or temperature <= 0:
        raise cv.Invalid("White color temperature must be finite and greater than zero")
    return temperature


def validate_output_pin(value):
    if value[CONF_INVERTED]:
        raise cv.Invalid("WS2805 requires a non-inverted data pin", path=[CONF_INVERTED])
    mode = value[CONF_MODE]
    if not mode[CONF_OUTPUT] or any(
        enabled for key, enabled in mode.items() if key != CONF_OUTPUT
    ):
        raise cv.Invalid("WS2805 requires a push-pull output pin", path=[CONF_MODE])
    if value.get("hold_state"):
        raise cv.Invalid("WS2805 does not support GPIO hold_state", path=["hold_state"])
    if value.get("drive_strength", 20.0) != 20.0:
        raise cv.Invalid(
            "WS2805 uses the ESP32 default GPIO drive strength", path=["drive_strength"]
        )
    return value


def data_pin_key(config):
    return CONF_DIN_PIN if CONF_DIN_PIN in config else CONF_PIN


def validate_different_pins(config):
    backup = config.get(CONF_FDIN_PIN)
    if backup is not None and backup[CONF_NUMBER] == config[data_pin_key(config)][CONF_NUMBER]:
        raise cv.Invalid("DIN and FDIN require different GPIOs", path=[CONF_FDIN_PIN])
    return config


def shared_rmt_available():
    return hasattr(esp32_rmt, "request_rmt_channel") and (
        esp32.idf_version() >= cv.Version(5, 0, 0)
    )


def register_rmt_usage(config):
    variant = esp32.get_esp32_variant()
    if variant in ("ESP32C2", "ESP32C61"):
        raise cv.Invalid(f"{variant} has no RMT peripheral for WS2805")
    if variant in ("ESP32H4", "ESP32H21") and esp32.idf_version() < cv.Version(6, 0, 0):
        raise cv.Invalid(f"{variant} requires ESP-IDF 6 or newer for RMT support")

    strips = CORE.data.setdefault(RMT_USAGE, [])
    use_dma = variant in DMA_VARIANTS and not strips
    strips.append((config, use_dma))
    if variant in RMT_MEMORY and shared_rmt_available():
        symbols_per_block, _, _ = RMT_MEMORY[variant]
        blocks = config[CONF_RMT_NO_DMA_BLOCKS]
        request = esp32_rmt.request_rmt_channel("TX", HARDWARE_SETUP_PRIORITY)
        channel = {
            CONF_ID: config[CONF_ID],
            CONF_NAME: config.get(CONF_NAME),
            CONF_SETUP_PRIORITY: config.get(CONF_SETUP_PRIORITY, HARDWARE_SETUP_PRIORITY),
            CONF_USE_DMA: use_dma,
            CONF_RMT_SYMBOLS: (4 if use_dma else blocks) * symbols_per_block,
        }
        request(channel)
        if CONF_FDIN_PIN in config:
            request({
                **channel,
                CONF_NAME: f"{channel[CONF_NAME] or 'WS2805'} FDIN",
                CONF_USE_DMA: False,
                CONF_RMT_SYMBOLS: blocks * symbols_per_block,
            })
    return config


def validate_rmt_usage(config):
    variant = esp32.get_esp32_variant()
    if variant not in RMT_MEMORY:
        return

    _, tx_starts, total_blocks = RMT_MEMORY[variant]
    modern = esp32.idf_version() >= cv.Version(5, 0, 0)
    ordered = sorted(
        CORE.data[RMT_USAGE],
        key=lambda entry: -entry[0].get(CONF_SETUP_PRIORITY, HARDWARE_SETUP_PRIORITY),
    )
    occupied = ~((1 << total_blocks) - 1)
    first_priority = ordered[0][0][CONF_ISR_PRIORITY]
    for strip, use_dma in ordered:
        blocks = strip[CONF_RMT_NO_DMA_BLOCKS] if modern else 1
        priority = strip[CONF_ISR_PRIORITY]
        if modern and priority and priority != first_priority:
            if strip is config:
                raise cv.Invalid(
                    f"All WS2805 outputs on {variant} share the RMT interrupt priority. "
                    f"The first strip uses {first_priority}; match it or use isr_priority: 0.",
                    path=[CONF_ISR_PRIORITY],
                )
            continue

        outputs = [(data_pin_key(strip), use_dma and modern)]
        if CONF_FDIN_PIN in strip:
            outputs.append((CONF_FDIN_PIN, False))
        for pin_key, dma in outputs:
            mask = 1 if dma else (1 << blocks) - 1
            starts = (tx_starts - 1,) if dma else range(tx_starts)
            start = next((slot for slot in starts if not occupied & (mask << slot)), None)
            if start is None:
                if strip is config:
                    mode = "the DMA channel" if dma else f"{blocks} contiguous RMT block(s)"
                    remedy = "Reduce the number of DIN/FDIN outputs"
                    if blocks == 4:
                        remedy += " or use rmt_no_dma_blocks: 2"
                    raise cv.Invalid(
                        f"{variant} cannot allocate {mode} for this WS2805 output. {remedy}.",
                        path=[pin_key],
                    )
                break
            occupied |= mask << start

    if shared_rmt_available():
        esp32_rmt.final_validate_rmt_channels(config)


DATA_PIN_SCHEMA = cv.All(pins.internal_gpio_output_pin_schema, validate_output_pin)
TIMING_VALUE_SCHEMA = cv.int_range(min=1, max=0xFFFFFFFF)
FINAL_VALIDATE_SCHEMA = validate_rmt_usage

CONFIG_SCHEMA = cv.All(
    normalize_config,
    light.ADDRESSABLE_LIGHT_SCHEMA.extend(
        {
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(WS2805LightOutput),
            cv.Optional(CONF_PIN): DATA_PIN_SCHEMA,
            cv.Optional(CONF_DIN_PIN): DATA_PIN_SCHEMA,
            cv.Required(CONF_NUM_LEDS): cv.int_range(min=1, max=65535),
            cv.Optional(CONF_CHANNEL_ORDER, default="GRBWWCW"): cv.one_of(*CHANNEL_ORDERS, upper=True),
            cv.Optional(CONF_FDIN_PIN): DATA_PIN_SCHEMA,
            cv.Optional(CONF_COLOR_INTERLOCK, default=False): cv.boolean,
            cv.Optional(
                CONF_COLD_WHITE_COLOR_TEMPERATURE, default="153 mireds"
            ): validate_white_temperature,
            cv.Optional(
                CONF_WARM_WHITE_COLOR_TEMPERATURE, default="500 mireds"
            ): validate_white_temperature,
            cv.Optional(CONF_CONSTANT_BRIGHTNESS, default=False): cv.boolean,
            cv.Optional(CONF_POWER_ESTIMATION): cv.invalid(
                "WS2805 does not support power_estimation for its two independent white channels"
            ),
            cv.Optional("dithering", default=False): cv.boolean,
            # The RMT resolution is auto-detected per build target.
            cv.Optional("bit0_high_ns", default=300): TIMING_VALUE_SCHEMA,
            cv.Optional("bit0_low_ns", default=975): TIMING_VALUE_SCHEMA,
            cv.Optional("bit1_high_ns", default=800): TIMING_VALUE_SCHEMA,
            cv.Optional("bit1_low_ns", default=800): TIMING_VALUE_SCHEMA,
            cv.Optional("reset_pulse_us", default=300): TIMING_VALUE_SCHEMA,
            cv.Optional(CONF_ISR_PRIORITY, default=3): cv.int_range(min=0, max=3),
            cv.Optional(CONF_RMT_PREENCODE, default=False): cv.boolean,
            cv.Optional(CONF_RMT_NO_DMA_BLOCKS, default=2): cv.one_of(2, 4, int=True),
            cv.Optional(CONF_SPARSE_DIMMING, default=False): cv.boolean,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.has_at_least_one_key(CONF_PIN, CONF_DIN_PIN),
    validate_different_pins,
    light.validate_color_temperature_channels,
    register_rmt_usage,
)

async def to_code(config):
    esp32.include_builtin_idf_component("esp_driver_rmt")
    variant = esp32.get_esp32_variant()
    pin_value = config[data_pin_key(config)]
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID], config[CONF_NUM_LEDS], pin_value[CONF_NUMBER])
    await cg.register_component(var, config)
    if variant in RMT_MEMORY and shared_rmt_available():
        esp32_rmt.register_rmt_channel(config)
    await light.register_light(var, config)

    if variant in DMA_VARIANTS:
        first_strip, _ = CORE.data[RMT_USAGE][0]
        if first_strip[CONF_ID] != config[CONF_ID]:
            cg.add(var.set_use_dma(False))

    cg.add(var.set_color_interlock(config[CONF_COLOR_INTERLOCK]))
    cg.add(var.set_cold_white_temperature(config[CONF_COLD_WHITE_COLOR_TEMPERATURE]))
    cg.add(var.set_warm_white_temperature(config[CONF_WARM_WHITE_COLOR_TEMPERATURE]))
    cg.add(var.set_constant_brightness(config[CONF_CONSTANT_BRIGHTNESS]))

    channel_order = CHANNEL_ORDERS[config[CONF_CHANNEL_ORDER]]
    cg.add(var.set_channel_order(*channel_order))

    if CONF_FDIN_PIN in config:
        cg.add(var.set_fdin_pin(config[CONF_FDIN_PIN][CONF_NUMBER]))

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
    if config[CONF_SPARSE_DIMMING]:
        cg.add(var.set_sparse_dimming(True))
